/*
 * elf_loader.c — ELF32 static executable loader
 *
 * How loading works
 * -----------------
 * 1. Read the entire file from BobFS into a kernel buffer.
 * 2. Validate the ELF header (magic, class=32-bit, machine=x86, type=EXEC).
 * 3. Walk the program header table.  For each PT_LOAD segment:
 *    a. Round vaddr down to the nearest page boundary → map_start.
 *    b. Round (vaddr + memsz) up to a page boundary → map_end.
 *    c. For each page in [map_start, map_end):
 *       - Allocate a fresh physical page from the PMM.
 *       - Install it in the new PD at the correct virtual address.
 *       - Mark it PAGE_USER.
 *    d. Copy p_filesz bytes from the file buffer into the correct virtual offset.
 *    e. Zero the (p_memsz - p_filesz) BSS tail.
 * 4. Return the ELF entry point and the new CR3 to the caller.
 *
 * Addressing note
 * ---------------
 * After paging_init() the kernel lives in a 1:1 identity map (virt == phys for
 * 0-4MB).  Freshly allocated PMM pages are also in this range, so writing to
 * their physical address directly (without any extra mapping) is safe.
 *
 * The user ELF is linked to load at USER_BASE (0x400000).  This is above the
 * kernel's 4MB identity map, so user segments never overlap kernel pages.
 */

#include <stdint.h>
#include "elf.h"
#include "elf_loader.h"
#include "fs.h"
#include "pmm.h"
#include "paging.h"
#include "vga.h"

/* User programs are linked to load at 4 MB — safely above the kernel. */
#define USER_BASE   0x400000u

/* Align address down to 4 KB page boundary. */
#define PAGE_ALIGN_DOWN(a)  ((a) & ~0xFFFu)
/* Align address up to 4 KB page boundary. */
#define PAGE_ALIGN_UP(a)    (((a) + 0xFFFu) & ~0xFFFu)

/*
 * install_user_page — allocate a physical page and map it at virt in cr3.
 *
 * Extends paging_new_address_space()'s simple first-4MB model: the user
 * program lives above 4MB (at USER_BASE), so we need to add a second PDE
 * and PT for that region.
 *
 * Returns the physical address of the new page, or 0 on OOM.
 */
static uint32_t install_user_page(uint32_t cr3, uint32_t virt) {
    uint32_t *pd  = (uint32_t *)cr3;
    uint32_t  pdi = virt >> 22;
    uint32_t  pti = (virt >> 12) & 0x3FFu;
    uint32_t *pt;
    uint32_t  frame;

    /* Allocate a page table for this PDE if one doesn't exist yet. */
    if (!(pd[pdi] & PAGE_PRESENT)) {
        uint32_t new_pt = pmm_alloc_page();
        int i;
        if (!new_pt) return 0;
        for (i = 0; i < 1024; i++) ((uint32_t *)new_pt)[i] = 0;
        pd[pdi] = new_pt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    } else {
        /* Ensure PDE has PAGE_USER so ring-3 can walk into this PT. */
        pd[pdi] |= PAGE_USER;
    }

    pt = (uint32_t *)(pd[pdi] & ~0xFFFu);

    /* Allocate and zero the physical frame. */
    frame = pmm_alloc_page();
    if (!frame) return 0;
    {
        int i;
        uint32_t *p = (uint32_t *)frame;
        for (i = 0; i < 1024; i++) p[i] = 0;
    }

    pt[pti] = frame | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return frame;
}

int elf_load(const char *filename, uint32_t *out_entry, uint32_t *out_cr3) {
    static uint8_t file_buf[FS_MAX_FILE_SIZE];
    int            file_size;
    Elf32_Ehdr    *ehdr;
    Elf32_Phdr    *phdr;
    uint32_t       cr3;
    int            i;

    /* ---- 1. Read from BobFS ---- */
    file_size = fs_read(filename, (char *)file_buf);
    if (file_size < 0) return -1;

    /* ---- 2. Validate ELF header ---- */
    ehdr = (Elf32_Ehdr *)file_buf;
    if (ehdr->e_ident[0] != ELF_MAGIC0 ||
        ehdr->e_ident[1] != ELF_MAGIC1 ||
        ehdr->e_ident[2] != ELF_MAGIC2 ||
        ehdr->e_ident[3] != ELF_MAGIC3)  return -2;
    if (ehdr->e_ident[4] != 1)           return -2; /* not ELF32 */
    if (ehdr->e_type    != ET_EXEC)      return -2;
    if (ehdr->e_machine != EM_386)       return -2;
    if (ehdr->e_phnum   == 0)            return -2;

    /* ---- 3. Fresh address space (kernel 0-4MB copied in, supervisor-only) ---- */
    cr3 = paging_new_address_space();
    if (!cr3) return -3;

    /* ---- 4. Map and load each PT_LOAD segment ---- */
    for (i = 0; i < (int)ehdr->e_phnum; i++) {
        uint32_t map_start, map_end, page;
        uint8_t *dst;
        uint8_t *src;

        phdr = (Elf32_Phdr *)(file_buf + ehdr->e_phoff +
                               (uint32_t)i * ehdr->e_phentsize);
        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_memsz == 0)      continue;

        map_start = PAGE_ALIGN_DOWN(phdr->p_vaddr);
        map_end   = PAGE_ALIGN_UP(phdr->p_vaddr + phdr->p_memsz);

        /* Allocate and map physical pages for this segment. */
        for (page = map_start; page < map_end; page += 0x1000u) {
            uint32_t frame = install_user_page(cr3, page);
            if (!frame) return -3;
        }

        /*
         * Copy the file data into the correct virtual pages.
         * Because the kernel's paging is identity-mapped and the pages we
         * just allocated are in the same identity-mapped region, we can
         * compute the physical address of any virtual address in the new PD
         * by walking the page tables directly.
         */
        {
            uint32_t bytes_left = phdr->p_filesz;
            uint32_t vaddr      = phdr->p_vaddr;
            uint32_t foff       = phdr->p_offset;
            uint32_t *pd_ptr    = (uint32_t *)cr3;

            while (bytes_left > 0) {
                uint32_t pdi2    = vaddr >> 22;
                uint32_t pti2    = (vaddr >> 12) & 0x3FFu;
                uint32_t pg_off  = vaddr & 0xFFFu;
                uint32_t *pt2    = (uint32_t *)(pd_ptr[pdi2] & ~0xFFFu);
                uint32_t  phys   = (pt2[pti2] & ~0xFFFu) + pg_off;
                uint32_t  chunk  = 0x1000u - pg_off;
                if (chunk > bytes_left) chunk = bytes_left;
                dst = (uint8_t *)phys;
                src = file_buf + foff;
                {
                    uint32_t k;
                    for (k = 0; k < chunk; k++) dst[k] = src[k];
                }
                vaddr      += chunk;
                foff       += chunk;
                bytes_left -= chunk;
            }
        }

        /* Zero the BSS tail (p_memsz > p_filesz region). */
        if (phdr->p_memsz > phdr->p_filesz) {
            uint32_t bss_start = phdr->p_vaddr + phdr->p_filesz;
            uint32_t bss_end   = phdr->p_vaddr + phdr->p_memsz;
            uint32_t vaddr2    = bss_start;
            uint32_t *pd_ptr   = (uint32_t *)cr3;

            while (vaddr2 < bss_end) {
                uint32_t pdi2   = vaddr2 >> 22;
                uint32_t pti2   = (vaddr2 >> 12) & 0x3FFu;
                uint32_t pg_off = vaddr2 & 0xFFFu;
                uint32_t *pt2   = (uint32_t *)(pd_ptr[pdi2] & ~0xFFFu);
                uint32_t  phys  = (pt2[pti2] & ~0xFFFu) + pg_off;
                uint32_t  chunk = 0x1000u - pg_off;
                if (vaddr2 + chunk > bss_end) chunk = bss_end - vaddr2;
                {
                    uint32_t k;
                    uint8_t *z = (uint8_t *)phys;
                    for (k = 0; k < chunk; k++) z[k] = 0;
                }
                vaddr2 += chunk;
            }
        }
    }

    *out_entry = ehdr->e_entry;
    *out_cr3   = cr3;
    return 0;
}
