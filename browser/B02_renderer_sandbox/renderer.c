#include "renderer.h"
#include "sandbox.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Renderer — makes 5 simulated syscalls demonstrating all three
 * sandbox dispositions: ALLOW, BROKER, BLOCK.
 * --------------------------------------------------------------- */

static void make_syscall(const char *label,
                         int nr, int fd,
                         const char *path,
                         const char *buf, int len,
                         int broker_req_fd, int broker_resp_fd)
{
    syscall_req_t req;
    memset(&req, 0, sizeof(req));
    req.syscall_nr = nr;
    req.arg_fd     = fd;
    if (path) strncpy(req.arg_path, path, sizeof(req.arg_path) - 1);
    if (buf)  strncpy(req.arg_buf,  buf,  sizeof(req.arg_buf)  - 1);
    req.arg_len = len;

    printf("\n[RENDERER] %s\n", label);
    syscall_result_t res = sandbox_intercept(&req, broker_req_fd, broker_resp_fd);
    printf("  [RESULT]  %s: %s\n", res.ok ? "OK" : "ERR", res.data);
}

void renderer_run(int broker_req_fd, int broker_resp_fd)
{
    printf("[RENDERER] Started. Making 5 simulated syscalls through sandbox.\n");

    /* 1. read(0, ...) — ALLOWED directly */
    make_syscall("read(fd=0) — should be ALLOWED",
                 SYS_READ, 0, NULL, NULL, 0,
                 broker_req_fd, broker_resp_fd);

    /* 2. write(1, "hello") — ALLOWED (stdout) */
    make_syscall("write(fd=1, \"Hello from renderer!\") — should be ALLOWED",
                 SYS_WRITE, 1, NULL, "Hello from renderer!", 20,
                 broker_req_fd, broker_resp_fd);

    /* 3. open("/tmp/cache.dat") — BROKER mediated, whitelisted path */
    make_syscall("open(\"/tmp/cache.dat\") — should go to BROKER (allowed path)",
                 SYS_OPEN, 0, "/tmp/cache.dat", NULL, 0,
                 broker_req_fd, broker_resp_fd);

    /* 4. open("/etc/passwd") — BROKER mediated, blocked path */
    make_syscall("open(\"/etc/passwd\") — BROKER should DENY",
                 SYS_OPEN, 0, "/etc/passwd", NULL, 0,
                 broker_req_fd, broker_resp_fd);

    /* 5. socket() — BLOCKED by sandbox whitelist */
    make_syscall("socket() — should be BLOCKED",
                 SYS_SOCKET, 0, NULL, NULL, 0,
                 broker_req_fd, broker_resp_fd);

    /* 6. exec() — BLOCKED */
    make_syscall("exec(\"/bin/sh\") — should be BLOCKED",
                 SYS_EXEC, 0, "/bin/sh", NULL, 0,
                 broker_req_fd, broker_resp_fd);

    /* 7. INTENTIONAL BUG: write to fd=42 — whitelist gap */
    printf("\n--- Demonstrating whitelist gap (intentional bug) ---\n");
    make_syscall("write(fd=42, data) — BUG: fd check missing in whitelist",
                 SYS_WRITE, 42, NULL, "exfiltrated_data", 16,
                 broker_req_fd, broker_resp_fd);
}
