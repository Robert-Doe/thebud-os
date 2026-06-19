/*
 * process.c — Process creation, fork, wait, and round-robin scheduler
 *
 * Module 17 additions
 * ───────────────────
 *
 * process_fork(frame)
 *   Called by SYS_FORK.  The calling process's interrupt frame sits on its
 *   kernel stack.  Fork:
 *     1. Clones the address space via paging_clone_address_space().
 *     2. Allocates a new kernel stack for the child.
 *     3. Copies the parent's interrupt frame (and the CPU-pushed user_esp/
 *        user_ss words for ring-3 frames) to the top of the child stack.
 *     4. Sets child's EAX = 0 (fork returns 0 in the child).
 *     5. Inserts the child into the scheduler ring.
 *   Returns the parent's unchanged ESP.  Parent's EAX is set to child PID
 *   by syscall_dispatch before returning.
 *
 * process_wait(current_esp, out_exit_code)
 *   If a zombie child exists, reaps it immediately.
 *   Otherwise marks the caller PROC_WAITING and calls scheduler_tick.
 *   When a child exits, process_exit() wakes the PROC_WAITING parent by
 *   setting its state to PROC_READY and writing the exit code directly into
 *   the parent's saved EAX in its interrupt frame.  The parent returns from
 *   SYS_WAIT with that value in EAX naturally.
 *
 * process_exit
 *   Now sets state to PROC_ZOMBIE (not PROC_DEAD) so the parent can reap.
 *   Wakes a PROC_WAITING parent if one exists.
 *   Frees the exiting process's address space via paging_free_address_space()
 *   AFTER switching to the next process's CR3.
 */

#include <stdint.h>
#include "process.h"
#include "heap.h"
#include "isr.h"
#include "tss.h"
#include "paging.h"

static struct process proc_table[MAX_PROCESSES];
static struct process *current = 0;
static uint32_t       next_pid = 1;
static int            proc_count = 0;

#define GDT_SEL_CODE   0x08u
#define GDT_SEL_DATA   0x10u
#define INITIAL_EFLAGS 0x00000202u

/* ── helpers ─────────────────────────────────────────────────────────── */

static struct process *find_free_slot(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].state == PROC_DEAD && proc_table[i].pid == 0)
            return &proc_table[i];
    return 0;
}

static struct process *find_by_pid(uint32_t pid) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].pid == pid)
            return &proc_table[i];
    return 0;
}

static uint32_t build_frame_ring0(void (*entry)(void), uint8_t *stack_top) {
    uint32_t *sp = (uint32_t *)stack_top;
    *--sp = INITIAL_EFLAGS;
    *--sp = GDT_SEL_CODE;
    *--sp = (uint32_t)entry;
    *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = GDT_SEL_DATA; *--sp = GDT_SEL_DATA;
    *--sp = GDT_SEL_DATA; *--sp = GDT_SEL_DATA;
    return (uint32_t)sp;
}

/* ── process_init ────────────────────────────────────────────────────── */

void process_init(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        proc_table[i].pid              = 0;
        proc_table[i].state            = PROC_DEAD;
        proc_table[i].next             = 0;
        proc_table[i].stack            = 0;
        proc_table[i].esp              = 0;
        proc_table[i].kernel_stack_top = 0;
        proc_table[i].cr3              = 0;
        proc_table[i].parent_pid       = 0;
        proc_table[i].exit_code        = 0;
    }
    proc_table[0].pid   = next_pid++;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].next  = &proc_table[0];
    proc_table[0].cr3   = paging_kernel_cr3();
    current    = &proc_table[0];
    proc_count = 1;
    isr_set_scheduler(scheduler_tick);
    isr_set_user_fault_handler(process_exit);
}

/* ── process_create (ring-0 kernel process) ──────────────────────────── */

uint32_t process_create(void (*entry)(void)) {
    struct process *p;
    uint8_t *stack;
    if (proc_count >= MAX_PROCESSES) return 0;
    stack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!stack) return 0;
    p = find_free_slot();
    if (!p) { kfree(stack); return 0; }
    p->pid              = next_pid++;
    p->stack            = stack;
    p->kernel_stack_top = 0;
    p->cr3              = paging_kernel_cr3();
    p->parent_pid       = current->pid;
    p->exit_code        = 0;
    p->state            = PROC_READY;
    p->esp              = build_frame_ring0(entry, stack + PROC_STACK_SIZE);
    p->next       = current->next;
    current->next = p;
    proc_count++;
    return p->pid;
}

/* ── process_fork ────────────────────────────────────────────────────── */

uint32_t process_fork(struct interrupt_frame *frame) {
    struct process *child;
    uint8_t *child_kstack;
    uint32_t child_cr3;
    uint32_t frame_bytes;
    uint8_t *child_frame_ptr;
    uint32_t k;

    if (proc_count >= MAX_PROCESSES) {
        frame->eax = (uint32_t)-1;
        return (uint32_t)frame;
    }

    /* Clone address space — deep copy of all user pages. */
    child_cr3 = paging_clone_address_space(current->cr3);
    if (!child_cr3) {
        frame->eax = (uint32_t)-1;
        return (uint32_t)frame;
    }

    child_kstack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!child_kstack) {
        paging_free_address_space(child_cr3);
        frame->eax = (uint32_t)-1;
        return (uint32_t)frame;
    }

    child = find_free_slot();
    if (!child) {
        kfree(child_kstack);
        paging_free_address_space(child_cr3);
        frame->eax = (uint32_t)-1;
        return (uint32_t)frame;
    }

    /*
     * Copy the interrupt frame (and user_esp/user_ss for ring-3 callers)
     * to the top of the child's kernel stack.
     *
     * For a ring-3 caller (cs & 3 == 3), the CPU pushed user_esp and user_ss
     * ABOVE the struct on the kernel stack.  isr_common_stub relies on them
     * being there when iret fires.  We must copy those 8 extra bytes too.
     *
     * Layout on stack (low addr = bottom of struct, high = above struct):
     *   [frame->gs ... frame->eflags]   ← sizeof(interrupt_frame) bytes
     *   [user_esp] [user_ss]            ← 8 extra bytes (ring-3 only)
     */
    frame_bytes = sizeof(struct interrupt_frame);
    if (frame->cs & 3u) frame_bytes += 8u;

    child_frame_ptr = (uint8_t *)(child_kstack + PROC_STACK_SIZE) - frame_bytes;
    for (k = 0; k < frame_bytes; k++)
        child_frame_ptr[k] = ((uint8_t *)frame)[k];

    /* Child gets 0 from fork(); parent gets child PID (set below). */
    ((struct interrupt_frame *)child_frame_ptr)->eax = 0;

    child->pid              = next_pid++;
    child->stack            = child_kstack;
    child->kernel_stack_top = (uint32_t)(child_kstack + PROC_STACK_SIZE);
    child->cr3              = child_cr3;
    child->parent_pid       = current->pid;
    child->exit_code        = 0;
    child->state            = PROC_READY;
    child->esp              = (uint32_t)child_frame_ptr;
    child->next             = current->next;
    current->next           = child;
    proc_count++;

    /* Parent gets the child PID returned from SYS_FORK. */
    frame->eax = child->pid;
    return (uint32_t)frame;
}

/* ── process_wait ────────────────────────────────────────────────────── */

uint32_t process_wait(uint32_t current_esp, uint32_t *out_exit_code) {
    int i;

    /* Scan for a zombie child. */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].parent_pid == current->pid &&
            proc_table[i].state      == PROC_ZOMBIE) {
            /* Reap: collect exit code, free slot. */
            *out_exit_code       = proc_table[i].exit_code;
            proc_table[i].state  = PROC_DEAD;
            proc_table[i].pid    = 0;
            proc_count--;
            return current_esp;   /* caller gets exit_code via out ptr */
        }
    }

    /*
     * No zombie yet.  Mark ourselves PROC_WAITING and yield.
     * process_exit() will wake us when our child exits by setting our
     * state back to PROC_READY and writing the exit code into our saved
     * EAX in the interrupt frame pointed to by our saved ESP.
     */
    current->esp   = current_esp;
    current->state = PROC_WAITING;
    return scheduler_tick(current_esp);
}

/* ── scheduler_tick ──────────────────────────────────────────────────── */

uint32_t scheduler_tick(uint32_t current_esp) {
    struct process *next;
    current->esp = current_esp;
    if (current->state == PROC_RUNNING) current->state = PROC_READY;
    next = current->next;
    while (next->state != PROC_READY) {
        next = next->next;
        if (next == current) {
            if (current->state == PROC_READY) {
                current->state = PROC_RUNNING;
                return current_esp;
            }
            /* All processes waiting or zombie — idle spin. */
            __asm__ volatile ("sti; hlt");
            next = current->next;
        }
    }
    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);
    return current->esp;
}

/* ── process_exit ────────────────────────────────────────────────────── */

uint32_t process_exit(uint32_t current_esp) {
    struct process *dead = current;
    struct process *parent = 0;
    struct process *next;
    uint32_t old_cr3 = dead->cr3;
    int i;

    dead->esp       = current_esp;
    dead->exit_code = 0;           /* exit code passed via SYS_EXIT arg — set by caller */
    dead->state     = PROC_ZOMBIE;

    /*
     * Wake a PROC_WAITING parent.
     * Write the exit code directly into the parent's saved EAX so when
     * the parent resumes from SYS_WAIT it naturally gets the value in EAX.
     */
    if (dead->parent_pid != 0) {
        parent = find_by_pid(dead->parent_pid);
        if (parent && parent->state == PROC_WAITING) {
            struct interrupt_frame *pf = (struct interrupt_frame *)parent->esp;
            pf->eax          = dead->exit_code;
            parent->state    = PROC_READY;
        }
    }

    /* Find the next READY process to run. */
    next = dead->next;
    for (i = 0; i < MAX_PROCESSES && next->state != PROC_READY; i++) {
        next = next->next;
        if (next == dead) {
            /* Nothing ready — run idle (PID 1). */
            next = &proc_table[0];
            break;
        }
    }

    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);

    /*
     * Now safe to free the dead process's address space: we've already
     * switched to a different CR3.  Don't free the kernel's own PD.
     */
    if (old_cr3 != paging_kernel_cr3())
        paging_free_address_space(old_cr3);

    return current->esp;
}

/* ── process_exit_with_code ──────────────────────────────────────────── */

uint32_t process_exit_with_code(uint32_t current_esp, uint32_t code) {
    current->exit_code = code;
    return process_exit(current_esp);
}

/* ── misc accessors ──────────────────────────────────────────────────── */

uint32_t process_current_pid(void)   { return current ? current->pid : 0; }
void     process_set_cr3(uint32_t c) { if (current) current->cr3 = c; }
uint32_t process_kernel_stack_top(void) { return current ? current->kernel_stack_top : 0; }
