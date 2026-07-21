/*
 * B02 — Renderer Sandbox
 * ======================
 * Demonstrates a syscall whitelist sandbox with broker mediation.
 *
 * Architecture:
 *   main() forks a broker process and a renderer process.
 *   The renderer calls sandbox_intercept() for every simulated syscall.
 *   The sandbox checks its whitelist:
 *     ALLOW  -> executed directly (read, write-to-stdout)
 *     BROKER -> forwarded to broker process via pipe
 *     BLOCK  -> rejected with EPERM
 *
 * Build:  gcc -o demo main.c sandbox.c renderer.c broker.c
 * Run:    ./demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "sandbox.h"
#include "broker.h"
#include "renderer.h"

int main(void)
{
    printf("=======================================================\n");
    printf(" B02 — Renderer Sandbox Demo\n");
    printf("=======================================================\n\n");

    sandbox_init();

    /* Pipe: renderer -> broker (requests) */
    int pipe_req[2];
    /* Pipe: broker -> renderer (responses) */
    int pipe_resp[2];

    if (pipe(pipe_req) < 0 || pipe(pipe_resp) < 0) {
        perror("pipe");
        return 1;
    }

    pid_t broker_pid = fork();
    if (broker_pid < 0) { perror("fork"); return 1; }

    if (broker_pid == 0) {
        /* ---- BROKER PROCESS ---- */
        close(pipe_req[1]);   /* broker only reads requests */
        close(pipe_resp[0]);  /* broker only writes responses */
        broker_run(pipe_req[0], pipe_resp[1]);
        close(pipe_req[0]);
        close(pipe_resp[1]);
        exit(0);
    }

    /* ---- RENDERER (runs in parent for simplicity) ---- */
    close(pipe_req[0]);   /* renderer only writes requests */
    close(pipe_resp[1]);  /* renderer only reads responses */

    renderer_run(pipe_req[1], pipe_resp[0]);

    /* Signal broker to exit */
    write(pipe_req[1], "EXIT\n", 5);
    close(pipe_req[1]);
    close(pipe_resp[0]);

    int status;
    waitpid(broker_pid, &status, 0);

    printf("\n=======================================================\n");
    printf(" Summary of sandbox dispositions:\n");
    printf("  read(fd=0)          -> ALLOW  (direct execution)\n");
    printf("  write(fd=1, data)   -> ALLOW  (stdout permitted)\n");
    printf("  open(/tmp/cache)    -> BROKER -> approved\n");
    printf("  open(/etc/passwd)   -> BROKER -> denied\n");
    printf("  socket()            -> BLOCK\n");
    printf("  exec()              -> BLOCK\n");
    printf("  write(fd=42, data)  -> BUG: allowed (fd not checked)\n");
    printf("=======================================================\n");
    return 0;
}
