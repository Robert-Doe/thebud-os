#include "browser_kernel.h"
#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * Browser Kernel — the trusted browser process.
 *
 * Responsibilities:
 *   - Own the network stack (navigate on behalf of renderer)
 *   - Enforce security policy
 *   - Reject any renderer request that violates the sandbox
 * --------------------------------------------------------------- */

/* Parse a request string "REQ:TYPE:ARGS\n" into type and args. */
static void parse_req(const char *msg, char *type, char *args)
{
    /* format: REQ:TYPE:ARGS */
    const char *p = msg;
    if (strncmp(p, "REQ:", 4) != 0) { type[0] = '\0'; args[0] = '\0'; return; }
    p += 4;
    const char *colon = strchr(p, ':');
    if (!colon) { strncpy(type, p, 31); type[31] = '\0'; args[0] = '\0'; return; }
    int tlen = (int)(colon - p);
    if (tlen > 31) tlen = 31;
    strncpy(type, p, (size_t)tlen);
    type[tlen] = '\0';
    strncpy(args, colon + 1, 127);
    args[127] = '\0';
    /* strip trailing newline */
    char *nl = strchr(args, '\n');
    if (nl) *nl = '\0';
}

void browser_kernel_run(int req_read_fd, int resp_write_fd)
{
    char msg[IPC_MSG_MAX];
    char type[32], args[128];
    char resp[IPC_MSG_MAX];

    printf("[BROWSER-KERNEL] Started. Waiting for renderer requests.\n");

    while (1) {
        int n = ipc_recv(req_read_fd, msg, IPC_MSG_MAX);
        if (n <= 0) {
            printf("[BROWSER-KERNEL] Renderer pipe closed — renderer exited.\n");
            break;
        }

        parse_req(msg, type, args);
        printf("[BROWSER-KERNEL] Received request  type='%s'  args='%s'\n", type, args);

        if (strcmp(type, "NAVIGATE") == 0) {
            /* Policy: only allow http/https schemes */
            if (strncmp(args, "https://", 8) == 0 || strncmp(args, "http://", 7) == 0) {
                /* In a real browser this would start a network request.
                 * Here we just acknowledge it. */
                snprintf(resp, sizeof(resp), "RESP:OK:Loading %s\n", args);
                printf("[BROWSER-KERNEL] APPROVED NAVIGATE to %s\n", args);
            } else {
                snprintf(resp, sizeof(resp), "RESP:ERR:Scheme not allowed\n");
                printf("[BROWSER-KERNEL] BLOCKED NAVIGATE — bad scheme\n");
            }
            ipc_send(resp_write_fd, resp);

        } else if (strcmp(type, "OPEN_FILE") == 0) {
            /* Renderer is NOT allowed to open arbitrary files.
             * This is a sandbox violation — block it. */
            snprintf(resp, sizeof(resp), "RESP:ERR:SANDBOX_VIOLATION open_file not permitted\n");
            printf("[BROWSER-KERNEL] BLOCKED OPEN_FILE '%s' — sandbox violation!\n", args);
            ipc_send(resp_write_fd, resp);

        } else if (strcmp(type, "COOKIE_GET") == 0) {
            /* Cookies are owned by the browser kernel, never the renderer. */
            snprintf(resp, sizeof(resp), "RESP:OK:session=abc123\n");
            printf("[BROWSER-KERNEL] APPROVED COOKIE_GET for %s\n", args);
            ipc_send(resp_write_fd, resp);

        } else if (strcmp(type, "SOCKET") == 0) {
            snprintf(resp, sizeof(resp), "RESP:ERR:SANDBOX_VIOLATION direct socket not permitted\n");
            printf("[BROWSER-KERNEL] BLOCKED SOCKET — renderer cannot open raw sockets\n");
            ipc_send(resp_write_fd, resp);

        } else if (strcmp(type, "EXIT") == 0) {
            printf("[BROWSER-KERNEL] Renderer signalled EXIT. Shutting down.\n");
            break;

        } else {
            snprintf(resp, sizeof(resp), "RESP:ERR:Unknown request type '%s'\n", type);
            ipc_send(resp_write_fd, resp);
        }
    }

    printf("[BROWSER-KERNEL] Exiting.\n");
}
