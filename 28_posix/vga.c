/*
 * vga.c — BobOS VGA Text Mode Driver Implementation
 *
 * Updated in Module 9: added '\b' (backspace) handling in vga_putchar().
 * Backspace moves the cursor one column to the left (stops at column 0).
 * It does NOT erase the character — the caller writes a space then '\b'
 * again to produce a visible erase (see vga_erase_last() in kernel.c).
 */

#include "vga.h"
#include <stdarg.h>

static volatile unsigned char *vga_buf = (volatile unsigned char *)VGA_ADDRESS;

static int cur_row   = 0;
static int cur_col   = 0;
static unsigned char cur_color = VGA_COLOR(VGA_BLACK, VGA_WHITE);

static void buf_write(int row, int col, char c, unsigned char color) {
    int idx = (row * VGA_COLS + col) * 2;
    vga_buf[idx]     = (unsigned char)c;
    vga_buf[idx + 1] = color;
}

static void scroll(void) {
    int row, col;
    for (row = 0; row < VGA_ROWS - 1; row++) {
        for (col = 0; col < VGA_COLS; col++) {
            int dst = (row * VGA_COLS + col) * 2;
            int src = ((row + 1) * VGA_COLS + col) * 2;
            vga_buf[dst]     = vga_buf[src];
            vga_buf[dst + 1] = vga_buf[src + 1];
        }
    }
    for (col = 0; col < VGA_COLS; col++) {
        buf_write(VGA_ROWS - 1, col, ' ', cur_color);
    }
}

static void advance_cursor(void) {
    cur_col++;
    if (cur_col >= VGA_COLS) {
        cur_col = 0;
        cur_row++;
    }
    if (cur_row >= VGA_ROWS) {
        scroll();
        cur_row = VGA_ROWS - 1;
    }
}

void vga_init(unsigned char default_color) {
    cur_color = default_color;
    vga_clear();
}

void vga_set_color(unsigned char color) {
    cur_color = color;
}

void vga_clear(void) {
    int row, col;
    for (row = 0; row < VGA_ROWS; row++)
        for (col = 0; col < VGA_COLS; col++)
            buf_write(row, col, ' ', cur_color);
    cur_row = 0;
    cur_col = 0;
}

void vga_putchar(char c) {
    switch (c) {
        case '\n':
            cur_col = 0;
            cur_row++;
            if (cur_row >= VGA_ROWS) { scroll(); cur_row = VGA_ROWS - 1; }
            break;
        case '\r':
            cur_col = 0;
            break;
        case '\t':
            do {
                buf_write(cur_row, cur_col, ' ', cur_color);
                advance_cursor();
            } while (cur_col % 8 != 0);
            break;
        case '\b':
            /* Move cursor one column left; stop at column 0. */
            if (cur_col > 0) {
                cur_col--;
            } else if (cur_row > 0) {
                /* Wrap back to end of previous row */
                cur_row--;
                cur_col = VGA_COLS - 1;
            }
            break;
        default:
            buf_write(cur_row, cur_col, c, cur_color);
            advance_cursor();
            break;
    }
}

void vga_puts(const char *str) {
    while (*str) vga_putchar(*str++);
}

static void print_uint(unsigned int n, unsigned int base, const char *digits) {
    char buf[16];
    int i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n > 0) { buf[i++] = digits[n % base]; n /= base; }
    while (i > 0) vga_putchar(buf[--i]);
}

static void print_int(int n) {
    if (n < 0) { vga_putchar('-'); print_uint((unsigned int)(-n), 10, "0123456789"); }
    else        print_uint((unsigned int)n, 10, "0123456789");
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt != '%') { vga_putchar(*fmt++); continue; }
        fmt++;
        switch (*fmt) {
            case 'c': vga_putchar((char)va_arg(args, int)); break;
            case 's': { const char *s = va_arg(args, const char *);
                        vga_puts(s ? s : "(null)"); break; }
            case 'd': print_int(va_arg(args, int)); break;
            case 'u': print_uint(va_arg(args, unsigned int), 10, "0123456789"); break;
            case 'x': print_uint(va_arg(args, unsigned int), 16, "0123456789abcdef"); break;
            case 'X': print_uint(va_arg(args, unsigned int), 16, "0123456789ABCDEF"); break;
            case '%': vga_putchar('%'); break;
            default:  vga_putchar('%'); vga_putchar(*fmt); break;
        }
        fmt++;
    }
    va_end(args);
}
