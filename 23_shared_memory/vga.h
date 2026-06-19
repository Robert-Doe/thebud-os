/*
 * vga.h — BobOS VGA Text Mode Driver Interface
 *
 * Public API for all text output from the kernel. Every other component that
 * wants to print anything includes this header and calls kprintf().
 */

#ifndef VGA_H
#define VGA_H

/* Physical base address of the VGA text buffer — hardwired into the x86 PC spec */
#define VGA_ADDRESS  0xB8000
#define VGA_COLS     80
#define VGA_ROWS     25

/*
 * VGA color constants.
 * Each color occupies one nibble (4 bits). The attribute byte is:
 *   bits 7-4 = background color
 *   bits 3-0 = foreground color
 */
#define VGA_BLACK         0x0
#define VGA_BLUE          0x1
#define VGA_GREEN         0x2
#define VGA_CYAN          0x3
#define VGA_RED           0x4
#define VGA_MAGENTA       0x5
#define VGA_BROWN         0x6
#define VGA_LIGHT_GREY    0x7
#define VGA_DARK_GREY     0x8
#define VGA_LIGHT_BLUE    0x9
#define VGA_LIGHT_GREEN   0xA
#define VGA_LIGHT_CYAN    0xB
#define VGA_LIGHT_RED     0xC
#define VGA_LIGHT_MAGENTA 0xD
#define VGA_YELLOW        0xE
#define VGA_WHITE         0xF

/* Build an attribute byte from background and foreground color constants */
#define VGA_COLOR(bg, fg) (((unsigned char)(bg) << 4) | (unsigned char)(fg))

/*
 * vga_init — Set up the driver and clear the screen.
 * Must be called once before any other vga_* or kprintf call.
 *
 * default_color: the attribute byte used for all output until vga_set_color()
 *                is called. Example: VGA_COLOR(VGA_BLACK, VGA_WHITE)
 */
void vga_init(unsigned char default_color);

/*
 * vga_set_color — Change the color used for subsequent output.
 * Does not affect characters already on screen.
 */
void vga_set_color(unsigned char color);

/*
 * vga_clear — Erase the entire screen and reset the cursor to (0, 0).
 * Fills with spaces in the current color.
 */
void vga_clear(void);

/*
 * vga_putchar — Write one character at the cursor and advance it.
 * Handles:
 *   '\n'  — move to the start of the next row
 *   '\t'  — advance to the next 8-column tab stop
 *   '\r'  — move to column 0 of the current row
 * Automatically scrolls when the cursor moves past the last row.
 */
void vga_putchar(char c);

/*
 * vga_puts — Write a null-terminated string using the current color.
 */
void vga_puts(const char *str);

/*
 * kprintf — Minimal kernel printf.
 * Supported format specifiers:
 *   %c   — single character
 *   %s   — null-terminated string
 *   %d   — signed decimal integer
 *   %u   — unsigned decimal integer
 *   %x   — unsigned hexadecimal (lowercase, no leading "0x")
 *   %X   — unsigned hexadecimal (uppercase)
 *   %%   — literal percent sign
 *
 * Width and precision are NOT supported — this is a bare-metal kernel.
 * Uses the current color set by vga_set_color().
 */
void kprintf(const char *fmt, ...);

#endif /* VGA_H */
