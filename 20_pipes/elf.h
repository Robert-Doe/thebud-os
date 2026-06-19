/*
 * elf.h — Minimal ELF32 structures for the BobOS ELF loader
 *
 * We only need the fields that matter for loading a static executable:
 *   - ELF header: magic check, entry point, program header offset/count
 *   - Program header: type (LOAD), vaddr, file offset, sizes, flags
 *
 * Everything else (section headers, symbol tables, dynamic linking) is
 * ignored — we only load statically-linked flat executables.
 */

#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* ELF magic bytes at offset 0 */
#define ELF_MAGIC0  0x7Fu
#define ELF_MAGIC1  'E'
#define ELF_MAGIC2  'L'
#define ELF_MAGIC3  'F'

/* e_type values */
#define ET_EXEC     2u      /* executable file */

/* e_machine */
#define EM_386      3u      /* x86 32-bit */

/* Program header p_type values */
#define PT_LOAD     1u      /* loadable segment */

/* Program header p_flags */
#define PF_X        (1u << 0)   /* execute */
#define PF_W        (1u << 1)   /* write   */
#define PF_R        (1u << 2)   /* read    */

/* ELF32 header — exactly 52 bytes */
typedef struct {
    uint8_t  e_ident[16];   /* magic + class + data + version + padding */
    uint16_t e_type;        /* ET_EXEC for an executable */
    uint16_t e_machine;     /* EM_386 */
    uint32_t e_version;     /* 1 */
    uint32_t e_entry;       /* virtual address of _start */
    uint32_t e_phoff;       /* offset of program header table */
    uint32_t e_shoff;       /* offset of section header table (ignored) */
    uint32_t e_flags;       /* processor-specific flags (0 for x86) */
    uint16_t e_ehsize;      /* size of this header (52) */
    uint16_t e_phentsize;   /* size of one program header entry (32) */
    uint16_t e_phnum;       /* number of program header entries */
    uint16_t e_shentsize;   /* size of one section header entry */
    uint16_t e_shnum;       /* number of section headers */
    uint16_t e_shstrndx;    /* section name string table index */
} __attribute__((packed)) Elf32_Ehdr;

/* ELF32 program header — exactly 32 bytes */
typedef struct {
    uint32_t p_type;    /* PT_LOAD, PT_NULL, … */
    uint32_t p_offset;  /* offset in the file */
    uint32_t p_vaddr;   /* target virtual address */
    uint32_t p_paddr;   /* physical address (ignored) */
    uint32_t p_filesz;  /* bytes in the file */
    uint32_t p_memsz;   /* bytes in memory (>= p_filesz; rest zeroed) */
    uint32_t p_flags;   /* PF_R, PF_W, PF_X */
    uint32_t p_align;   /* alignment (must be power of two) */
} __attribute__((packed)) Elf32_Phdr;

#endif /* ELF_H */
