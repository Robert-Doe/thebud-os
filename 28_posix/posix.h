/*
 * posix.h — POSIX-compatible wrappers over BobOS syscalls
 *
 * This header exposes a subset of the POSIX.1-2017 interface:
 *   - errno-setting wrappers for all existing syscalls
 *   - BSD socket API: socket(), bind(), sendto(), recvfrom()
 *   - select() for multiplexed I/O wait
 *   - stat() for file metadata
 *   - lseek() for seekable file descriptors
 *
 * Programs that include only this header (and errno.h) can be
 * cross-compiled for BobOS with minimal source changes.
 */

#ifndef POSIX_H
#define POSIX_H

#include <stdint.h>
#include "errno.h"
#include "syscall.h"

/* ── Types ──────────────────────────────────────────────────────────── */

typedef uint32_t  off_t;
typedef uint32_t  size_t;
typedef int32_t   ssize_t;
typedef uint32_t  mode_t;
typedef uint32_t  ino_t;

/* stat structure (POSIX subset) */
struct stat {
    ino_t    st_ino;    /* inode number */
    mode_t   st_mode;   /* file type and permissions */
    uint32_t st_size;   /* file size in bytes */
    uint32_t st_blksize;
    uint32_t st_blocks;
};

/* S_IFMT bitmask constants */
#define S_IFMT   0170000
#define S_IFREG  0100000   /* regular file */
#define S_IFDIR  0040000   /* directory    */
#define S_IFCHR  0020000   /* char device  */
#define S_IFBLK  0060000   /* block device */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)

/* lseek whence values */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* open flags */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0x40

/* select fd_set (simplified: supports up to 32 fds) */
typedef struct { uint32_t bits; } fd_set;
#define FD_ZERO(s)     ((s)->bits = 0)
#define FD_SET(fd,s)   ((s)->bits |=  (1u << (fd)))
#define FD_CLR(fd,s)   ((s)->bits &= ~(1u << (fd)))
#define FD_ISSET(fd,s) (((s)->bits >> (fd)) & 1u)

/* Socket address family */
#define AF_INET  2
#define SOCK_DGRAM 2

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;    /* network byte order */
    uint32_t sin_addr;    /* network byte order */
    uint8_t  sin_zero[8];
};

/* ── POSIX function declarations ────────────────────────────────────── */

void  posix_init(void);

/* File I/O */
int     open  (const char *path, int flags);
ssize_t read  (int fd, void *buf, size_t n);
ssize_t write (int fd, const void *buf, size_t n);
int     close (int fd);
off_t   lseek (int fd, off_t offset, int whence);
int     fstat (int fd, struct stat *st);

/* Process */
int   getpid (void);
void  exit   (int code);
int   fork   (void);
int   waitpid(int pid, int *status, int options);

/* Signals */
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
int  kill(int pid, int sig);

/* Sockets */
int     socket  (int domain, int type, int protocol);
int     bind    (int sockfd, const struct sockaddr_in *addr, uint32_t addrlen);
ssize_t sendto  (int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr_in *dest, uint32_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr_in *src, uint32_t *addrlen);

/* Multiplexed wait */
struct timeval { uint32_t tv_sec; uint32_t tv_usec; };
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);

/* Memory */
void *malloc(size_t size);
void  free  (void *ptr);

#endif /* POSIX_H */
