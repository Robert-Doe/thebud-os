/*
 * process.c — Process creation and round-robin scheduler
 *
 * Module 16 changes
 * ─────────────────
 * process_create_user() is removed — user processes are now created by
 * SYS_EXEC via elf_load(), not by a function pointer compiled into the kernel.
 * process_set_cr3() and process_kernel_stack_top() are added to support exec.
 *
 * Module 15 context: each PCB carries a cr3 field; the scheduler switches CR3
 * on every context switch; ring-3 exceptions are caught by user_fault_hook.
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

#define GDT_SEL_CODE       0x08u
#define GDT_SEL_DATA       0x10u
#define GDT_SEL_USER_CODE  0x1Bu
#define GDT_SEL_USER_DATA  0x23u
#define INITIAL_EFLAGS     0x00000202u

static struct process *find_free_slot(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].state == PROC_DEAD && proc_table[i].pid == 0)
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

static uint32_t build_frame_ring3(void (*entry)(void),
                                   uint8_t *kernel_stack_top,
                                   uint8_t *user_stack_top)
{
    uint32_t *sp = (uint32_t *)kernel_stack_top;
    *--sp = GDT_SEL_USER_DATA;
    *--sp = (uint32_t)user_stack_top;
    *--sp = INITIAL_EFLAGS;
    *--sp = GDT_SEL_USER_CODE;
    *--sp = (uint32_t)entry;
    *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;
    *--sp = GDT_SEL_USER_DATA; *--sp = GDT_SEL_USER_DATA;
    return (uint32_t)sp;
}

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
    p->state            = PROC_READY;
    p->esp              = build_frame_ring0(entry, stack + PROC_STACK_SIZE);
    p->next       = current->next;
    current->next = p;
    proc_count++;
    return p->pid;
}


uint32_t scheduler_tick(uint32_t current_esp) {
    struct process *next;
    current->esp   = current_esp;
    current->state = PROC_READY;
    next = current->next;
    while (next->state != PROC_READY) {
        next = next->next;
        if (next == current) {
            current->state = PROC_RUNNING;
            return current_esp;
        }
    }
    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);
    return current->esp;
}

uint32_t process_current_pid(void) {
    return current ? current->pid : 0;
}

void process_set_cr3(uint32_t cr3) {
    if (current) current->cr3 = cr3;
}

uint32_t process_kernel_stack_top(void) {
    return current ? current->kernel_stack_top : 0;
}

uint32_t process_exit(uint32_t current_esp) {
    struct process *next;
    current->esp   = current_esp;
    current->state = PROC_DEAD;
    proc_count--;
    next = current->next;
    while (next->state != PROC_READY) {
        if (next == current) { __asm__ volatile ("cli; hlt"); for (;;) {} }
        next = next->next;
    }
    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);
    return current->esp;
}
