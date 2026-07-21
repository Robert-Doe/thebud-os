#include "origin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * URL parsing — extract scheme, host, port.
 *
 * Handles:
 *   https://example.com/path        -> (https, example.com, 0)
 *   http://example.com:8080/        -> (http,  example.com, 8080)
 *   file:///etc/passwd              -> opaque (null origin)
 *   data:text/html,<h1>hi</h1>     -> opaque
 * --------------------------------------------------------------- */

static void set_opaque(origin_t *o)
{
    o->scheme[0] = '\0';
    o->host[0]   = '\0';
    o->port      = PORT_OPAQUE;
}

int parse_origin(const char *url, origin_t *out)
{
    memset(out, 0, sizeof(*out));

    /* data: URLs always produce an opaque origin */
    if (strncmp(url, "data:", 5) == 0) {
        set_opaque(out);
        return -1;
    }

    /* Find "://" separator */
    const char *sep = strstr(url, "://");
    if (!sep) {
        set_opaque(out);
        return -1;
    }

    /* Extract scheme */
    int slen = (int)(sep - url);
    if (slen >= (int)sizeof(out->scheme)) slen = (int)sizeof(out->scheme) - 1;
    strncpy(out->scheme, url, (size_t)slen);
    out->scheme[slen] = '\0';

    /* file:// → opaque (local files have null origin) */
    if (strcmp(out->scheme, "file") == 0) {
        set_opaque(out);
        return -1;
    }

    /* Advance past "://" */
    const char *host_start = sep + 3;

    /* Find end of host[:port] — terminated by '/', '?', '#', or '\0' */
    const char *host_end = host_start;
    while (*host_end && *host_end != '/' && *host_end != '?' && *host_end != '#')
        host_end++;

    /* Check for port */
    const char *colon = NULL;
    for (const char *p = host_start; p < host_end; p++) {
        if (*p == ':') { colon = p; break; }
    }

    if (colon) {
        int hlen = (int)(colon - host_start);
        if (hlen >= (int)sizeof(out->host)) hlen = (int)sizeof(out->host) - 1;
        strncpy(out->host, host_start, (size_t)hlen);
        out->host[hlen] = '\0';
        out->port = atoi(colon + 1);
    } else {
        int hlen = (int)(host_end - host_start);
        if (hlen >= (int)sizeof(out->host)) hlen = (int)sizeof(out->host) - 1;
        strncpy(out->host, host_start, (size_t)hlen);
        out->host[hlen] = '\0';
        out->port = PORT_DEFAULT;
    }

    /* Reject empty host */
    if (out->host[0] == '\0') {
        set_opaque(out);
        return -1;
    }

    return 0;
}

int effective_port(const origin_t *o)
{
    if (o->port == PORT_OPAQUE) return PORT_OPAQUE;
    if (o->port != PORT_DEFAULT) return o->port;
    /* Infer from scheme */
    if (strcmp(o->scheme, "https") == 0) return 443;
    if (strcmp(o->scheme, "http")  == 0) return 80;
    if (strcmp(o->scheme, "ftp")   == 0) return 21;
    return 0;
}

int origins_equal(const origin_t *a, const origin_t *b)
{
    /* Two opaque origins are never equal (each is unique) */
    if (origin_is_opaque(a) || origin_is_opaque(b)) return 0;

    if (strcmp(a->scheme, b->scheme) != 0) return 0;
    if (strcmp(a->host,   b->host)   != 0) return 0;
    if (effective_port(a) != effective_port(b)) return 0;
    return 1;
}

int origin_is_opaque(const origin_t *o)
{
    return (o->port == PORT_OPAQUE);
}

void print_origin(const origin_t *o)
{
    if (origin_is_opaque(o)) {
        printf("(opaque/null origin)");
        return;
    }
    int ep = effective_port(o);
    printf("%s://%s:%d", o->scheme, o->host, ep);
}
