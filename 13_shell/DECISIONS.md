# Module 13 — Shell: Design Decisions

## What this module adds

An interactive shell that runs as a kernel process (PID 2) and accepts
commands from the keyboard.  The shell composes every subsystem built in
Modules 1-12 into a single working program.

---

## Decision 1: Shell as a scheduled process, not a special kernel thread

**Two options:**
1. Run the shell directly in `kernel_main()` after initialisation — a simple
   infinite loop that reads keyboard input.
2. Spawn the shell as an ordinary process via `process_create(shell_main)` and
   let `kernel_main` become the idle process.

We chose option 2.  The scheduler already exists and is correct.  Making the
shell a normal process:

- Proves that `process_create` works for real user-facing code (not just demos).
- Means the shell can call `sys_yield()` between keystrokes, giving the idle
  process (and any future processes) fair CPU time.
- Mirrors how real OSes work: `init` is just PID 1; the shell is a child.

The idle process becomes `kernel_main`'s bare `for (;;) hlt` loop — exactly
what it should be.

---

## Decision 2: Polling keyboard with `sys_yield()` instead of blocking

A blocking `keyboard_read()` that sleeps until a character arrives would
require:
- A wait queue in the scheduler (processes that are WAITING, not READY).
- A wakeup call from the keyboard IRQ handler.

Both are real kernel features but add ~100 lines of scheduling complexity.

Instead, `shell_main` calls `sys_yield()` on every loop iteration (giving the
idle process a turn), then calls `keyboard_getchar()`.  If no character is
ready, the loop iterates again on the next scheduler tick (~55 ms).  For an
interactive shell, a 55 ms response latency is imperceptible.

This is cooperative multitasking inside a preemptive scheduler — exactly how
early Unix shells worked before proper blocking I/O.

---

## Decision 3: Static line and read buffers

The shell needs two large buffers:

- `line_buf[128]` — accumulates the current line of input.
- `read_buf[32768]` — holds a file's content for `cat`.

Placing these on the stack would require either a very large process stack or
careful accounting of call-frame sizes.  Instead, both are `static` in
`shell.c` — they live in the BSS/data segment, not on the stack.

The process stack (4 KB from `kmalloc`) is then only used for `shell_main`'s
frame and the frames of the small helper functions it calls.  No stack
overflow risk.

---

## Decision 4: Command parsing by hand (no `strtok`)

There is no C standard library.  `strtok` is off the table.

The parser is 12 lines: scan for the first space, null-terminate the command
token there, advance past whitespace to find the argument string.  This handles
all the commands we need:

```
help
echo hello world      -> cmd="echo", arg="hello world"
write file.txt hello  -> cmd="write", arg="file.txt hello"
cat notes.txt         -> cmd="cat",  arg="notes.txt"
```

The `write` handler then does a second manual parse on `arg` to split the
filename from the content.  For 9 commands this is readable and maintainable.
A general tokenizer would add complexity for no gain at this scale.

---

## Decision 5: `reboot` via triple-fault

A clean ACPI shutdown or BIOS reboot requires either:
- Writing to the ACPI PM1a control port (address varies by ACPI table).
- Using the keyboard controller reset pulse (port 0x64, command 0xFE).

The keyboard controller reset is the traditional approach, but the 8042 chip
must be in the right state first.  The simplest approach that works reliably
in QEMU is a triple-fault: load a zero-size IDT (`lidt (0)`) then trigger
any exception.  The CPU tries to dispatch vector 0, can't find a handler,
fires a double fault (#DF), tries vector 8 with a null IDT, triple-faults,
and resets.  QEMU restarts the machine.

This is not suitable for real hardware (a triple-fault may hang some systems)
but is fine as a demo reboot.

---

## Decision 6: `vga_clear()` already exists — no changes to vga.c

The shell's `clear` command calls `vga_clear()`.  This function was added to
`vga.h` / `vga.c` in an earlier module.  We do not need to modify the VGA
driver, which shows that the layered API design from Module 3 continues to pay
off — new modules consume the driver without touching it.

---

## What the `write` command teaches

```
bob@BobOS> write hello.txt This is my first file
```

This single command exercises:
- Shell input -> line buffer
- Parser -> cmd="write", arg="hello.txt This is my first file"
- `cmd_write` -> parses name and text, calls `fs_create()`
- `fs_create()` -> allocates sectors, writes via ATA PIO, updates superblock
- ATA PIO driver -> 256 word-writes to port 0x1F0

Nine modules of infrastructure, invoked by one user command.

---

## Key lessons from this module

| Concept | Takeaway |
|---|---|
| Shell as process | Ordinary processes can drive interactive UI — no special threading needed |
| Cooperative yield | `sys_yield()` in a polling loop is simpler than blocking I/O |
| Static buffers | Large per-command buffers belong in BSS, not on the stack |
| Manual parsing | For a small fixed command set, hand-written parsing is clearer than a framework |
| Triple-fault reboot | A practical (if crude) reset mechanism available with no extra drivers |
| Module composition | All 13 modules compose cleanly — each consumed the previous ones' APIs unchanged |
