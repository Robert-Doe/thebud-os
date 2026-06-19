/*
 * user_prog.h — the ring-3 demo process entry point
 */

#ifndef USER_PROG_H
#define USER_PROG_H

/* Entry function for the user-mode demo process.
 * Must never return — call sys_exit() at the end. */
void user_main(void);

#endif /* USER_PROG_H */
