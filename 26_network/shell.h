/*
 * shell.h — BobOS interactive shell
 *
 * The shell is a kernel process (PID 2) that:
 *   1. Reads characters from the keyboard via keyboard_getchar().
 *   2. Echoes them to the VGA display.
 *   3. On Enter, parses the command and dispatches to a handler.
 *
 * Supported commands
 * ──────────────────
 *   help          — list all commands
 *   clear         — clear the screen
 *   echo <text>   — print text
 *   pid           — print the shell's PID
 *   ls            — list files on BobFS
 *   cat <name>    — print a file's contents
 *   write <n> <t> — create file <n> with text <t>
 *   rm <name>     — delete a file
 *   reboot        — triple-fault reboot (hard reset)
 */

#ifndef SHELL_H
#define SHELL_H

/* Entry point — pass to process_create() */
void shell_main(void);

#endif /* SHELL_H */
