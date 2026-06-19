/*
 * elf_loader.h — ELF32 executable loader
 *
 * elf_load(filename, out_entry, out_cr3)
 *   Reads a static ELF32 executable from BobFS, creates a fresh page directory,
 *   maps every PT_LOAD segment into that address space (page-aligned, user-accessible),
 *   and returns the entry point and new CR3 to the caller.
 *
 * Returns 0 on success, negative on error:
 *   -1  file not found in BobFS
 *   -2  not a valid ELF32 executable for x86
 *   -3  out of physical memory
 */

#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>

int elf_load(const char *filename,
             uint32_t   *out_entry,
             uint32_t   *out_cr3);

#endif /* ELF_LOADER_H */
