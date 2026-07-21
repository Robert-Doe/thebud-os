#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int ipc_open(ipc_channel_t *ch)
{
    if (pipe(ch->pipe_req)  < 0) return -1;
    if (pipe(ch->pipe_resp) < 0) return -1;
    return 0;
}

/* Parent keeps: write end of req, read end of resp */
void ipc_parent_setup(ipc_channel_t *ch)
{
    close(ch->pipe_req[0]);   /* child reads requests  */
    close(ch->pipe_resp[1]);  /* child writes responses */
}

/* Child keeps: read end of req, write end of resp */
void ipc_child_setup(ipc_channel_t *ch)
{
    close(ch->pipe_req[1]);   /* parent writes requests  */
    close(ch->pipe_resp[0]);  /* parent reads responses  */
}

int ipc_send(int write_fd, const char *msg)
{
    int len = (int)strlen(msg);
    return (int)write(write_fd, msg, (size_t)len);
}

int ipc_recv(int read_fd, char *buf, int len)
{
    int n = (int)read(read_fd, buf, (size_t)(len - 1));
    if (n > 0) buf[n] = '\0';
    return n;
}
