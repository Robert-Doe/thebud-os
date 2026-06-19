/*
 * process.c — Process creation, fork, wait, and round-robin scheduler
 *
 * Module 18 additions
 * ───────────────────
 *
 * Signal delivery flow
 *   1. process_send_signal(pid, signo) sets bit `signo` in target->pending_signals.
 *      For SIGKILL targeting a non-current process, the process is zombified
 *      immediately (we're still in the sender's CR3, so it's safe to free the
 *      victim's address space right now).
 *   2. scheduler_tick(), just before returning, calls deliver_user_signals().
 *      This loops over every pending bit and either kills the process (SIG_DFL
 *      for fatal signals) or builds a signal call frame on the user stack.
 *   3. Signal call frame layout (cdecl, grows down):
 *        [new_user_esp + 0] = original EIP  (return address for handler's `ret`)
 *        [new_user_esp + 4] = signo          (first argument, int sig)
 *      The saved EIP in the interrupt frame is replaced with the handler address.
 *      When the handler returns via `ret`, execution resumes at the original EIP.
 *
 * process_fault_handler replaces the raw process_exit hook registered with
 * isr_set_user_fault_handler.  For SIGSEGV it checks whether the current
 * process has a user-installed handler; if so, it delivers the signal in-place
 * instead of killing.  All other exceptions still kill the process.
 */

#include <stdint.h>
#include "process.h"
#include "heap.h"
#include "isr.h"
#include "tss.h"
#include "paging.h"
#include "vga.h"

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

/* ── signal helpers ──────────────────────────────────────────────────── */

/*
 * zombify — transition a process to ZOMBIE, wake a PROC_WAITING parent.
 * Does NOT free address space or touch CR3.  Caller is responsible for
 * freeing p->cr3 if the caller is still in a different address space.
 */
static void zombify(struct process *p, uint32_t code) {
    struct process *parent;
    p->exit_code       = code;
    p->state           = PROC_ZOMBIE;
    p->pending_signals = 0;
    if (p->parent_pid == 0) return;
    parent = find_by_pid(p->parent_pid);
    if (parent && parent->state == PROC_WAITING) {
        struct interrupt_frame *pf = (struct interrupt_frame *)parent->esp;
        pf->eax       = code;
        parent->state = PROC_READY;
    }
}

/*
 * deliver_user_signals — called from scheduler_tick after paging_switch.
 *
 * For each pending signal (lowest-numbered first):
 *   SIGKILL  — can't be caught; kill immediately (handled in scheduler_tick
 *               loop before we even switch to the process; this path handles
 *               the unlikely case of current process having SIGKILL set).
 *   SIG_DFL  — fatal signals (SIGSEGV, SIGTERM, SIGHUP) call process_exit.
 *               Others are silently ignored.
 *   SIG_IGN  — skip.
 *   handler  — build call frame on user stack, redirect EIP.
 */
static void deliver_user_signals(void) {
    uint32_t signo, handler;
    struct interrupt_frame *f;
    uint32_t *user_esp_ptr, user_esp;
    uint32_t *user_sp;

    for (signo = 1u; signo < (uint32_t)NSIG; signo++) {
        if (!(current->pending_signals & (1u << signo))) continue;
        current->pending_signals &= ~(1u << signo);

        /* SIGKILL — process_exit will select a new current; return immediately */
        if (signo == (uint32_t)SIGKILL) {
            process_exit_with_code(current->esp, 9u);
            return;
        }

        handler = current->signal_handlers[signo];

        if (handler == SIG_DFL || handler == 0u) {
            /* Fatal default action */
            if (signo == (uint32_t)SIGSEGV ||
                signo == (uint32_t)SIGTERM  ||
                signo == (uint32_t)SIGHUP) {
                process_exit_with_code(current->esp, signo);
                return;
            }
            /* Non-fatal default: ignore */
            continue;
        }

        if (handler == SIG_IGN) continue;

        /* User handler — only meaningful for ring-3 processes */
        f = (struct interrupt_frame *)current->esp;
        if (!(f->cs & 3u)) continue;

        /*
         * Build the signal call frame on the user stack.
         *
         * cdecl calling convention: on entry to the handler, ESP points to
         * the return address, ESP+4 holds the first argument.
         *
         *   [new_user_esp + 0] = original EIP  ← `ret` jumps back here
         *   [new_user_esp + 4] = signo          ← handler's (int sig) argument
         *
         * user_esp is stored ABOVE the struct (CPU pushes it on ring-3→0 switch).
         */
        user_esp_ptr  = (uint32_t *)((uint8_t *)f + sizeof(*f));
        user_esp      = *user_esp_ptr;
        user_sp       = (uint32_t *)user_esp;
        *--user_sp    = (uint32_t)signo;    /* argument */
        *--user_sp    = f->eip;             /* return address */

        *user_esp_ptr = (uint32_t)user_sp;
        f->eip        = handler;
    }
}

/* ── process_init ────────────────────────────────────────────────────── */

void process_init(void) {
    int i, s;
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
        proc_table[i].pending_signals  = 0;
        for (s = 0; s < NSIG; s++) proc_table[i].signal_handlers[s] = SIG_DFL;
    }
    proc_table[0].pid   = next_pid++;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].next  = &proc_table[0];
    proc_table[0].cr3   = paging_kernel_cr3();
    current    = &proc_table[0];
    proc_count = 1;
    isr_set_scheduler(scheduler_tick);
    isr_set_user_fault_handler(process_fault_handler);
}

/* ── process_create (ring-0 kernel process) ──────────────────────────── */

uint32_t process_create(void (*entry)(void)) {
    struct process *p;
    uint8_t *stack;
    int s;
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
    p->pending_signals  = 0;
    for (s = 0; s < NSIG; s++) p->signal_handlers[s] = SIG_DFL;
    p->esp              = build_frame_ring0(entry, stack + PROC_STACK_SIZE);
    p->next             = current->next;
    current->next       = p;
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
    int s;

    if (proc_count >= MAX_PROCESSES) {
        frame->eax = (uint32_t)-1;
        return (uint32_t)frame;
    }

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

    frame_bytes = sizeof(struct interrupt_frame);
    if (frame->cs & 3u) frame_bytes += 8u;

    child_frame_ptr = (uint8_t *)(child_kstack + PROC_STACK_SIZE) - frame_bytes;
    for (k = 0; k < frame_bytes; k++)
        child_frame_ptr[k] = ((uint8_t *)frame)[k];

    ((struct interrupt_frame *)child_frame_ptr)->eax = 0;

    child->pid              = next_pid++;
    child->stack            = child_kstack;
    child->kernel_stack_top = (uint32_t)(child_kstack + PROC_STACK_SIZE);
    child->cr3              = child_cr3;
    child->parent_pid       = current->pid;
    child->exit_code        = 0;
    child->state            = PROC_READY;
    child->pending_signals  = 0;
    /* Inherit signal handlers from parent (child can re-register if needed) */
    for (s = 0; s < NSIG; s++) child->signal_handlers[s] = current->signal_handlers[s];
    child->esp              = (uint32_t)child_frame_ptr;
    child->next             = current->next;
    current->next           = child;
    proc_count++;

    frame->eax = child->pid;
    return (uint32_t)frame;
}

/* ── process_wait ────────────────────────────────────────────────────── */

uint32_t process_wait(uint32_t current_esp, uint32_t *out_exit_code) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].parent_pid == current->pid &&
            proc_table[i].state      == PROC_ZOMBIE) {
            *out_exit_code       = proc_table[i].exit_code;
            proc_table[i].state  = PROC_DEAD;
            proc_table[i].pid    = 0;
            proc_count--;
            return current_esp;
        }
    }
    current->esp   = current_esp;
    current->state = PROC_WAITING;
    return scheduler_tick(current_esp);
}

/* ── scheduler_tick ──────────────────────────────────────────────────── */

uint32_t scheduler_tick(uint32_t current_esp) {
    struct process *next;
    uint32_t old_cr3;

    current->esp = current_esp;
    if (current->state == PROC_RUNNING) current->state = PROC_READY;

    next = current->next;
    for (;;) {
        if (next->state == PROC_READY) {
            /*
             * SIGKILL check: kill the process before it ever gets the CPU.
             * We are still in `current`'s CR3, which is different from
             * `next`'s CR3, so freeing next->cr3 here is safe.
             */
            if (next->pending_signals & (1u << (uint32_t)SIGKILL)) {
                old_cr3 = next->cr3;
                zombify(next, 9u);
                if (old_cr3 != paging_kernel_cr3())
                    paging_free_address_space(old_cr3);
                next->cr3 = paging_kernel_cr3();
                /* keep scanning */
            } else {
                break;  /* found a runnable process */
            }
        }
        next = next->next;
        if (next == current) {
            /* Wrapped around — is current itself runnable? */
            if (current->state == PROC_READY &&
                !(current->pending_signals & (1u << (uint32_t)SIGKILL))) {
                current->state = PROC_RUNNING;
                paging_switch(current->cr3);
                if (current->pending_signals) deliver_user_signals();
                return current->esp;
            }
            /* All processes are blocked/zombie or current has SIGKILL pending */
            __asm__ volatile ("sti; hlt");
            next = current->next;
        }
    }

    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);

    /* Deliver any pending signals before returning to ring-3 */
    if (current->pending_signals)
        deliver_user_signals();

    return current->esp;
}

/* ── process_exit ────────────────────────────────────────────────────── */

uint32_t process_exit(uint32_t current_esp) {
    struct process *dead = current;
    struct process *next;
    uint32_t old_cr3 = dead->cr3;
    int i;

    dead->esp = current_esp;
    zombify(dead, dead->exit_code);

    next = dead->next;
    for (i = 0; i < MAX_PROCESSES && next->state != PROC_READY; i++) {
        next = next->next;
        if (next == dead) {
            next = &proc_table[0];
            break;
        }
    }

    current        = next;
    current->state = PROC_RUNNING;
    if (current->kernel_stack_top != 0)
        tss_set_kernel_stack(current->kernel_stack_top);
    paging_switch(current->cr3);

    if (old_cr3 != paging_kernel_cr3())
        paging_free_address_space(old_cr3);

    return current->esp;
}

uint32_t process_exit_with_code(uint32_t current_esp, uint32_t code) {
    current->exit_code = code;
    return process_exit(current_esp);
}

/* ── signal API ──────────────────────────────────────────────────────── */

void process_send_signal(uint32_t pid, int signo) {
    struct process *p;
    uint32_t old_cr3;

    if (signo <= 0 || signo >= NSIG) return;
    p = find_by_pid(pid);
    if (!p || p->state == PROC_DEAD || p->state == PROC_ZOMBIE) return;

    if (signo == SIGKILL && p != current) {
        /*
         * SIGKILL to a non-running process: zombify immediately.
         * We are currently in `current`'s CR3; `p`'s CR3 is different,
         * so freeing it now is safe (it is not the active page directory).
         */
        old_cr3 = p->cr3;
        zombify(p, 9u);
        if (old_cr3 != paging_kernel_cr3())
            paging_free_address_space(old_cr3);
        p->cr3 = paging_kernel_cr3();
        return;
    }

    p->pending_signals |= (1u << (uint32_t)signo);
    if (p->state == PROC_WAITING) p->state = PROC_READY;
}

void process_set_handler(int signo, uint32_t handler_addr) {
    if (signo <= 0 || signo >= NSIG || signo == SIGKILL) return;
    if (current) current->signal_handlers[signo] = handler_addr;
}

uint32_t process_fault_handler(uint32_t esp) {
    struct interrupt_frame *f = (struct interrupt_frame *)esp;
    uint32_t handler;
    uint32_t *user_esp_ptr, user_esp;
    uint32_t *user_sp;

    if (!current) return esp;

    handler = current->signal_handlers[SIGSEGV];

    /* If a user handler is registered and the faulting code is ring-3 */
    if (handler != SIG_DFL && handler != SIG_IGN && handler != 0u &&
        (f->cs & 3u)) {
        kprintf("[signal] SIGSEGV caught by PID %u, delivering to handler 0x%x\n",
                current->pid, handler);
        user_esp_ptr  = (uint32_t *)((uint8_t *)f + sizeof(*f));
        user_esp      = *user_esp_ptr;
        user_sp       = (uint32_t *)user_esp;
        *--user_sp    = (uint32_t)SIGSEGV;
        *--user_sp    = f->eip;
        *user_esp_ptr = (uint32_t)user_sp;
        f->eip        = handler;
        return esp;   /* resume process — now headed to the SIGSEGV handler */
    }

    /* Default: kill */
    current->exit_code = (uint32_t)SIGSEGV;
    return process_exit(esp);
}

/* ── misc accessors ──────────────────────────────────────────────────── */

uint32_t process_current_pid(void)      { return current ? current->pid : 0; }
void     process_set_cr3(uint32_t c)    { if (current) current->cr3 = c; }
uint32_t process_kernel_stack_top(void) { return current ? current->kernel_stack_top : 0; }
