/*
 * errno.h — POSIX-standard error codes for BobOS
 *
 * Each constant matches the Linux/POSIX value so that code written against
 * this header is source-compatible with standard C error handling.
 * The thread-local errno variable is defined in posix.c.
 */

#ifndef ERRNO_H
#define ERRNO_H

/* errno is per-process (one global is fine on a single-CPU OS without
   true shared libraries — a real implementation uses TLS via FS segment) */
extern int errno;

#define EPERM    1   /* Operation not permitted         */
#define ENOENT   2   /* No such file or directory       */
#define ESRCH    3   /* No such process                 */
#define EINTR    4   /* Interrupted system call         */
#define EIO      5   /* I/O error                       */
#define EBADF    9   /* Bad file descriptor             */
#define ENOMEM  12   /* Out of memory                   */
#define EACCES  13   /* Permission denied               */
#define EFAULT  14   /* Bad address                     */
#define EBUSY   16   /* Device or resource busy         */
#define EEXIST  17   /* File exists                     */
#define ENODEV  19   /* No such device                  */
#define EINVAL  22   /* Invalid argument                */
#define EMFILE  24   /* Too many open files             */
#define ENOSPC  28   /* No space left on device         */
#define EPIPE   32   /* Broken pipe                     */
#define ENOSYS  38   /* Function not implemented        */
#define EAFNOSUPPORT 47 /* Address family not supported */
#define EAGAIN  11   /* Try again (would block)         */

#endif /* ERRNO_H */
