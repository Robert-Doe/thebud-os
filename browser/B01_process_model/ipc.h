#ifndef IPC_H
#define IPC_H

#include <unistd.h>

/* Maximum message size over the pipe */
#define IPC_MSG_MAX 256

/* IPC channel: a pair of unidirectional pipes.
 * pipe_req: caller writes requests,  callee reads.
 * pipe_resp: callee writes responses, caller reads. */
typedef struct {
    int pipe_req[2];   /* [0]=read end, [1]=write end */
    int pipe_resp[2];
} ipc_channel_t;

/* Create both pipes.  Returns 0 on success, -1 on error. */
int  ipc_open(ipc_channel_t *ch);

/* Parent side: close the child's ends after fork(). */
void ipc_parent_setup(ipc_channel_t *ch);

/* Child side: close the parent's ends after fork(). */
void ipc_child_setup(ipc_channel_t *ch);

/* Send a null-terminated message string. */
int  ipc_send(int write_fd, const char *msg);

/* Receive a message into buf (max len bytes). Returns bytes read or -1. */
int  ipc_recv(int read_fd, char *buf, int len);

#endif /* IPC_H */
