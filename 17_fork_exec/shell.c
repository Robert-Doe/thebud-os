/*
 * shell.c — BobOS interactive shell implementation
 *
 * shell_main() runs as a kernel process (scheduled by the round-robin
 * scheduler).  It busy-polls keyboard_getchar() and only does meaningful work
 * when a character arrives.  Between characters the process yields via
 * sys_yield() so other processes (and the idle loop) can run.
 *
 * Input is collected into a static line buffer.  On Enter, the buffer is
 * parsed: the first space-delimited token is the command, the rest is
 * the argument string.
 */

#include <stdint.h>
#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "process.h"
#include "syscall.h"

/* ── Configuration ───────────────────────────────────────────────────── */
#define LINE_MAX   128u
#define READ_MAX   (FS_MAX_FILE_SIZE)

/* ── Static buffers (never on the stack) ────────────────────────────── */
static char line_buf[LINE_MAX];
static char read_buf[READ_MAX + 1u];
static uint32_t line_len;

/* ── Tiny string helpers (no libc) ──────────────────────────────────── */

static uint32_t sh_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static int sh_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int sh_strncmp(const char *a, const char *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (!a[i]) break;
    }
    return 0;
}

/* ── fs_list callback ────────────────────────────────────────────────── */

static void list_cb(const char *name, uint32_t size) {
    kprintf("  %-20s  %u bytes\n", name, size);
}

/* ── Command handlers ────────────────────────────────────────────────── */

static void cmd_help(void) {
    kprintf("Commands:\n");
    kprintf("  help            -- show this list\n");
    kprintf("  clear           -- clear the screen\n");
    kprintf("  echo <text>     -- print text\n");
    kprintf("  pid             -- print shell PID\n");
    kprintf("  ls              -- list files\n");
    kprintf("  cat <name>      -- print a file\n");
    kprintf("  write <n> <t>   -- create file <n> with text <t>\n");
    kprintf("  rm <name>       -- delete a file\n");
    kprintf("  reboot          -- hard reboot\n");
}

static void cmd_echo(const char *arg) {
    kprintf("%s\n", arg ? arg : "");
}

static void cmd_pid(void) {
    kprintf("PID: %u\n", process_current_pid());
}

static void cmd_ls(void) {
    uint32_t n = fs_num_files();
    kprintf("%u file(s):\n", n);
    if (n) fs_list(list_cb);
}

static void cmd_cat(const char *name) {
    int r;
    if (!name || !name[0]) { kprintf("usage: cat <name>\n"); return; }
    r = fs_read(name, read_buf, READ_MAX);
    if (r < 0) { kprintf("cat: not found: %s\n", name); return; }
    read_buf[r] = '\0';
    kprintf("%s", read_buf);
    if (r > 0 && read_buf[r - 1] != '\n') kprintf("\n");
}

static void cmd_write(const char *args) {
    /* args = "<name> <text...>" */
    const char *p = args;
    uint32_t i;
    char name[FS_NAME_LEN];
    const char *text;
    int r;

    if (!p || !p[0]) { kprintf("usage: write <name> <text>\n"); return; }

    /* Extract name (up to first space) */
    for (i = 0; i < FS_NAME_LEN - 1u && p[i] && p[i] != ' '; i++)
        name[i] = p[i];
    name[i] = '\0';

    /* Skip space */
    text = p + i;
    while (*text == ' ') text++;

    if (!text[0]) { kprintf("usage: write <name> <text>\n"); return; }

    r = fs_create(name, text, sh_strlen(text));
    if (r == 0)   kprintf("created: %s\n", name);
    else if (r == -2) kprintf("write: already exists: %s\n", name);
    else          kprintf("write: error %d\n", r);
}

static void cmd_rm(const char *name) {
    int r;
    if (!name || !name[0]) { kprintf("usage: rm <name>\n"); return; }
    r = fs_delete(name);
    if (r == 0) kprintf("deleted: %s\n", name);
    else        kprintf("rm: not found: %s\n", name);
}

static void cmd_reboot(void) {
    /* Triple-fault: load an invalid IDT then trigger a divide-by-zero. */
    kprintf("Rebooting...\n");
    __asm__ volatile (
        "lidt (0)\n\t"
        "int $0\n\t"
    );
}

/* ── Line parser ─────────────────────────────────────────────────────── */

static void dispatch(char *buf) {
    uint32_t i;
    char *cmd = buf;
    char *arg = (char *)0;

    /* Find end of command token */
    for (i = 0; buf[i] && buf[i] != ' '; i++) {}
    if (buf[i] == ' ') {
        buf[i] = '\0';
        arg = buf + i + 1;
        while (*arg == ' ') arg++;
        if (!*arg) arg = (char *)0;
    }

    if      (sh_strcmp(cmd, "help")   == 0) cmd_help();
    else if (sh_strcmp(cmd, "clear")  == 0) vga_clear();
    else if (sh_strcmp(cmd, "echo")   == 0) cmd_echo(arg);
    else if (sh_strcmp(cmd, "pid")    == 0) cmd_pid();
    else if (sh_strcmp(cmd, "ls")     == 0) cmd_ls();
    else if (sh_strcmp(cmd, "cat")    == 0) cmd_cat(arg);
    else if (sh_strcmp(cmd, "write")  == 0) cmd_write(arg);
    else if (sh_strcmp(cmd, "rm")     == 0) cmd_rm(arg);
    else if (sh_strcmp(cmd, "reboot") == 0) cmd_reboot();
    else if (cmd[0] != '\0')
        kprintf("unknown command: %s (type 'help')\n", cmd);

    /* Suppress "unused variable" warning for strncmp which we included
       but could remove — kept for potential future prefix matching. */
    (void)sh_strncmp;
}

/* ── Prompt ──────────────────────────────────────────────────────────── */

static void print_prompt(void) {
    kprintf("\nbob@BobOS> ");
}

/* ── Shell entry point ───────────────────────────────────────────────── */

void shell_main(void) {
    char c;
    line_len = 0;
    line_buf[0] = '\0';

    kprintf("[shell] BobOS shell started. Type 'help' for commands.\n");
    print_prompt();

    for (;;) {
        /* Yield to other processes while waiting for input. */
        sys_yield();

        c = keyboard_getchar();
        if (!c) continue;

        if (c == '\n' || c == '\r') {
            /* Enter: execute the line */
            kprintf("\n");
            line_buf[line_len] = '\0';
            if (line_len > 0) dispatch(line_buf);
            line_len = 0;
            print_prompt();
        } else if (c == '\b') {
            /* Backspace */
            if (line_len > 0) {
                line_len--;
                kprintf("\b \b");
            }
        } else if (line_len < LINE_MAX - 1u) {
            /* Printable character */
            line_buf[line_len++] = c;
            vga_putchar(c);
        }
    }
}
