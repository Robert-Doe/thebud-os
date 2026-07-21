#include "cookie.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static char *ltrim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}

static void rtrim(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static int strncasecmp_impl(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == '\0') return 0;
    }
    return 0;
}

int cookie_parse(const char *hdr, struct cookie *out) {
    if (!hdr || !out) return -1;
    memset(out, 0, sizeof(*out));
    out->expires = -1;
    strcpy(out->path, "/");
    out->same_site = 1; /* default Lax since Chrome 80 */

    char buf[1024];
    strncpy(buf, hdr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* First token: name=value */
    char *save;
    char *tok = strtok_r(buf, ";", &save);
    if (!tok) return -1;
    tok = ltrim(tok); rtrim(tok);

    char *eq = strchr(tok, '=');
    if (!eq) return -1;
    *eq = '\0';
    strncpy(out->name, ltrim(tok), sizeof(out->name) - 1);
    strncpy(out->value, ltrim(eq + 1), sizeof(out->value) - 1);

    /* Remaining tokens: attributes */
    while ((tok = strtok_r(NULL, ";", &save)) != NULL) {
        tok = ltrim(tok); rtrim(tok);
        if (strncasecmp_impl(tok, "HttpOnly", 8) == 0) {
            out->http_only = 1;
        } else if (strncasecmp_impl(tok, "Secure", 6) == 0) {
            out->secure = 1;
        } else if (strncasecmp_impl(tok, "SameSite=Strict", 15) == 0) {
            out->same_site = 2;
        } else if (strncasecmp_impl(tok, "SameSite=Lax", 12) == 0) {
            out->same_site = 1;
        } else if (strncasecmp_impl(tok, "SameSite=None", 13) == 0) {
            out->same_site = 0;
        } else if (strncasecmp_impl(tok, "Domain=", 7) == 0) {
            strncpy(out->domain, tok + 7, sizeof(out->domain) - 1);
        } else if (strncasecmp_impl(tok, "Path=", 5) == 0) {
            strncpy(out->path, tok + 5, sizeof(out->path) - 1);
        } else if (strncasecmp_impl(tok, "Expires=", 8) == 0 ||
                   strncasecmp_impl(tok, "Max-Age=", 8) == 0) {
            out->expires = 1750000000L + 3600; /* simulate 1 hour from now */
        }
    }
    return 0;
}

void cookie_jar_add(struct cookie_jar *jar, const struct cookie *c) {
    if (jar->count >= MAX_COOKIES) return;
    jar->cookies[jar->count++] = *c;
}

static int domain_matches(const char *cookie_domain, const char *req_domain) {
    if (!cookie_domain[0]) return 1; /* no domain restriction */
    /* Exact match */
    if (strcmp(cookie_domain, req_domain) == 0) return 1;
    /* Domain cookie matches subdomains */
    size_t clen = strlen(cookie_domain);
    size_t rlen = strlen(req_domain);
    if (rlen > clen &&
        req_domain[rlen - clen - 1] == '.' &&
        strcmp(req_domain + rlen - clen, cookie_domain) == 0) {
        return 1;
    }
    return 0;
}

static int path_matches(const char *cookie_path, const char *req_path) {
    size_t plen = strlen(cookie_path);
    return strncmp(cookie_path, req_path, plen) == 0;
}

int cookie_jar_get(const struct cookie_jar *jar,
                   const char *scheme,
                   const char *domain,
                   const char *path,
                   int is_cross_site,
                   int is_top_level_nav,
                   struct cookie *out,
                   int out_max) {
    int count = 0;
    int is_https = (strcmp(scheme, "https") == 0);

    for (int i = 0; i < jar->count && count < out_max; i++) {
        const struct cookie *c = &jar->cookies[i];

        /* Secure flag: only send over HTTPS */
        if (c->secure && !is_https) continue;

        /* Domain match */
        if (!domain_matches(c->domain, domain)) continue;

        /* Path match */
        if (!path_matches(c->path, path)) continue;

        /* SameSite enforcement */
        if (is_cross_site) {
            if (c->same_site == 2) {
                /* Strict: never send cross-site */
                continue;
            }
            if (c->same_site == 1 && !is_top_level_nav) {
                /* Lax: only allow cross-site on top-level navigations */
                continue;
            }
            /* SameSite=None: always send (requires Secure) */
            if (c->same_site == 0 && !c->secure) continue;
        }

        out[count++] = *c;
    }
    return count;
}

void cookie_print(const struct cookie *c) {
    const char *ss_names[] = {"None", "Lax", "Strict"};
    printf("  Cookie: %s=%s\n", c->name, c->value);
    printf("    Domain=%s Path=%s\n", c->domain, c->path);
    printf("    HttpOnly=%d Secure=%d SameSite=%s Expires=%ld\n",
           c->http_only, c->secure,
           ss_names[c->same_site < 3 ? c->same_site : 1],
           c->expires);
}

int cookie_js_readable(const struct cookie *c) {
    return !c->http_only;
}
