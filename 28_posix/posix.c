/*
 * posix.c — POSIX-compatible wrappers over BobOS syscalls
 *
 * Each function:
 *   1. Calls the appropriate BobOS syscall
 *   2. Translates BobOS negative error codes into POSIX errno values
 *   3. Returns -1 on error (POSIX convention) instead of the negative code
 *
 * This file bridges the gap between "kernel correct" and "POSIX compatible."
 */

#include "posix.h"
#include "syscall.h"
#include "heap.h"
#include "udp.h"
#include "net.h"
#include "vga.h"

/* Per-process errno — a real implementation uses TLS (FS segment) */
int errno = 0;

/* Map BobOS syscall error codes to POSIX errno values */
static int map_err(int bobos_err) {
    switch (bobos_err) {
        case SYSCALL_EBADF:   return EBADF;
        case SYSCALL_EINVAL:  return EINVAL;
        case SYSCALL_ENOSYS:  return ENOSYS;
        case SYSCALL_ENOENT:  return ENOENT;
        case SYSCALL_ENOEXEC: return ENOENT;
        case SYSCALL_ENOMEM:  return ENOMEM;
        case SYSCALL_EAGAIN:  return EAGAIN;
        case SYSCALL_ESRCH:   return ESRCH;
        default:               return EIO;
    }
}

static int ret_or_errno(int rc) {
    if (rc < 0) { errno = map_err(rc); return -1; }
    errno = 0;
    return rc;
}

void posix_init(void) {
    errno = 0;
    vga_print("[posix] POSIX layer ready\n");
}

/* ── File I/O ─────────────────────────────────────────────────────── */

int open(const char *path, int flags) {
    return ret_or_errno(sys_open(path, flags));
}

ssize_t read(int fd, void *buf, size_t n) {
    return ret_or_errno(sys_read(fd, (char *)buf, (int)n));
}

ssize_t write(int fd, const void *buf, size_t n) {
    return ret_or_errno(sys_write(fd, (const char *)buf, (int)n));
}

int close(int fd) {
    sys_close(fd);
    errno = 0;
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    int rc = syscall(SYS_LSEEK, fd, (int)offset, whence);
    return (off_t)ret_or_errno(rc);
}

int fstat(int fd, struct stat *st) {
    int rc = syscall(SYS_STAT, fd, (int)st, 0);
    return ret_or_errno(rc);
}

/* ── Process ──────────────────────────────────────────────────────── */

int getpid(void) {
    return sys_getpid();
}

void exit(int code) {
    sys_exit(code);
    /* unreachable — sys_exit does not return */
    while (1);
}

int fork(void) {
    return ret_or_errno(sys_fork());
}

int waitpid(int pid, int *status, int options) {
    (void)pid; (void)options;
    int rc = sys_wait();
    if (status) *status = rc;
    return ret_or_errno(rc);
}

/* ── Signals ──────────────────────────────────────────────────────── */

sighandler_t signal(int signum, sighandler_t handler) {
    int rc = sys_signal(signum, handler);
    if (rc < 0) { errno = EINVAL; return (sighandler_t)-1; }
    return handler;
}

int kill(int pid, int sig) {
    return ret_or_errno(sys_kill(pid, sig));
}

/* ── Sockets ──────────────────────────────────────────────────────── */

int socket(int domain, int type, int protocol) {
    (void)protocol;
    if (domain != AF_INET || type != SOCK_DGRAM) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    /* Return a socket handle — caller must bind() before recv */
    /* We use fd 10+ as socket fds (above the normal VFS range) */
    /* Actual binding deferred to bind() */
    errno = 0;
    return 10; /* placeholder socket fd — bound in bind() */
}

int bind(int sockfd, const struct sockaddr_in *addr, uint32_t addrlen) {
    (void)sockfd; (void)addrlen;
    uint16_t port = ntohs(addr->sin_port);
    int sock_id = udp_bind(port);
    if (sock_id < 0) { errno = EADDRINUSE; return -1; }
    errno = 0;
    return 0;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr_in *dest, uint32_t addrlen) {
    (void)sockfd; (void)flags; (void)addrlen;
    uint32_t dst_ip   = ntohl(dest->sin_addr);
    uint16_t dst_port = ntohs(dest->sin_port);
    int rc = udp_send(dst_ip, 0 /* ephemeral */, dst_port,
                      (const uint8_t *)buf, (uint16_t)len);
    return ret_or_errno(rc < 0 ? rc : (int)len);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr_in *src, uint32_t *addrlen) {
    (void)flags;
    /* sock_id maps 1:1 to sockfd - 10 */
    int sock_id = sockfd - 10;
    uint32_t src_ip;
    uint16_t src_port;
    int n = udp_recvfrom(sock_id, (uint8_t *)buf, (uint16_t)len,
                         &src_ip, &src_port);
    if (n < 0)  { errno = EBADF;  return -1; }
    if (n == 0) { errno = EAGAIN; return -1; }
    if (src) {
        src->sin_family = AF_INET;
        src->sin_port   = htons(src_port);
        src->sin_addr   = htonl(src_ip);
        if (addrlen) *addrlen = sizeof(struct sockaddr_in);
    }
    errno = 0;
    return n;
}

/* ── select() ─────────────────────────────────────────────────────── */

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {
    (void)writefds; (void)exceptfds; (void)timeout;
    /* Minimal implementation: call sys_select which spins checking fds */
    if (!readfds) { errno = EINVAL; return -1; }
    int rc = syscall(SYS_SELECT, nfds, (int)readfds->bits, 0);
    if (rc < 0) { errno = EINVAL; return -1; }
    readfds->bits = (uint32_t)rc;
    errno = 0;
    return __builtin_popcount((unsigned)rc);
}

/* ── Memory ────────────────────────────────────────────────────────── */

void *malloc(size_t size) {
    void *p = kmalloc(size);
    if (!p) errno = ENOMEM;
    return p;
}

void free(void *ptr) {
    kfree(ptr);
}
