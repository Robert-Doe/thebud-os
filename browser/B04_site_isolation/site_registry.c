#include "site_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void site_registry_init(site_registry_t *reg)
{
    memset(reg, 0, sizeof(*reg));
    for (int i = 0; i < MAX_SITES; i++) {
        reg->entries[i].renderer_pid = -1;
        reg->entries[i].active       = 0;
        reg->entries[i].pipe_to[0]   = -1;
        reg->entries[i].pipe_to[1]   = -1;
        reg->entries[i].pipe_from[0] = -1;
        reg->entries[i].pipe_from[1] = -1;
    }
    reg->count = 0;
}

/* ---------------------------------------------------------------
 * Simplified eTLD+1 extraction.
 * Real browsers use the Public Suffix List (publicsuffix.org).
 * We handle common cases: extract the last two domain labels.
 *
 * "https://a.example.com/path" -> "https://example.com"
 * "https://example.com/path"   -> "https://example.com"
 * --------------------------------------------------------------- */
void url_to_site(const char *url, char *site_out, int site_len)
{
    char scheme[16] = {0};
    char host[64]   = {0};

    /* Extract scheme */
    const char *sep = strstr(url, "://");
    if (!sep) { strncpy(site_out, url, (size_t)(site_len - 1)); return; }
    int slen = (int)(sep - url);
    if (slen >= 16) slen = 15;
    strncpy(scheme, url, (size_t)slen);
    scheme[slen] = '\0';

    /* Extract host (up to first / after scheme://) */
    const char *hstart = sep + 3;
    const char *hend   = hstart;
    while (*hend && *hend != '/' && *hend != ':') hend++;
    int hlen = (int)(hend - hstart);
    if (hlen >= 64) hlen = 63;
    strncpy(host, hstart, (size_t)hlen);
    host[hlen] = '\0';

    /* Find eTLD+1: last two dot-separated labels */
    /* Walk backwards to find the second-to-last dot */
    char *last_dot = strrchr(host, '.');
    if (!last_dot) {
        /* single label — use as-is */
        snprintf(site_out, (size_t)site_len, "%s://%s", scheme, host);
        return;
    }
    /* Find the dot before last_dot */
    char *prev_dot = NULL;
    for (char *p = host; p < last_dot; p++) {
        if (*p == '.') prev_dot = p;
    }
    const char *etld1 = prev_dot ? prev_dot + 1 : host;
    snprintf(site_out, (size_t)site_len, "%s://%s", scheme, etld1);
}

site_entry_t *site_registry_get_or_create(site_registry_t *reg, const char *site)
{
    /* Search for existing entry */
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].active &&
            strcmp(reg->entries[i].site, site) == 0) {
            return &reg->entries[i];
        }
    }
    /* Create new entry */
    if (reg->count >= MAX_SITES) return NULL;
    site_entry_t *e = &reg->entries[reg->count++];
    strncpy(e->site, site, sizeof(e->site) - 1);
    e->renderer_pid = -1;
    e->active       = 1;
    pipe(e->pipe_to);
    pipe(e->pipe_from);
    return e;
}

void site_registry_print(const site_registry_t *reg)
{
    printf("  Site Registry (%d entries):\n", reg->count);
    printf("  %-30s  %-8s  %s\n", "Site", "PID", "IPC pipes");
    printf("  %-30s  %-8s  %s\n", "----", "---", "---------");
    for (int i = 0; i < reg->count; i++) {
        const site_entry_t *e = &reg->entries[i];
        if (!e->active) continue;
        if (e->renderer_pid > 0)
            printf("  %-30s  %-8d  to=[%d,%d] from=[%d,%d]\n",
                   e->site, (int)e->renderer_pid,
                   e->pipe_to[0],   e->pipe_to[1],
                   e->pipe_from[0], e->pipe_from[1]);
        else
            printf("  %-30s  (not yet spawned)\n", e->site);
    }
}
