#ifndef ORIGIN_H
#define ORIGIN_H

/* An origin is the triple (scheme, host, port).
 * The URL path, query, and fragment are NOT part of the origin. */
typedef struct {
    char scheme[16];   /* "https", "http", "file", "data", "" */
    char host[64];     /* "example.com", "" for opaque */
    int  port;         /* -1 = opaque/null, 0 = default for scheme */
} origin_t;

/* Special port values */
#define PORT_DEFAULT  0   /* use scheme's default (80 for http, 443 for https) */
#define PORT_OPAQUE  -1   /* null / opaque origin */

/* Parse a URL string into an origin_t.
 * Returns 0 on success, -1 if the URL yields an opaque origin
 * (in which case *out is set to the null/opaque origin). */
int parse_origin(const char *url, origin_t *out);

/* Compare two origins. Returns 1 if equal, 0 if not. */
int origins_equal(const origin_t *a, const origin_t *b);

/* Return effective port: if port == PORT_DEFAULT, infer from scheme. */
int effective_port(const origin_t *o);

/* Print origin in "scheme://host:port" form */
void print_origin(const origin_t *o);

/* True if this is a null/opaque origin */
int origin_is_opaque(const origin_t *o);

#endif /* ORIGIN_H */
