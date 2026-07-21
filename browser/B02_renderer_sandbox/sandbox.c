#include "sandbox.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------
 * Whitelist table: one entry per simulated syscall.
 * --------------------------------------------------------------- */
static disposition_t whitelist[SYS_COUNT];

void sandbox_init(void)
{
    whitelist[SYS_READ]   = DISP_ALLOW;
    whitelist[SYS_WRITE]  = DISP_ALLOW;  /* fd check happens inside intercept */
    whitelist[SYS_OPEN]   = DISP_BROKER; /* broker must mediate file opens */
    whitelist[SYS_SOCKET] = DISP_BLOCK;
    whitelist[SYS_EXEC]   = DISP_BLOCK;
}

const char *syscall_name(int nr)
{
    switch (nr) {
        case SYS_READ:   return "read";
        case SYS_WRITE:  return "write";
        case SYS_OPEN:   return "open";
        case SYS_SOCKET: return "socket";
        case SYS_EXEC:   return "exec";
        default:         return "unknown";
    }
}

/* ---------------------------------------------------------------
 * Execute a simulated read() directly inside the sandbox.
 * --------------------------------------------------------------- */
static syscall_result_t do_read(const syscall_req_t *req)
{
    syscall_result_t r;
    /* Simulate reading from stdin (fd=0) */
    if (req->arg_fd == 0) {
        snprintf(r.data, sizeof(r.data), "(simulated stdin data)");
        r.ok = 1;
    } else {
        snprintf(r.data, sizeof(r.data), "ERR: renderer cannot read fd=%d directly", req->arg_fd);
        r.ok = 0;
    }
    return r;
}

/* ---------------------------------------------------------------
 * Execute a simulated write() with an fd policy check.
 *
 * INTENTIONAL BUG (see DECISIONS.md #5):
 *   We only check fd == 1 (stdout).  A write to fd=42 is not
 *   caught by this guard, demonstrating a whitelist gap.
 * --------------------------------------------------------------- */
static syscall_result_t do_write(const syscall_req_t *req)
{
    syscall_result_t r;
    if (req->arg_fd == 1) {
        /* Allowed: write to stdout */
        printf("    [sandbox:write] -> \"%.*s\"\n", req->arg_len, req->arg_buf);
        r.ok = 1;
        snprintf(r.data, sizeof(r.data), "wrote %d bytes to stdout", req->arg_len);
    } else if (req->arg_fd == 42) {
        /* BUG: whitelist says ALLOW for write, but we forgot to
         * check the fd.  fd=42 might be an open socket inherited
         * from the browser kernel — this is the escape hatch. */
        printf("    [sandbox:write] BUG! fd=42 not checked — write allowed!\n");
        r.ok = 1;
        snprintf(r.data, sizeof(r.data), "ESCAPE: wrote %d bytes to fd=42", req->arg_len);
    } else {
        r.ok = 0;
        snprintf(r.data, sizeof(r.data), "ERR: write to fd=%d not permitted", req->arg_fd);
    }
    return r;
}

/* ---------------------------------------------------------------
 * Forward an open() request to the broker via IPC.
 * --------------------------------------------------------------- */
static syscall_result_t forward_to_broker(const syscall_req_t *req,
                                           int broker_req_fd,
                                           int broker_resp_fd)
{
    syscall_result_t r;
    char msg[256];
    snprintf(msg, sizeof(msg), "OPEN:%s\n", req->arg_path);
    write(broker_req_fd, msg, strlen(msg));

    char resp[256];
    int n = (int)read(broker_resp_fd, resp, sizeof(resp) - 1);
    if (n <= 0) {
        r.ok = 0;
        snprintf(r.data, sizeof(r.data), "ERR: broker did not respond");
        return r;
    }
    resp[n] = '\0';
    if (strncmp(resp, "OK:", 3) == 0) {
        r.ok = 1;
        snprintf(r.data, sizeof(r.data), "%s", resp + 3);
    } else {
        r.ok = 0;
        snprintf(r.data, sizeof(r.data), "%s", resp + 4); /* skip "ERR:" */
    }
    return r;
}

/* ---------------------------------------------------------------
 * Main intercept point — called for every simulated syscall.
 * --------------------------------------------------------------- */
syscall_result_t sandbox_intercept(const syscall_req_t *req,
                                   int broker_req_fd,
                                   int broker_resp_fd)
{
    syscall_result_t r;

    if (req->syscall_nr < 0 || req->syscall_nr >= SYS_COUNT) {
        r.ok = 0;
        snprintf(r.data, sizeof(r.data), "ERR: unknown syscall %d", req->syscall_nr);
        return r;
    }

    disposition_t disp = whitelist[req->syscall_nr];

    switch (disp) {
    case DISP_ALLOW:
        printf("  [SANDBOX] syscall %-8s -> ALLOW (direct)\n", syscall_name(req->syscall_nr));
        if (req->syscall_nr == SYS_READ)  return do_read(req);
        if (req->syscall_nr == SYS_WRITE) return do_write(req);
        /* fallthrough for any other ALLOW (shouldn't happen) */
        r.ok = 1; snprintf(r.data, sizeof(r.data), "ok"); return r;

    case DISP_BROKER:
        printf("  [SANDBOX] syscall %-8s -> BROKER (mediated)\n", syscall_name(req->syscall_nr));
        if (broker_req_fd < 0) {
            r.ok = 0;
            snprintf(r.data, sizeof(r.data), "ERR: no broker available");
            return r;
        }
        return forward_to_broker(req, broker_req_fd, broker_resp_fd);

    case DISP_BLOCK:
        printf("  [SANDBOX] syscall %-8s -> BLOCK\n", syscall_name(req->syscall_nr));
        r.ok = 0;
        snprintf(r.data, sizeof(r.data),
                 "ERR: syscall '%s' is blocked by sandbox policy",
                 syscall_name(req->syscall_nr));
        return r;
    }

    r.ok = 0;
    snprintf(r.data, sizeof(r.data), "ERR: internal sandbox error");
    return r;
}
