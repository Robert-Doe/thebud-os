#include "renderer.h"
#include "ipc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * Renderer Process
 *
 * This is the UNTRUSTED process.  It cannot make privileged calls
 * directly — everything goes through the browser kernel via IPC.
 *
 * In a real browser this is where HTML parsing, layout, JavaScript
 * execution, and painting happen.  All of those components are
 * exposed to arbitrary attacker-controlled web content, which is
 * why the renderer is isolated in its own OS process.
 * --------------------------------------------------------------- */

/* Send a request and print the response. */
static void do_request(int req_fd, int resp_fd, const char *type, const char *args)
{
    char msg[IPC_MSG_MAX];
    char resp[IPC_MSG_MAX];

    snprintf(msg, sizeof(msg), "REQ:%s:%s\n", type, args);
    printf("[RENDERER]        Sending  -> %s", msg);
    ipc_send(req_fd, msg);

    int n = ipc_recv(resp_fd, resp, IPC_MSG_MAX);
    if (n > 0)
        printf("[RENDERER]        Response <- %s\n", resp);
    else
        printf("[RENDERER]        (no response)\n");
}

void renderer_run(int req_write_fd, int resp_read_fd)
{
    printf("[RENDERER] Started (untrusted process). Sending requests to browser kernel.\n\n");

    /* --- legitimate request: navigate to an https URL --- */
    do_request(req_write_fd, resp_read_fd, "NAVIGATE", "https://example.com");

    /* --- sandbox escape attempt: open a local file directly --- */
    printf("\n[RENDERER] Attempting to read /etc/passwd directly through browser kernel...\n");
    do_request(req_write_fd, resp_read_fd, "OPEN_FILE", "/etc/passwd");

    /* --- legitimate request: get a cookie (kernel mediates) --- */
    printf("\n[RENDERER] Requesting cookie for example.com...\n");
    do_request(req_write_fd, resp_read_fd, "COOKIE_GET", "example.com");

    /* --- sandbox escape: raw socket --- */
    printf("\n[RENDERER] Attempting to open a raw socket...\n");
    do_request(req_write_fd, resp_read_fd, "SOCKET", "1.2.3.4:80");

    /* --- bad scheme navigate --- */
    printf("\n[RENDERER] Attempting to navigate to file:// URL...\n");
    do_request(req_write_fd, resp_read_fd, "NAVIGATE", "file:///etc/shadow");

    /* --- done --- */
    printf("\n[RENDERER] All requests sent. Signalling exit.\n");
    do_request(req_write_fd, resp_read_fd, "EXIT", "");
}
