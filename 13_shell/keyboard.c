/*
 * keyboard.c — PS/2 Keyboard Driver (IRQ1, Set 1 scancodes)
 *
 * Flow on each keypress:
 *   Hardware asserts IRQ1
 *   → PIC delivers vector 33 to the CPU
 *   → isr_common_stub saves registers, calls interrupt_handler()
 *   → interrupt_handler() sends EOI to PIC, calls keyboard_irq_handler()
 *   → keyboard_irq_handler() reads port 0x60, looks up ASCII, pushes to ring
 *   → kernel main loop calls keyboard_getchar(), pops from ring, echoes char
 *
 * Ring buffer:
 *   A fixed-size circular buffer shared between the IRQ handler (writer) and
 *   the main loop (reader).  Both head and tail are 'volatile' to prevent the
 *   compiler from caching them in registers across the interrupt boundary.
 *   The buffer is "full" when (head + 1) % SIZE == tail.
 *   The buffer is "empty" when head == tail.
 */

#include "keyboard.h"
#include "isr.h"
#include "io.h"

/* -------------------------------------------------------------------------
 * I/O port
 * ------------------------------------------------------------------------- */
#define KB_DATA_PORT  0x60   /* read scancode from here after IRQ1 fires */

/* -------------------------------------------------------------------------
 * Ring buffer — written by IRQ handler, read by main loop.
 * Power-of-2 size so the modulo reduces to a bitwise AND.
 * ------------------------------------------------------------------------- */
#define KB_BUF_SIZE  256u

static volatile char     kb_buf[KB_BUF_SIZE];
static volatile uint32_t kb_head = 0;   /* next write position */
static volatile uint32_t kb_tail = 0;   /* next read  position */

static void buf_push(char c) {
    uint32_t next = (kb_head + 1u) & (KB_BUF_SIZE - 1u);
    if (next != kb_tail) {          /* drop silently if full */
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

static char buf_pop(void) {
    if (kb_head == kb_tail) return 0;   /* empty */
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1u) & (KB_BUF_SIZE - 1u);
    return c;
}

/* -------------------------------------------------------------------------
 * Scancode → ASCII lookup tables (PS/2 Set 1, US QWERTY)
 *
 * Index = scancode (0x00–0x57).  Value = ASCII character.
 * 0 means "no printable character" (modifier keys, function keys, etc.).
 * ------------------------------------------------------------------------- */
static const char sc_normal[88] = {
/*00*/  0,
/*01*/  0x1B,  /* ESC        */
/*02*/  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
/*0E*/  '\b',  /* Backspace  */
/*0F*/  '\t',  /* Tab        */
/*10*/  'q','w','e','r','t','y','u','i','o','p','[',']',
/*1C*/  '\n',  /* Enter      */
/*1D*/  0,     /* Left Ctrl  */
/*1E*/  'a','s','d','f','g','h','j','k','l',';','\'','`',
/*2A*/  0,     /* Left Shift */
/*2B*/  '\\',
/*2C*/  'z','x','c','v','b','n','m',',','.','/',
/*36*/  0,     /* Right Shift*/
/*37*/  '*',   /* Keypad *   */
/*38*/  0,     /* Left Alt   */
/*39*/  ' ',   /* Space      */
/*3A*/  0,     /* Caps Lock  */
/*3B–3F*/  0,0,0,0,0,   /* F1–F5      */
/*40–44*/  0,0,0,0,0,   /* F6–F10     */
/*45*/  0,     /* Num Lock   */
/*46*/  0,     /* Scroll Lock*/
/*47*/  '7',   /* Keypad 7   */
/*48*/  '8',   /* Keypad 8   */
/*49*/  '9',   /* Keypad 9   */
/*4A*/  '-',   /* Keypad -   */
/*4B*/  '4',   /* Keypad 4   */
/*4C*/  '5',   /* Keypad 5   */
/*4D*/  '6',   /* Keypad 6   */
/*4E*/  '+',   /* Keypad +   */
/*4F*/  '1',   /* Keypad 1   */
/*50*/  '2',   /* Keypad 2   */
/*51*/  '3',   /* Keypad 3   */
/*52*/  '0',   /* Keypad 0   */
/*53*/  '.',   /* Keypad .   */
/*54–57*/  0,0,0,0
};

static const char sc_shift[88] = {
/*00*/  0,
/*01*/  0x1B,
/*02*/  '!','@','#','$','%','^','&','*','(',')','_','+',
/*0E*/  '\b',
/*0F*/  '\t',
/*10*/  'Q','W','E','R','T','Y','U','I','O','P','{','}',
/*1C*/  '\n',
/*1D*/  0,
/*1E*/  'A','S','D','F','G','H','J','K','L',':','"','~',
/*2A*/  0,
/*2B*/  '|',
/*2C*/  'Z','X','C','V','B','N','M','<','>','?',
/*36*/  0,
/*37*/  '*',
/*38*/  0,
/*39*/  ' ',
/*3A*/  0,
/*3B–3F*/  0,0,0,0,0,
/*40–44*/  0,0,0,0,0,
/*45*/  0,
/*46*/  0,
/*47–53*/  '7','8','9','-','4','5','6','+','1','2','3','0','.',
/*54–57*/  0,0,0,0
};

/* -------------------------------------------------------------------------
 * Shift state
 * ------------------------------------------------------------------------- */
static volatile int shift_held = 0;

#define SC_LSHIFT_MAKE   0x2A
#define SC_RSHIFT_MAKE   0x36
#define SC_LSHIFT_BREAK  0xAA   /* 0x2A | 0x80 */
#define SC_RSHIFT_BREAK  0xB6   /* 0x36 | 0x80 */

/* -------------------------------------------------------------------------
 * IRQ1 handler — called from the generic interrupt dispatcher.
 * ------------------------------------------------------------------------- */
static void keyboard_irq_handler(struct interrupt_frame *frame) {
    (void)frame;

    uint8_t sc = inb(KB_DATA_PORT);

    /* Track shift key state */
    if (sc == SC_LSHIFT_MAKE || sc == SC_RSHIFT_MAKE) { shift_held = 1; return; }
    if (sc == SC_LSHIFT_BREAK || sc == SC_RSHIFT_BREAK) { shift_held = 0; return; }

    /* Ignore break codes (bit 7 set) for all other keys */
    if (sc & 0x80) return;

    /* Ignore scancodes beyond our table */
    if (sc >= 88) return;

    char c = shift_held ? sc_shift[sc] : sc_normal[sc];
    if (c != 0) {
        buf_push(c);
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
void keyboard_init(void) {
    irq_register(1, keyboard_irq_handler);
}

char keyboard_getchar(void) {
    return buf_pop();
}

int keyboard_pending(void) {
    return kb_head != kb_tail;
}
