/*
 * paging.c — Virtual Memory / Paging implementation
 *
 * Module 17 additions
 * ───────────────────
 * paging_clone_address_space(src_cr3)
 *   Used by fork().  Creates a fresh PD that is a deep copy of the source.
 *   Kernel pages (PDE[0]) share the same physical frames — only a new PT
 *   struct is allocated and the kernel's PTEs are copied in (supervisor-only).
 *   User pages (PDE[1+]) get brand-new physical frames whose content is
 *   memcpy'd from the source frames.  This is the "copy" in copy-on-write's
 *   predecessor — Module 21 will add the lazy CoW optimisation.
 *
 * paging_free_address_space(cr3)
 *   Called when a process exits to reclaim all physical pages it owns.
 *   Kernel frames are deliberately excluded — they belong to the kernel PMM
 *   allocation and must never be freed by a user process cleanup path.
 */

#include "paging.h"
#include "pmm.h"

static uint32_t pd_phys  = 0;
static uint32_t pt0_phys = 0;

static void zero_page(uint32_t *p) {
    int i;
    for (i = 0; i < 1024; i++) p[i] = 0;
}

static void copy_4k(uint32_t *dst, uint32_t *src) {
    int i;
    for (i = 0; i < 1024; i++) dst[i] = src[i];
}

void paging_init(void) {
    int i;
    uint32_t cr0;
    uint32_t *pd = (uint32_t *)pmm_alloc_page();
    uint32_t *pt = (uint32_t *)pmm_alloc_page();

    pd_phys  = (uint32_t)pd;
    pt0_phys = (uint32_t)pt;

    zero_page(pd);
    zero_page(pt);

    for (i = 0; i < 1024; i++)
        pt[i] = (uint32_t)(i * 0x1000u) | PAGE_PRESENT | PAGE_WRITABLE;

    pd[0] = (uint32_t)pt | PAGE_PRESENT | PAGE_WRITABLE;

    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

uint32_t paging_new_address_space(void) {
    int i;
    uint32_t *new_pd  = (uint32_t *)pmm_alloc_page();
    uint32_t *new_pt  = (uint32_t *)pmm_alloc_page();
    uint32_t *kern_pt = (uint32_t *)pt0_phys;

    zero_page(new_pd);
    zero_page(new_pt);

    for (i = 0; i < 1024; i++)
        new_pt[i] = kern_pt[i] & ~PAGE_USER;

    new_pd[0] = (uint32_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE;
    return (uint32_t)new_pd;
}

void paging_set_user_page(uint32_t cr3, uint32_t virt_addr) {
    uint32_t *pd  = (uint32_t *)cr3;
    uint32_t  pdi = virt_addr >> 22;
    uint32_t  pti = (virt_addr >> 12) & 0x3FFu;
    uint32_t *pt;

    if (!(pd[pdi] & PAGE_PRESENT)) return;
    pd[pdi] |= PAGE_USER;
    pt = (uint32_t *)(pd[pdi] & ~0xFFFu);
    if (!(pt[pti] & PAGE_PRESENT)) return;
    pt[pti] |= PAGE_USER;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

void paging_switch(uint32_t cr3) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

uint32_t paging_kernel_cr3(void)         { return pd_phys;  }
uint32_t paging_get_pd_phys(void)        { return pd_phys;  }
uint32_t paging_get_pt_phys(uint32_t pdi){ return pdi == 0 ? pt0_phys : 0; }

/* ── paging_clone_address_space ──────────────────────────────────────── */

uint32_t paging_clone_address_space(uint32_t src_cr3) {
    uint32_t *src_pd = (uint32_t *)src_cr3;
    uint32_t *new_pd = (uint32_t *)pmm_alloc_page();
    int pdi, pti;

    if (!new_pd) return 0;
    zero_page(new_pd);

    for (pdi = 0; pdi < 1024; pdi++) {
        if (!(src_pd[pdi] & PAGE_PRESENT)) continue;

        if (pdi == 0) {
            /*
             * Kernel region (0-4MB): allocate a new PT struct but copy
             * the same PTEs (pointing at the same physical frames).
             * Strip PAGE_USER so the kernel region stays supervisor-only
             * in the child's PD, just as in paging_new_address_space().
             */
            uint32_t *src_pt = (uint32_t *)(src_pd[pdi] & ~0xFFFu);
            uint32_t *new_pt = (uint32_t *)pmm_alloc_page();
            if (!new_pt) return 0;
            for (pti = 0; pti < 1024; pti++)
                new_pt[pti] = src_pt[pti] & ~PAGE_USER;
            new_pd[0] = (uint32_t)new_pt | PAGE_PRESENT | PAGE_WRITABLE;
        } else {
            /*
             * User region: deep-copy every present page frame.
             * Each present PTE gets a brand-new physical frame whose
             * contents are copied from the source frame.
             */
            uint32_t *src_pt = (uint32_t *)(src_pd[pdi] & ~0xFFFu);
            uint32_t *new_pt = (uint32_t *)pmm_alloc_page();
            uint32_t  pde_flags = src_pd[pdi] & 0xFFFu;
            if (!new_pt) return 0;
            zero_page(new_pt);

            for (pti = 0; pti < 1024; pti++) {
                if (!(src_pt[pti] & PAGE_PRESENT)) continue;

                uint32_t src_frame = src_pt[pti] & ~0xFFFu;
                uint32_t pte_flags = src_pt[pti] & 0xFFFu;

                /* CoW: share frame, clear write bit in both parent and child */
                pte_flags &= ~PAGE_WRITABLE;
                src_pt[pti] &= ~PAGE_WRITABLE;
                pmm_incref(src_frame);
                new_pt[pti] = src_frame | pte_flags;
            }

            new_pd[pdi] = (uint32_t)new_pt | pde_flags;
        }
    }

    return (uint32_t)new_pd;
}

/* ── paging_free_address_space ───────────────────────────────────────── */

void paging_free_address_space(uint32_t cr3) {
    uint32_t *pd = (uint32_t *)cr3;
    int pdi, pti;

    for (pdi = 0; pdi < 1024; pdi++) {
        if (!(pd[pdi] & PAGE_PRESENT)) continue;

        uint32_t *pt = (uint32_t *)(pd[pdi] & ~0xFFFu);

        if (pdi == 0) {
            /*
             * Kernel region: the PT struct is ours (we allocated it in
             * paging_new_address_space or paging_clone_address_space),
             * but the physical frames it points to belong to the kernel —
             * never free those.
             */
            pmm_free_page((uint32_t)pt);
        } else {
            /* User region: decref every present frame (CoW-aware), then free PT. */
            for (pti = 0; pti < 1024; pti++) {
                if (pt[pti] & PAGE_PRESENT)
                    pmm_decref(pt[pti] & ~0xFFFu);
            }
            pmm_free_page((uint32_t)pt);
        }
    }

    pmm_free_page(cr3);
}

/* -- paging_cow_fault ------------------------------------------------------- */

uint32_t paging_cow_fault(uint32_t cr3, uint32_t fault_virt) {
    uint32_t *pd  = (uint32_t *)cr3;
    uint32_t  pdi = fault_virt >> 22;
    uint32_t  pti = (fault_virt >> 12) & 0x3FFu;
    uint32_t *pt;
    uint32_t  old_frame, new_frame;

    if (!(pd[pdi] & PAGE_PRESENT)) return (uint32_t)-1;
    pt = (uint32_t *)(pd[pdi] & ~0xFFFu);
    if (!(pt[pti] & PAGE_PRESENT)) return (uint32_t)-1;

    old_frame = pt[pti] & ~0xFFFu;

    if (pmm_getref(old_frame) <= 1u) {
        /* Sole owner -- just restore the write bit */
        pt[pti] |= PAGE_WRITABLE;
        __asm__ volatile ("invlpg (%0)" : : "r"(fault_virt) : "memory");
        return 0;
    }

    /* Shared frame -- allocate a new one, copy, swap in */
    new_frame = (uint32_t)pmm_alloc_page();
    if (!new_frame) return (uint32_t)-1;

    copy_4k((uint32_t *)new_frame, (uint32_t *)old_frame);
    pmm_decref(old_frame);  /* release our share of the old frame */

    pt[pti] = new_frame | (pt[pti] & 0xFFFu) | PAGE_WRITABLE;
    __asm__ volatile ("invlpg (%0)" : : "r"(fault_virt) : "memory");
    return 0;
}
