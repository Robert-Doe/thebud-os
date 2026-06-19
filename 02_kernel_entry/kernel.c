/*
 * kernel.c — BobOS First Kernel
 *
 * This is the first C code that runs on our OS. There is no printf(),
 * no standard library, no OS beneath us — just the CPU, RAM, and hardware.
 *
 * To prove the kernel loaded and ran, we write directly to VGA text memory.
 *
 * VGA Text Mode:
 *   When the BIOS initialized video, it set the display to VGA text mode 3.
 *   In this mode, the screen is 80 columns × 25 rows of characters.
 *   Each character on screen is represented by 2 bytes in memory starting
 *   at physical address 0xB8000:
 *
 *     Byte 0: ASCII character code
 *     Byte 1: Attribute byte — high nibble = background color, low nibble = foreground color
 *
 *   Color values:
 *     0x0 = Black    0x4 = Red      0x8 = Dark grey    0xC = Light red
 *     0x1 = Blue     0x5 = Magenta  0x9 = Light blue   0xD = Light magenta
 *     0x2 = Green    0x6 = Brown    0xA = Light green   0xE = Yellow
 *     0x3 = Cyan     0x7 = Grey     0xB = Light cyan    0xF = White
 *
 *   So to write a white 'A' on a black background at row 0, col 0:
 *     0xB8000[0] = 'A'   (ASCII 65)
 *     0xB8000[1] = 0x0F  (black background = 0x0, white foreground = 0xF)
 *
 * We are doing this completely without any library — just raw pointer arithmetic.
 */

/* VGA text buffer starts at this physical address — always, on all x86 PCs */
#define VGA_ADDRESS 0xB8000

/* Screen dimensions */
#define VGA_COLS 80
#define VGA_ROWS 25

/* Color constants — attribute byte format: (background << 4) | foreground */
#define COLOR_BLACK  0x0
#define COLOR_GREEN  0x2
#define COLOR_WHITE  0xF

#define MAKE_COLOR(bg, fg) ((bg << 4) | fg)

/* A pointer to the VGA buffer. 'volatile' tells the C compiler:
 * "do NOT optimize away writes to this pointer — the hardware actually reads it."
 * Without volatile, the compiler might see "you write but never read" and delete
 * the writes entirely. Volatile prevents that. */
volatile unsigned char *vga = (volatile unsigned char *)VGA_ADDRESS;

/* Write a single character to the VGA buffer at a given row and column */
void vga_putchar(int row, int col, char c, unsigned char color) {
    /* Each cell is 2 bytes. Position = (row * 80 + col) * 2 */
    int index = (row * VGA_COLS + col) * 2;
    vga[index]     = c;      /* the character */
    vga[index + 1] = color;  /* the color attribute */
}

/* Write a null-terminated string starting at (row, col) */
void vga_print(int row, int col, const char *str, unsigned char color) {
    int c = col;
    while (*str) {
        vga_putchar(row, c, *str, color);
        str++;
        c++;
    }
}

/* Clear the entire screen by filling every cell with spaces */
void vga_clear(unsigned char color) {
    for (int row = 0; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            vga_putchar(row, col, ' ', color);
        }
    }
}

/*
 * kernel_main — the C entry point of our kernel
 *
 * Called by kernel_entry.asm after Protected Mode is fully set up.
 * This function should never return.
 */
void kernel_main(void) {
    unsigned char white_on_black = MAKE_COLOR(COLOR_BLACK, COLOR_WHITE);
    unsigned char green_on_black = MAKE_COLOR(COLOR_BLACK, COLOR_GREEN);

    /* Clear screen to a clean black background */
    vga_clear(white_on_black);

    /* Print our kernel banner */
    vga_print(0, 0, "============================================================", white_on_black);
    vga_print(1, 0, "  BobOS Kernel — v0.1", green_on_black);
    vga_print(2, 0, "  Running in 32-bit Protected Mode", green_on_black);
    vga_print(3, 0, "  CPU: x86  |  Mode: Protected  |  Ring: 0 (Kernel)", green_on_black);
    vga_print(4, 0, "============================================================", white_on_black);
    vga_print(6, 0, "  Kernel loaded successfully from disk.", white_on_black);
    vga_print(7, 0, "  GDT configured. Segments: flat 0x0 -> 0xFFFFFFFF (4GB).", white_on_black);
    vga_print(8, 0, "  Next: VGA driver, then GDT, then interrupts...", white_on_black);
    vga_print(10, 0, "  Halting. Waiting for next component.", white_on_black);

    /* Hang forever — the kernel has nothing else to do yet */
    for (;;) {}
}
