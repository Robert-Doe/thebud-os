#include "broker.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------
 * Broker Process
 *
 * The broker is a small, trusted helper that sits between the
 * sandbox and the OS.  It receives mediated requests (currently
 * only open()) and applies its own policy before executing them.
 *
 * Because the broker is a separate process, a compromised renderer
 * cannot hijack the broker's stack or function pointers.
 * --------------------------------------------------------------- */

/* Allowed file prefixes — only whitelisted paths may be opened */
static const char *allowed_paths[] = {
    "/tmp/",
    "/var/cache/browser/",
    NULL
};

static int path_is_allowed(const char *path)
{
    for (int i = 0; allowed_paths[i] != NULL; i++) {
        if (strncmp(path, allowed_paths[i], strlen(allowed_paths[i])) == 0)
            return 1;
    }
    return 0;
}

void broker_run(int req_read_fd, int resp_write_fd)
{
    char msg[256];
    char resp[256];

    printf("[BROKER] Started. Waiting for mediated open() requests.\n");

    while (1) {
        int n = (int)read(req_read_fd, msg, sizeof(msg) - 1);
        if (n <= 0) {
            printf("[BROKER] Renderer pipe closed.\n");
            break;
        }
        msg[n] = '\0';

        /* Strip trailing newline */
        char *nl = strchr(msg, '\n');
        if (nl) *nl = '\0';

        if (strncmp(msg, "OPEN:", 5) == 0) {
            const char *path = msg + 5;
            printf("[BROKER]   open() requested for path='%s'\n", path);

            if (path_is_allowed(path)) {
                /* In a real broker we'd call open() and pass the fd back
                 * via SCM_RIGHTS (fd-passing over a Unix socket).  Here
                 * we simulate success. */
                snprintf(resp, sizeof(resp), "OK:fd=7 (simulated)\n");
                printf("[BROKER]   -> APPROVED (path is whitelisted)\n");
            } else {
                snprintf(resp, sizeof(resp), "ERR:path not in whitelist\n");
                printf("[BROKER]   -> DENIED (path not in whitelist)\n");
            }
            write(resp_write_fd, resp, strlen(resp));

        } else if (strcmp(msg, "EXIT") == 0) {
            break;
        } else {
            snprintf(resp, sizeof(resp), "ERR:unknown broker request\n");
            write(resp_write_fd, resp, strlen(resp));
        }
    }

    printf("[BROKER] Exiting.\n");
}
