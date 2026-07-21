#include "renderer_pool.h"
#include "site_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ---------------------------------------------------------------
 * Renderer Pool
 *
 * Manages a pool of renderer processes.  Each site gets exactly
 * one renderer process.  When a new site is navigated to, we
 * fork() a new renderer; existing sites reuse their renderer.
 * --------------------------------------------------------------- */

pid_t renderer_pool_spawn(site_entry_t *entry, const char *site)
{
    printf("[POOL] Spawning new renderer process for site: %s\n", site);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* ---- RENDERER CHILD ---- */
        /* Close the parent's ends of the pipes */
        close(entry->pipe_to[1]);    /* parent writes to this; child reads */
        close(entry->pipe_from[0]);  /* parent reads from this; child writes */

        /* Simple renderer event loop: read commands, write acknowledgements */
        char buf[256];
        char resp[256];
        int fd_read  = entry->pipe_to[0];
        int fd_write = entry->pipe_from[1];

        printf("[RENDERER pid=%d site=%s] Started.\n", (int)getpid(), site);

        while (1) {
            int n = (int)read(fd_read, buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';

            /* Strip newline */
            char *nl = strchr(buf, '\n');
            if (nl) *nl = '\0';

            printf("[RENDERER pid=%d site=%s] Received: '%s'\n",
                   (int)getpid(), site, buf);

            if (strcmp(buf, "EXIT") == 0) break;

            if (strncmp(buf, "POSTMSG:", 8) == 0) {
                /* postMessage received from browser broker */
                snprintf(resp, sizeof(resp),
                         "ACK:postMessage received: '%s'\n", buf + 8);
            } else {
                snprintf(resp, sizeof(resp), "ACK:%s\n", buf);
            }
            write(fd_write, resp, strlen(resp));
        }

        printf("[RENDERER pid=%d site=%s] Exiting.\n", (int)getpid(), site);
        close(fd_read);
        close(fd_write);
        exit(0);
    }

    /* Parent: record pid and close the child's ends */
    entry->renderer_pid = pid;
    close(entry->pipe_to[0]);    /* child reads; parent writes */
    close(entry->pipe_from[1]);  /* child writes; parent reads */

    printf("[POOL] Spawned renderer pid=%d for site=%s\n", (int)pid, site);
    return pid;
}

void renderer_pool_send(site_entry_t *entry, const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s\n", msg);
    write(entry->pipe_to[1], buf, strlen(buf));
}

int renderer_pool_recv(site_entry_t *entry, char *out, int len)
{
    int n = (int)read(entry->pipe_from[0], out, (size_t)(len - 1));
    if (n > 0) out[n] = '\0';
    return n;
}

void renderer_pool_shutdown(site_entry_t *entry)
{
    if (entry->renderer_pid <= 0) return;
    renderer_pool_send(entry, "EXIT");
    int status;
    waitpid(entry->renderer_pid, &status, 0);
    printf("[POOL] Renderer pid=%d for site=%s exited.\n",
           (int)entry->renderer_pid, entry->site);
    entry->renderer_pid = -1;
}
