/*
 * vga.c — BobOS VGA Text Mode Driver Implementation
 *
 * This driver owns all text output for the kernel. It tracks the cursor,
 * handles special characters (\n, \t, \r), and scrolls the screen when
 * output reaches the bottom row. Higher-level code calls kprintf() —
 * nothing below this file should touch VGA memory directly.
 */

#include "vga.h"
#include <stdarg.h>   /* va_list, va_start, va_arg, va_end — GCC built-in, safe in -ffreestanding */

/* -------------------------------------------------------------------------
 * Driver state — not exposed outside this file.
 * ------------------------------------------------------------------------- */

static volatile unsigned char *vga_buf = (volatile unsigned char *)VGA_ADDRESS;

static int cur_row   = 0;
static int cur_col   = 0;
static unsigned char cur_color = VGA_COLOR(VGA_BLACK, VGA_WHITE);

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/* Write a character+attribute pair directly to the buffer at (row, col). */
static void buf_write(int row, int col, char c, unsigned char color) {
    int idx = (row * VGA_COLS + col) * 2;
    vga_buf[idx]     = (unsigned char)c;
    vga_buf[idx + 1] = color;
}

/*
 * Scroll the screen up by one row.
 * Every row shifts up by one; the last row is cleared.
 * This is O(rows*cols) but runs rarely and needs no memory allocator.
 */
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
    /* blank the newly exposed bottom row */
    for (col = 0; col < VGA_COLS; col++) {
        buf_write(VGA_ROWS - 1, col, ' ', cur_color);
    }
}

/* Advance the cursor by one cell, scrolling if needed. */
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

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void vga_init(unsigned char default_color) {
    cur_color = default_color;
    vga_clear();
}

void vga_set_color(unsigned char color) {
    cur_color = color;
}

void vga_clear(void) {
    int row, col;
    for (row = 0; row < VGA_ROWS; row++) {
        for (col = 0; col < VGA_COLS; col++) {
            buf_write(row, col, ' ', cur_color);
        }
    }
    cur_row = 0;
    cur_col = 0;
}

void vga_putchar(char c) {
    switch (c) {
        case '\n':
            cur_col = 0;
            cur_row++;
            if (cur_row >= VGA_ROWS) {
                scroll();
                cur_row = VGA_ROWS - 1;
            }
            break;
        case '\r':
            cur_col = 0;
            break;
        case '\t':
            /* advance to the next 8-column boundary */
            do {
                buf_write(cur_row, cur_col, ' ', cur_color);
                advance_cursor();
            } while (cur_col % 8 != 0);
            break;
        default:
            buf_write(cur_row, cur_col, c, cur_color);
            advance_cursor();
            break;
    }
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/* -------------------------------------------------------------------------
 * kprintf internals — integer formatting without any standard library
 * ------------------------------------------------------------------------- */

/* Print an unsigned integer in the given base (10 or 16) using digits[]. */
static void print_uint(unsigned int n, unsigned int base, const char *digits) {
    char buf[16];   /* max 10 decimal digits or 8 hex digits for a 32-bit value */
    int  i = 0;

    if (n == 0) {
        vga_putchar('0');
        return;
    }

    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }

    /* digits were built in reverse order */
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

/* Print a signed decimal integer. */
static void print_int(int n) {
    if (n < 0) {
        vga_putchar('-');
        /* cast to unsigned to handle INT_MIN correctly */
        print_uint((unsigned int)(-n), 10, "0123456789");
    } else {
        print_uint((unsigned int)n, 10, "0123456789");
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            vga_putchar(*fmt++);
            continue;
        }

        fmt++;  /* skip '%' */

        switch (*fmt) {
            case 'c':
                vga_putchar((char)va_arg(args, int));
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                vga_puts(s ? s : "(null)");
                break;
            }
            case 'd':
                print_int(va_arg(args, int));
                break;
            case 'u':
                print_uint(va_arg(args, unsigned int), 10, "0123456789");
                break;
            case 'x':
                print_uint(va_arg(args, unsigned int), 16, "0123456789abcdef");
                break;
            case 'X':
                print_uint(va_arg(args, unsigned int), 16, "0123456789ABCDEF");
                break;
            case '%':
                vga_putchar('%');
                break;
            default:
                /* unknown specifier — print it literally */
                vga_putchar('%');
                vga_putchar(*fmt);
                break;
        }
        fmt++;
    }

    va_end(args);
}
