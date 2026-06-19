/*
 * process.c — Process creation and round-robin scheduler
 *
 * Module 14 changes
 * ─────────────────
 * Ring-3 processes need two stacks and a larger fake frame.
 *
 * The fake frame for a ring-3 process has two extra words compared to a
 * ring-0 frame.  They sit at the TOP of the frame (highest addresses),
 * because the CPU pushes them there automatically during a ring-3 interrupt:
 *
 *   High address  ─────────────────────────────────────────
 *     user_ss   = GDT_SEL_USER_DATA (0x23)   ─┐
 *     user_esp  = top of the user stack        ├─ CPU pops on iret to ring 3
 *     eflags    = 0x202 (IF=1)               ─┘
 *     cs        = GDT_SEL_USER_CODE (0x1B)
 *     eip       = entry function
 *     err_code  = 0
 *     int_no    = 0
 *     eax..edi  = 0
 *     ds/es/fs/gs = GDT_SEL_USER_DATA  ← PCB.esp points here
 *   Low address  ──────────────────────────────────────────
 *
 * scheduler_tick() calls tss_set_kernel_stack() before running any ring-3
 * process so TSS.esp0 always points at that process's kernel stack.
 */

#include <stdint.h>
#include "process.h"
#include "heap.h"
#include "isr.h"
#include "tss.h"

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
    *--sp = 0; *--sp = 0;                    /* err_code, int_no */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;  /* eax ecx edx ebx */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;  /* esp_d ebp esi edi */
    *--sp = GDT_SEL_DATA; *--sp = GDT_SEL_DATA;
    *--sp = GDT_SEL_DATA; *--sp = GDT_SEL_DATA;  /* ds es fs gs */
    return (uint32_t)sp;
}

static uint32_t build_frame_ring3(void (*entry)(void),
                                   uint8_t *kernel_stack_top,
                                   uint8_t *user_stack_top)
{
    uint32_t *sp = (uint32_t *)kernel_stack_top;
    /* Extra: CPU pops these on iret when CS.RPL=3 */
    *--sp = GDT_SEL_USER_DATA;           /* user ss  */
    *--sp = (uint32_t)user_stack_top;    /* user esp */
    /* Standard iret frame */
    *--sp = INITIAL_EFLAGS;
    *--sp = GDT_SEL_USER_CODE;           /* cs = 0x1B */
    *--sp = (uint32_t)entry;
    *--sp = 0; *--sp = 0;                    /* err_code, int_no */
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
    /* Segment registers: must be user selectors */
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
    }
    proc_table[0].pid   = next_pid++;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].next  = &proc_table[0];
    current    = &proc_table[0];
    proc_count = 1;
    isr_set_scheduler(scheduler_tick);
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
    p->state            = PROC_READY;
    p->esp              = build_frame_ring0(entry, stack + PROC_STACK_SIZE);
    p->next       = current->next;
    current->next = p;
    proc_count++;
    return p->pid;
}

uint32_t process_create_user(void (*entry)(void)) {
    struct process *p;
    uint8_t *kstack;
    uint8_t *ustack;
    if (proc_count >= MAX_PROCESSES) return 0;
    kstack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!kstack) return 0;
    ustack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!ustack) { kfree(kstack); return 0; }
    p = find_free_slot();
    if (!p) { kfree(kstack); kfree(ustack); return 0; }
    p->pid              = next_pid++;
    p->stack            = kstack;
    p->kernel_stack_top = (uint32_t)(kstack + PROC_STACK_SIZE);
    p->state            = PROC_READY;
    p->esp              = build_frame_ring3(entry,
                                            kstack + PROC_STACK_SIZE,
                                            ustack + PROC_STACK_SIZE);
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
    return current->esp;
}

uint32_t process_current_pid(void) {
    return current ? current->pid : 0;
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
    return current->esp;
}
