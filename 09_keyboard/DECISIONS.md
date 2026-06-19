# Component 9: Keyboard Driver — Deep Dive

---

## 1. Where We Left Off

Components 1–8 built a kernel that initialises hardware and allocates memory —
but it cannot interact with a user. Every module so far has been a one-shot demo
that prints results and halts. Component 9 changes that: for the first time,
**BobOS responds to user input** in real time.

The keyboard driver has three jobs:
1. Receive IRQ1 from the hardware every time a key is pressed.
2. Translate the raw hardware code into a human-readable ASCII character.
3. Buffer the character so the rest of the kernel can read it at its own pace.

---

## 2. How the PS/2 Keyboard Works at the Hardware Level

The PC keyboard is connected to the CPU via the **PS/2 keyboard controller**
(also called the 8042 chip, after the original Intel part number).

When a key is pressed:
1. The keycap's electrical contact closes.
2. The keyboard's internal microcontroller detects the press and encodes it as
   a **scancode** byte.
3. The keyboard sends the scancode over the PS/2 serial wire to the 8042.
4. The 8042 stores the byte in its **output buffer** (I/O port 0x60) and
   **asserts IRQ1**, pulling the interrupt line high.
5. The PIC sees IRQ1 and delivers it to the CPU as vector 33 (after our
   PIC remapping from Module 5).
6. The CPU calls our interrupt handler, which reads port 0x60 to retrieve the
   scancode and acknowledge the hardware.

Reading port 0x60 is both the data retrieval AND the acknowledgement — the 8042
lowers the interrupt line as soon as its output buffer is read.

---

## 3. Scancodes — Make and Break Codes

The keyboard does not send ASCII; it sends **scancodes** — hardware-specific codes
for the physical key positions, completely independent of any character encoding.

**Scancode Set 1** (the default on QEMU and BIOS-initialised real keyboards):

| Event      | Byte emitted         |
|------------|----------------------|
| Key press  | **Make code** (bit 7 = 0) |
| Key release| **Break code** = make code `| 0x80` (bit 7 = 1) |

Example — pressing then releasing 'A':
```
Press:   scancode 0x1E  (make)
Release: scancode 0x9E  (break = 0x1E | 0x80)
```

The driver ignores break codes (for most keys — shift is special) by checking
`if (sc & 0x80) return;`. Only make codes produce characters.

---

## 4. The Scancode → ASCII Lookup Table

The driver maintains two 88-entry tables — `sc_normal[]` and `sc_shift[]`:

```
scancode → sc_normal[scancode]   (shift not held)
scancode → sc_shift[scancode]    (shift held)
```

A table entry of `0` means "no character" (function keys, ctrl, alt, etc.).
These are silently dropped — they never enter the ring buffer.

Why 88 entries? Scancodes 0x00–0x57 cover all keys on a standard 84/88-key
AT keyboard (including the numeric keypad). Extended two-byte scancodes (0xE0
prefix, used for cursor keys, numpad enter, etc.) are not handled in Module 9 —
they arrive as 0xE0 then a second byte; since 0xE0 > 88, the table lookup is
skipped cleanly.

---

## 5. Shift Key Tracking

Left shift and right shift do not produce characters — they modify the next key.
The driver tracks shift state with a single integer:

```c
static volatile int shift_held = 0;

// Make codes for shift keys
if (sc == 0x2A || sc == 0x36) { shift_held = 1; return; }

// Break codes for shift keys
if (sc == 0xAA || sc == 0xB6) { shift_held = 0; return; }
```

After filtering shift, the translation uses:
```c
char c = shift_held ? sc_shift[sc] : sc_normal[sc];
```

This gives correct behavior: pressing `Shift+a` yields `'A'`, pressing `Shift+1`
yields `'!'`, etc. Caps Lock is not implemented — its stateful toggle requires
tracking it separately and only applying it to letters, not digits/symbols.

---

## 6. The Ring Buffer — Interrupt-Safe Character Queue

The IRQ handler (writer) and the main loop (reader) run in different contexts.
The handler can fire at any time. We need a data structure that is:
- **Lock-free** (the OS has no locks yet): a single-producer/single-consumer
  ring buffer is inherently safe without locks when the indices are power-of-2.
- **Bounded**: if the user types faster than the kernel reads, extra keys are
  dropped rather than corrupting memory.

```
       tail (read here)         head (write here)
          ↓                          ↓
  [ c ][ c ][ c ][ c ][ c ][ _ ][ _ ][ _ ]
         ←── data ──→         ←── free ──→
```

Empty condition: `head == tail`
Full condition: `(head + 1) % SIZE == tail` (one slot wasted to distinguish)

Because `KB_BUF_SIZE` is a power of 2 (256), the modulo is a bitwise AND:
```c
(head + 1) & (KB_BUF_SIZE - 1)
```
This is a single instruction — important inside an IRQ handler.

Both `kb_head` and `kb_tail` are `volatile` to prevent the compiler from
caching them in registers between the IRQ context and the main loop context.

---

## 7. Why Not Use `kmalloc` for the Buffer?

Module 8's preview said the ring buffer would be heap-allocated. In practice,
allocating a buffer inside an interrupt handler is dangerous:

- `kmalloc` scans a linked list — it is not safe to call from an interrupt
  unless interrupted code was not also inside `kmalloc` (re-entrancy problem).
- At this stage we have no interrupt masking around the heap.

The static array approach is simpler, safer, and equally illustrative. A
production OS would use a pre-allocated ring buffer (allocated during driver
init, not during the interrupt). That is exactly what we do here: the buffer
is allocated at compile time in BSS, and init time is `keyboard_init()`, not
the IRQ handler.

---

## 8. The `\b` Backspace in the VGA Driver

Prior modules only handled `\n`, `\r`, and `\t`. Module 9 adds `'\b'`
(backspace, ASCII 0x08):

```c
case '\b':
    if (cur_col > 0) cur_col--;
    else if (cur_row > 0) { cur_row--; cur_col = VGA_COLS - 1; }
    break;
```

`'\b'` only **moves the cursor** — it does not erase the character under it.
To produce a visible erase, the caller does:
```c
vga_putchar('\b');  // move left
vga_putchar(' ');   // overwrite with space
vga_putchar('\b');  // move left again (cursor now on the blank)
```
This three-step pattern is the same technique real terminals use.

---

## 9. The Interactive Echo Loop

```c
for (;;) {
    __asm__ volatile ("hlt");   // sleep until next interrupt
    char c;
    while ((c = keyboard_getchar()) != 0) {
        if (c == '\b')  vga_erase_last();
        else if (c == '\n') { vga_putchar('\n'); kprintf("> "); }
        else vga_putchar(c);
    }
}
```

`hlt` (halt) puts the CPU into a low-power sleep state until the next interrupt
arrives. When any interrupt fires (timer, keyboard), the CPU wakes, services the
interrupt handler, and then returns to the instruction after `hlt` — which is
our ring-buffer drain loop.

This is the correct pattern for an idle kernel loop: burn no CPU cycles between
events, wake exactly when there is work to do.

---

## 10. File Structure

| File           | Purpose |
|----------------|---------|
| `keyboard.h`   | Public API: `keyboard_init`, `keyboard_getchar`, `keyboard_pending` |
| `keyboard.c`   | IRQ1 handler, scancode tables, shift tracking, ring buffer |
| `vga.c`        | Updated from Module 3: added `'\b'` cursor-back handling |
| `kernel.c`     | Full init + interactive echo loop with backspace and prompt |
| All prior files| Carried forward from Module 8 unchanged |

---

## 11. Decisions Made

| Decision | Choice | Why |
|----------|--------|-----|
| Scancode set | PS/2 Set 1 | Default on QEMU and BIOS hardware; no reprogramming needed |
| Make/break | Only make codes produce characters | Break codes are release events; we only want to act on presses |
| Shift tracking | Single `shift_held` int | Sufficient for one modifier; Caps Lock would need a separate toggle flag |
| Extended scancodes (0xE0) | Silently dropped | 0xE0 > table size; no crash, no character emitted; simplest correct handling |
| Ring buffer | Static array, power-of-2 size | Lock-free single-producer/single-consumer; safe from IRQ context; no heap re-entrancy risk |
| Buffer full | Drop newest character | Prevents corruption; user typing ahead rarely matters at this stage |
| `volatile` on indices | Required | Without `volatile`, the compiler may cache `kb_head`/`kb_tail` in registers and never see the IRQ's update |
| `\b` handling | Move cursor only, no erase | Matches terminal convention; caller writes space+`\b` to visually erase |
| Idle loop | `hlt` + drain buffer | Zero CPU burn between keystrokes; wakes on any IRQ |

---

## 12. What We Proved By Getting This Working

- IRQ1 fires correctly for every key press — the PIC remapping from Module 5
  routes IRQ1 to vector 33 as expected.
- Scancodes are correctly translated: letters, digits, and punctuation appear
  as their typed characters.
- Shift key held → uppercase letters and shifted symbols.
- Backspace erases the previous character visually.
- Enter produces a newline and a fresh prompt.
- The ring buffer does not corrupt state under fast typing.
- The `hlt`-based idle loop is power-efficient and wakes instantly on input.
- The VGA driver, IRQ system, PIC, and keyboard controller cooperate correctly
  as an end-to-end interrupt-driven I/O path.

---

## 13. What Comes Next (Component 10 Preview)

Component 10 builds **Processes and the Scheduler** — the ability to run more
than one task "simultaneously" through cooperative time-slicing.

Each process has:
- Its own **stack** (allocated from the heap).
- A saved **CPU state** (register snapshot, EIP, ESP).
- A state: Running, Ready, or Blocked.

The scheduler runs on every timer tick (IRQ0). It saves the current process's
registers, picks the next ready process from a round-robin queue, restores its
registers, and returns to it. From each process's perspective, it appears to
run continuously — the context switch is invisible.

The keyboard driver (Module 9) will naturally integrate with processes: a
process that calls a future `keyboard_readline()` will block until Enter is
pressed, yielding the CPU to other processes in the meantime.
