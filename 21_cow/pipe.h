/*
 * pipe.h -- Anonymous pipe interface for BobOS
 *
 * Module 20: Kernel ring-buffer connecting two processes.
 * pipe_alloc() reserves a slot in pipe_table and installs read/write
 * file descriptors in the current process's fd table via the VFS layer.
 */

#ifndef PIPE_H
#define PIPE_H
#include <stdint.h>
#include "vfs.h"

#define PIPE_BUF  4096
#define MAX_PIPES 8

struct pipe_buf {
    char     data[PIPE_BUF];
    uint32_t head;       /* read index (mod PIPE_BUF)  */
    uint32_t tail;       /* write index                 */
    uint32_t count;      /* bytes currently available   */
    int      read_open;
    int      write_open;
    int      valid;
};

void pipe_init(void);
int  pipe_alloc(int fds[2]);  /* fills fds[0]=read end, fds[1]=write end */

extern struct vfs_ops pipe_read_ops;
extern struct vfs_ops pipe_write_ops;

#endif /* PIPE_H */
