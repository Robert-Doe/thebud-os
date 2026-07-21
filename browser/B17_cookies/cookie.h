#ifndef COOKIE_H
#define COOKIE_H

#define MAX_COOKIES 32

struct cookie {
    char name[64];
    char value[256];
    char domain[128];
    char path[128];
    int  http_only;   /* 1 = inaccessible to JavaScript */
    int  secure;      /* 1 = HTTPS only */
    int  same_site;   /* 0=None, 1=Lax, 2=Strict */
    long expires;     /* unix timestamp, -1 = session cookie */
};

struct cookie_jar {
    struct cookie cookies[MAX_COOKIES];
    int count;
};

/* Parse a Set-Cookie header value into a cookie struct.
   Returns 0 on success, -1 on parse error. */
int cookie_parse(const char *set_cookie_header, struct cookie *out);

/* Add a cookie to the jar. */
void cookie_jar_add(struct cookie_jar *jar, const struct cookie *c);

/* Get all cookies that should be sent for a given request.
   `scheme`: "http" or "https"
   `domain`: request domain
   `path`:   request path
   `is_cross_site`: 1 if the request is cross-site
   `is_top_level_nav`: 1 if this is a top-level navigation (link click)
   Writes matching cookies into `out`, returns count. */
int cookie_jar_get(const struct cookie_jar *jar,
                   const char *scheme,
                   const char *domain,
                   const char *path,
                   int is_cross_site,
                   int is_top_level_nav,
                   struct cookie *out,
                   int out_max);

/* Print a cookie */
void cookie_print(const struct cookie *c);

/* Check whether JS can read a cookie */
int cookie_js_readable(const struct cookie *c);

#endif /* COOKIE_H */
