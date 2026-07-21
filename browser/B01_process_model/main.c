/*
 * B01 — Multi-Process Browser Architecture
 * =========================================
 * Demonstrates the Chromium multi-process model:
 *
 *   main() forks into two OS processes:
 *     - Browser Kernel (trusted): owns security policy, network, cookies
 *     - Renderer      (untrusted): parses HTML, runs JS, paints
 *
 * They communicate exclusively through a pair of pipes (the IPC channel).
 * The renderer CANNOT make privileged OS calls directly — every such
 * operation must be approved by the kernel process.
 *
 * Build:  gcc -o demo main.c browser_kernel.c renderer.c ipc.c
 * Run:    ./demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "ipc.h"
#include "browser_kernel.h"
#include "renderer.h"

int main(void)
{
    printf("=======================================================\n");
    printf(" B01 — Multi-Process Browser Architecture Demo\n");
    printf("=======================================================\n\n");

    ipc_channel_t ch;
    if (ipc_open(&ch) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* ---- CHILD = RENDERER PROCESS (untrusted) ---- */
        ipc_child_setup(&ch);
        /*
         * Child writes requests  to pipe_req[1]
         * Child reads  responses from pipe_resp[0]
         */
        renderer_run(ch.pipe_req[1], ch.pipe_resp[0]);
        close(ch.pipe_req[1]);
        close(ch.pipe_resp[0]);
        exit(0);

    } else {
        /* ---- PARENT = BROWSER KERNEL PROCESS (trusted) ---- */
        ipc_parent_setup(&ch);
        /*
         * Parent reads  requests  from pipe_req[0]
         * Parent writes responses to pipe_resp[1]
         */
        browser_kernel_run(ch.pipe_req[0], ch.pipe_resp[1]);
        close(ch.pipe_req[0]);
        close(ch.pipe_resp[1]);

        int status;
        waitpid(pid, &status, 0);
        printf("\n[MAIN] Renderer exited with status %d.\n", WEXITSTATUS(status));
    }

    printf("\n=======================================================\n");
    printf(" Key takeaways:\n");
    printf("  - NAVIGATE https://  -> APPROVED  (safe scheme)\n");
    printf("  - OPEN_FILE /etc/passwd -> BLOCKED (sandbox violation)\n");
    printf("  - SOCKET direct      -> BLOCKED  (no raw network)\n");
    printf("  - NAVIGATE file://   -> BLOCKED  (bad scheme)\n");
    printf("  - COOKIE_GET         -> APPROVED  (kernel mediates)\n");
    printf(" The renderer runs in a separate OS process so a\n");
    printf(" compromised renderer cannot bypass kernel policy.\n");
    printf("=======================================================\n");
    return 0;
}
