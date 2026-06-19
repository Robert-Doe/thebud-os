/*
 * process.c — Process creation and round-robin scheduler
 *
 * ── How the context switch works ──────────────────────────────────────────
 *
 * When IRQ0 (the timer) fires, the CPU automatically pushes EFLAGS, CS, and
 * EIP onto the CURRENT process's stack, then our isr_common_stub pushes all
 * other registers (via pusha + push ds/es/fs/gs).  At that point the stack
 * looks exactly like a struct interrupt_frame.  The stub then calls:
 *
 *     new_esp = interrupt_handler(frame);   // EAX = return value
 *     mov esp, eax                          // switch to new_esp
 *     pop gs / pop fs / pop es / pop ds
 *     popa
 *     add esp, 8                            // skip int_no and err_code
 *     iret                                  // pop EIP, CS, EFLAGS → jump!
 *
 * interrupt_handler() calls scheduler_tick(current_esp) here.
 * scheduler_tick() saves current_esp into the running process's PCB, picks
 * the next READY process, and returns its saved ESP.
 *
 * The stub then restores registers FROM THAT DIFFERENT STACK and iret jumps
 * to the other process's EIP.  That's the whole trick — one MOV ESP.
 *
 * ── Setting up a new process's initial stack ───────────────────────────
 *
 * A freshly created process has never run, so there is no real saved frame.
 * We build a FAKE interrupt_frame on its stack so the first context switch
 * into it behaves identically to any other switch:
 *
 *   High address ─────────────────────────────────────
 *     eflags  = 0x202  (bit 1 always set; IF=1 → interrupts enabled)
 *     cs      = 0x08   (GDT kernel code selector)
 *     eip     = entry  (function to start executing)
 *     err_code= 0
 *     int_no  = 0
 *     eax..edi= 0      (all GP registers zero)
 *     ds/es/fs/gs = 0x10  (GDT kernel data selector)
 *   Low address  ← saved ESP points here ────────────
 *
 * When the scheduler returns this ESP and the stub pops/irets, the CPU
 * jumps to entry() as if it had always been running.
 */

#include <stdint.h>
#include "process.h"
#include "heap.h"
#include "isr.h"

/* ── Internal state ──────────────────────────────────────────────────── */

static struct process proc_table[MAX_PROCESSES];
static struct process *current = 0;     /* currently running process   */
static uint32_t       next_pid = 1;     /* PID counter                 */
static int            proc_count = 0;   /* live processes              */

/* ── Fake interrupt_frame size (bytes) ────────────────────────────────
 *
 * struct interrupt_frame fields in stack order (low → high):
 *   gs, fs, es, ds            4 × 4 = 16
 *   edi,esi,ebp,esp_dummy,
 *   ebx,edx,ecx,eax           8 × 4 = 32   (from pusha)
 *   int_no, err_code          2 × 4 =  8
 *   eip, cs, eflags           3 × 4 = 12
 *                             ──────────
 *   Total                             68 bytes
 */
#define FRAME_WORDS  17u    /* 68 bytes / 4 */

/* GDT selectors established in Module 4. */
#define GDT_SEL_CODE  0x08u
#define GDT_SEL_DATA  0x10u

/* EFLAGS value for a new process: bit 1 (reserved, always 1) + IF (bit 9). */
#define INITIAL_EFLAGS  0x00000202u

/* ── Internal helpers ─────────────────────────────────────────────────── */

static struct process *find_free_slot(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_DEAD && proc_table[i].pid == 0) {
            return &proc_table[i];
        }
    }
    return 0;
}

/*
 * build_initial_frame(entry, stack_top)
 *
 * Fills in a fake interrupt_frame at the top of the process's stack and
 * returns the ESP that should be saved in the PCB (points at the bottom
 * of the frame = where 'gs' lives).
 *
 * We pre-decrement a uint32_t pointer and push each field from HIGH address
 * to LOW address, mirroring exactly what the CPU + isr_common_stub do when
 * they save state during a real interrupt.
 */
static uint32_t build_initial_frame(void (*entry)(void), uint8_t *stack_top) {
    uint32_t *sp = (uint32_t *)stack_top;

    /* CPU pushes these last (highest addresses in the frame). */
    *--sp = INITIAL_EFLAGS;         /* eflags                         */
    *--sp = GDT_SEL_CODE;           /* cs                             */
    *--sp = (uint32_t)entry;        /* eip  → first instruction       */

    /* isr_common_stub pushes these before pusha. */
    *--sp = 0;                      /* err_code (dummy)               */
    *--sp = 0;                      /* int_no   (dummy)               */

    /* pusha saves EAX first (highest) … EDI last (lowest). */
    *--sp = 0;                      /* eax */
    *--sp = 0;                      /* ecx */
    *--sp = 0;                      /* edx */
    *--sp = 0;                      /* ebx */
    *--sp = 0;                      /* esp_dummy (pusha captures old ESP) */
    *--sp = 0;                      /* ebp */
    *--sp = 0;                      /* esi */
    *--sp = 0;                      /* edi */

    /* stub pushes DS, ES, FS, GS — GS ends up at the lowest address.
     * push ds → push es → push fs → push gs in the stub, so on stack
     * (low→high): gs, fs, es, ds.  We push high→low: ds, es, fs, gs. */
    *--sp = GDT_SEL_DATA;           /* ds */
    *--sp = GDT_SEL_DATA;           /* es */
    *--sp = GDT_SEL_DATA;           /* fs */
    *--sp = GDT_SEL_DATA;           /* gs  ← saved ESP points here   */

    return (uint32_t)sp;
}

/* ── Public API ───────────────────────────────────────────────────────── */

void process_init(void) {
    int i;

    /* Zero the table so find_free_slot() can distinguish empty slots. */
    for (i = 0; i < MAX_PROCESSES; i++) {
        proc_table[i].pid   = 0;
        proc_table[i].state = PROC_DEAD;
        proc_table[i].next  = 0;
        proc_table[i].stack = 0;
        proc_table[i].esp   = 0;
    }

    /*
     * Create the "idle" process for the current kernel context.
     * We do NOT allocate a stack — kernel_main() already has one (the boot
     * stack at 0x90000 set by kernel_entry.asm).  We set esp=0 as a
     * sentinel; scheduler_tick() will fill it in on the first preemption.
     */
    proc_table[0].pid   = next_pid++;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].stack = 0;    /* uses the existing boot stack */
    proc_table[0].esp   = 0;
    proc_table[0].next  = &proc_table[0];   /* circular list, one entry */

    current     = &proc_table[0];
    proc_count  = 1;

    /* Hook the scheduler into the IRQ0 path inside isr.c. */
    isr_set_scheduler(scheduler_tick);
}

uint32_t process_create(void (*entry)(void)) {
    struct process *p;
    uint8_t *stack;

    if (proc_count >= MAX_PROCESSES) return 0;

    /* Allocate private stack from the heap. */
    stack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!stack) return 0;

    /* Find an empty PCB slot. */
    p = find_free_slot();
    if (!p) { kfree(stack); return 0; }

    p->pid   = next_pid++;
    p->stack = stack;
    p->state = PROC_READY;
    p->esp   = build_initial_frame(entry, stack + PROC_STACK_SIZE);

    /* Insert after 'current' in the circular list. */
    p->next       = current->next;
    current->next = p;

    proc_count++;
    return p->pid;
}

uint32_t scheduler_tick(uint32_t current_esp) {
    struct process *next;

    /* Save the interrupted process's stack pointer. */
    current->esp   = current_esp;
    current->state = PROC_READY;

    /* Walk the circular list for the next READY process. */
    next = current->next;
    while (next->state != PROC_READY) {
        next = next->next;
        if (next == current) {
            /* No other ready process — stay on current. */
            current->state = PROC_RUNNING;
            return current_esp;
        }
    }

    current        = next;
    current->state = PROC_RUNNING;
    return current->esp;
}

uint32_t process_current_pid(void) {
    return current ? current->pid : 0;
}
