/*
 * paging.h — Virtual Memory / Paging interface
 *
 * Module 17 additions:
 *   paging_clone_address_space(src_cr3) — deep-copy a page directory for fork.
 *     Kernel pages (PDE[0], 0-4MB) are copied supervisor-only (shared frames,
 *     not duplicated).  User pages (PDE[1+]) get fresh physical frames with the
 *     contents of the source frames copied in.
 *
 *   paging_free_address_space(cr3) — release all physical pages owned by a
 *     user process's PD.  Kernel frames are not freed (they are shared).
 *     Must NOT be called while cr3 is the active CR3.
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT   (1u << 0)
#define PAGE_WRITABLE  (1u << 1)
#define PAGE_USER      (1u << 2)

void     paging_init(void);
uint32_t paging_new_address_space(void);
void     paging_set_user_page(uint32_t cr3, uint32_t virt_addr);
void     paging_switch(uint32_t cr3);
uint32_t paging_kernel_cr3(void);

/*
 * paging_clone_address_space(src_cr3)
 *
 * Creates a new PD that is a deep copy of the source PD, for use by fork().
 *
 * For PDE[0] (0-4MB kernel region):
 *   Allocates a new PT and copies the kernel's PTEs with PAGE_USER stripped.
 *   The physical frames are SHARED (not copied) — these are kernel pages.
 *
 * For PDE[1..1023] (user regions):
 *   For each present PTE: allocates a fresh physical frame, copies the 4KB
 *   of data from the source frame into it, and maps it with the same flags.
 *
 * Returns the physical address of the new PD, or 0 on OOM.
 */
uint32_t paging_clone_address_space(uint32_t src_cr3);

/*
 * paging_free_address_space(cr3)
 *
 * Frees all physical memory owned by the process at cr3:
 *   - User frames (PDE[1+] PTEs) are returned to the PMM.
 *   - User PTs (PDE[1+]) are returned to the PMM.
 *   - The copied kernel PT (PDE[0]) is returned to the PMM.
 *   - The PD itself is returned to the PMM.
 *
 * Kernel frames (pointed to by the kernel PT copy in PDE[0]) are NOT freed.
 * Must not be called while this cr3 is loaded in CR3.
 */
void paging_free_address_space(uint32_t cr3);

uint32_t paging_get_pd_phys(void);
uint32_t paging_get_pt_phys(uint32_t pdi);

/*
 * paging_cow_fault(cr3, fault_virt)
 *
 * Called from the #PF handler when a write fault hits a read-only user page.
 * If the physical frame has refcount > 1: allocate a new frame, copy content,
 * decref old, and map the new frame with write permission.
 * If refcount == 1 (sole owner): just restore the write bit.
 * Returns 0 on success, -1 on OOM.
 */
uint32_t paging_cow_fault(uint32_t cr3, uint32_t fault_virt);

/*
 * paging_map_user_phys(cr3, virt, phys)
 *
 * Map a specific physical frame at the given virtual address in cr3.
 * Used by shared memory to install the same phys frame in multiple PDs.
 * Creates a new PT if the PDE is not present.
 */
void paging_map_user_phys(uint32_t cr3, uint32_t virt, uint32_t phys);

#endif /* PAGING_H */
