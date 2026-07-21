/*
 * B04 — Site Isolation
 * =====================
 * Demonstrates that each site (registered domain) gets its own
 * OS renderer process.  Cross-site iframes cannot share memory.
 *
 * Scenario: the main page at https://example.com embeds two
 * cross-origin iframes:
 *   - iframe1: https://widget.thirdparty.com
 *   - iframe2: https://ads.adnetwork.com
 *
 * Without site isolation (pre-2018 Chromium): all three could
 * share a renderer process, enabling Spectre cross-site reads.
 *
 * With site isolation: each gets a separate process.  The only
 * way they can communicate is postMessage through the browser broker.
 *
 * Build:  gcc -o demo main.c site_registry.c renderer_pool.c
 * Run:    ./demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "site_registry.h"
#include "renderer_pool.h"

/* Browser broker: relay a postMessage from one site to another.
 * Only the browser broker (this process) can do this — the renderers
 * cannot communicate directly. */
static void broker_postmsg(site_registry_t *reg,
                            const char *from_url,
                            const char *to_url,
                            const char *message)
{
    char from_site[64], to_site[64];
    url_to_site(from_url, from_site, sizeof(from_site));
    url_to_site(to_url,   to_site,   sizeof(to_site));

    printf("\n[BROKER] postMessage from '%s' -> '%s': \"%s\"\n",
           from_site, to_site, message);

    site_entry_t *target = site_registry_get_or_create(reg, to_site);
    if (!target || target->renderer_pid <= 0) {
        printf("[BROKER] Target site has no active renderer.\n");
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "POSTMSG:%s", message);
    renderer_pool_send(target, buf);

    char resp[256];
    int n = renderer_pool_recv(target, resp, sizeof(resp));
    if (n > 0)
        printf("[BROKER] Target renderer replied: %s", resp);
}

int main(void)
{
    printf("=======================================================\n");
    printf(" B04 — Site Isolation Demo\n");
    printf("=======================================================\n\n");

    site_registry_t reg;
    site_registry_init(&reg);

    /* Three URLs loaded in the same tab */
    const char *main_url    = "https://example.com/index.html";
    const char *iframe1_url = "https://sub.thirdparty.com/widget.html";
    const char *iframe2_url = "https://sub.adnetwork.com/ad.html";

    /* Compute site keys */
    char main_site[64], site1[64], site2[64];
    url_to_site(main_url,    main_site, sizeof(main_site));
    url_to_site(iframe1_url, site1,     sizeof(site1));
    url_to_site(iframe2_url, site2,     sizeof(site2));

    printf("[BROWSER] Loading page with two cross-origin iframes.\n\n");
    printf("  Main frame : %s  -> site: %s\n", main_url,    main_site);
    printf("  iframe1    : %s  -> site: %s\n", iframe1_url, site1);
    printf("  iframe2    : %s  -> site: %s\n", iframe2_url, site2);
    printf("\n");

    /* Allocate (or find) a renderer for each site */
    site_entry_t *e_main  = site_registry_get_or_create(&reg, main_site);
    site_entry_t *e_site1 = site_registry_get_or_create(&reg, site1);
    site_entry_t *e_site2 = site_registry_get_or_create(&reg, site2);

    /* Spawn a renderer process per site */
    renderer_pool_spawn(e_main,  main_site);
    renderer_pool_spawn(e_site1, site1);
    renderer_pool_spawn(e_site2, site2);

    sleep(0); /* give renderers a moment to print startup messages */

    printf("\n[BROWSER] Site registry after spawning:\n");
    site_registry_print(&reg);

    /* Demonstrate that each site has a DIFFERENT pid */
    printf("\n[BROWSER] Isolation check:\n");
    printf("  main_frame  pid = %d\n", (int)e_main->renderer_pid);
    printf("  iframe1     pid = %d\n", (int)e_site1->renderer_pid);
    printf("  iframe2     pid = %d\n", (int)e_site2->renderer_pid);
    if (e_main->renderer_pid != e_site1->renderer_pid &&
        e_site1->renderer_pid != e_site2->renderer_pid &&
        e_main->renderer_pid != e_site2->renderer_pid) {
        printf("  -> All three sites have DIFFERENT pids. Isolated!\n");
    }

    printf("\n[BROWSER] Demonstrating that cross-site iframes cannot\n");
    printf("  communicate directly — only through the broker:\n");

    /* iframe1 tries to directly message iframe2 — NOT POSSIBLE
     * because they have no shared pipe.  Only the broker can relay. */
    printf("\n  iframe1 cannot write to iframe2's pipe directly.\n");
    printf("  iframe1's pipe_to[1] = %d\n", e_site1->pipe_to[1]);
    printf("  iframe2's pipe_to[1] = %d\n", e_site2->pipe_to[1]);
    printf("  These are different file descriptors in different processes.\n");
    printf("  No shared memory between the two renderers.\n");

    /* Only the browser broker can relay postMessage */
    broker_postmsg(&reg, iframe1_url, iframe2_url,
                   "Hello from thirdparty.com!");

    broker_postmsg(&reg, main_url, iframe1_url,
                   "resize to 300px");

    /* Shutdown */
    printf("\n[BROWSER] Shutting down all renderers.\n");
    renderer_pool_shutdown(e_main);
    renderer_pool_shutdown(e_site1);
    renderer_pool_shutdown(e_site2);

    printf("\n=======================================================\n");
    printf(" Key takeaways:\n");
    printf("  - Each site gets a separate OS process (separate pid).\n");
    printf("  - Renderers have no shared memory — no Spectre leaks.\n");
    printf("  - Cross-site messaging ONLY through browser broker.\n");
    printf("  - Subdomains of same eTLD+1 share a renderer (same site).\n");
    printf("=======================================================\n");
    return 0;
}
