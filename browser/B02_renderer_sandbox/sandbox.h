#ifndef SANDBOX_H
#define SANDBOX_H

/* Simulated syscall numbers */
#define SYS_READ   0   /* allowed directly */
#define SYS_WRITE  1   /* allowed to stdout (fd=1) only */
#define SYS_OPEN   2   /* mediated — forwarded to broker */
#define SYS_SOCKET 3   /* blocked */
#define SYS_EXEC   4   /* blocked */

#define SYS_COUNT  5

/* Sandbox disposition for each syscall */
typedef enum {
    DISP_ALLOW,   /* execute directly inside renderer */
    DISP_BROKER,  /* forward to broker process */
    DISP_BLOCK    /* deny with error */
} disposition_t;

/* Syscall request passed from renderer to sandbox interceptor */
typedef struct {
    int  syscall_nr;
    int  arg_fd;          /* for SYS_WRITE / SYS_READ  */
    char arg_path[128];   /* for SYS_OPEN              */
    char arg_buf[128];    /* for SYS_WRITE data        */
    int  arg_len;
} syscall_req_t;

/* Result returned to renderer */
typedef struct {
    int  ok;          /* 1 = success, 0 = error */
    char data[128];   /* result data or error message */
} syscall_result_t;

/* Initialise the whitelist table */
void sandbox_init(void);

/* Intercept a syscall.  May execute it, forward to broker, or block.
 * broker_req_fd / broker_resp_fd: IPC to the broker (used for DISP_BROKER).
 * Set both to -1 to disable broker forwarding. */
syscall_result_t sandbox_intercept(const syscall_req_t *req,
                                   int broker_req_fd,
                                   int broker_resp_fd);

/* Return human-readable name of a syscall number */
const char *syscall_name(int nr);

#endif /* SANDBOX_H */
