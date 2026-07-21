#include "csp_enforcer.h"
#include <stdio.h>
#include <string.h>

/* Find the directive with the given name. Falls back to default-src. */
static const struct csp_directive *find_directive(const struct csp_policy *p,
                                                   const char *name) {
    const struct csp_directive *fallback = NULL;
    for (int i = 0; i < p->directive_count; i++) {
        if (strcmp(p->directives[i].name, name) == 0)
            return &p->directives[i];
        if (strcmp(p->directives[i].name, "default-src") == 0)
            fallback = &p->directives[i];
    }
    return fallback;
}

/* Check whether `url` matches a CSP source expression `src`.
   Handles: 'self', 'none', *, scheme://host, scheme://host/* */
static int source_matches(const char *src_expr, const char *url) {
    /* Bare wildcard: match everything */
    if (strcmp(src_expr, "*") == 0) return 1;

    /* 'none' blocks everything */
    if (strcmp(src_expr, "'none'") == 0) return 0;

    /* 'self' — for demo purposes treat as same-origin marker.
       In a real browser this compares to the document origin. */
    if (strcmp(src_expr, "'self'") == 0) {
        /* Simulate: 'self' matches if url starts with "https://self.example" */
        return (strncmp(url, "https://self.example", 20) == 0 ||
                strncmp(url, "http://self.example", 19) == 0);
    }

    /* Host with trailing /* — strip wildcard and match prefix */
    char pattern[128];
    strncpy(pattern, src_expr, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';
    int plen = (int)strlen(pattern);
    if (plen >= 2 && pattern[plen - 1] == '*' && pattern[plen - 2] == '/') {
        pattern[plen - 2] = '\0'; /* remove /* */
        return (strncmp(url, pattern, strlen(pattern)) == 0);
    }

    /* Scheme-only (e.g. "https:") — match any https URL */
    if (plen > 0 && pattern[plen - 1] == ':') {
        return (strncmp(url, pattern, plen) == 0);
    }

    /* Exact host prefix match (scheme://host) */
    /* Strip trailing slash from pattern for comparison */
    if (plen > 0 && pattern[plen - 1] == '/') pattern[--plen] = '\0';

    /* Check if url starts with pattern and next char is / or end */
    size_t pl = strlen(pattern);
    if (strncmp(url, pattern, pl) == 0) {
        char next = url[pl];
        if (next == '\0' || next == '/' || next == '?') return 1;
    }

    return 0;
}

int csp_check(struct csp_policy *p, const char *directive_name,
              const char *source_url) {
    const struct csp_directive *dir = find_directive(p, directive_name);
    if (!dir) {
        /* No directive, no default-src → allow (open policy) */
        return 1;
    }

    /* Explicit 'none' with no other sources → block all */
    if (dir->source_count == 1 &&
        strcmp(dir->sources[0], "'none'") == 0) {
        return 0;
    }

    /* Wildcard shortcut */
    if (dir->has_wildcard) return 1;

    /* Check each source expression */
    for (int i = 0; i < dir->source_count; i++) {
        if (source_matches(dir->sources[i], source_url)) return 1;
    }

    return 0;
}

int csp_check_inline_script(struct csp_policy *p) {
    const struct csp_directive *dir = find_directive(p, "script-src");
    if (!dir) return 0; /* default deny inline */
    return dir->has_unsafe_inline;
}

int csp_check_eval(struct csp_policy *p) {
    const struct csp_directive *dir = find_directive(p, "script-src");
    if (!dir) return 0;
    return dir->has_unsafe_eval;
}
