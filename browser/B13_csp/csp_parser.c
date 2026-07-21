#include "csp_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Trim leading/trailing whitespace in-place, returns pointer to start. */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

/* Split `src` on `delim`, writing pointers into `parts` up to `max`.
   Returns count of parts. Modifies `src` in place. */
static int split(char *src, char delim, char **parts, int max) {
    int count = 0;
    char *p = src;
    parts[count++] = p;
    while (*p && count < max) {
        if (*p == delim) {
            *p = '\0';
            parts[count++] = p + 1;
        }
        p++;
    }
    return count;
}

int csp_parse(const char *header_value, struct csp_policy *out) {
    if (!header_value || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* Work on a mutable copy */
    char buf[2048];
    strncpy(buf, header_value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Split on ';' to get directives */
    char *dir_parts[MAX_DIRECTIVES];
    int dir_count = split(buf, ';', dir_parts, MAX_DIRECTIVES);

    for (int i = 0; i < dir_count && out->directive_count < MAX_DIRECTIVES; i++) {
        char *d = trim(dir_parts[i]);
        if (!*d) continue;

        struct csp_directive *dir = &out->directives[out->directive_count];
        memset(dir, 0, sizeof(*dir));

        /* First token is the directive name */
        char *space = strchr(d, ' ');
        if (space) {
            int name_len = (int)(space - d);
            if (name_len >= (int)sizeof(dir->name)) name_len = sizeof(dir->name) - 1;
            memcpy(dir->name, d, name_len);
            dir->name[name_len] = '\0';
            d = space + 1;
        } else {
            strncpy(dir->name, d, sizeof(dir->name) - 1);
            out->directive_count++;
            continue;
        }

        /* Remaining tokens are source expressions */
        char src_buf[1024];
        strncpy(src_buf, d, sizeof(src_buf) - 1);
        src_buf[sizeof(src_buf) - 1] = '\0';

        char *src_parts[MAX_SOURCES];
        int src_count = split(src_buf, ' ', src_parts, MAX_SOURCES);

        for (int j = 0; j < src_count && dir->source_count < MAX_SOURCES; j++) {
            char *s = trim(src_parts[j]);
            if (!*s) continue;

            if (strcmp(s, "'unsafe-inline'") == 0) {
                dir->has_unsafe_inline = 1;
            } else if (strcmp(s, "'unsafe-eval'") == 0) {
                dir->has_unsafe_eval = 1;
            } else if (strcmp(s, "*") == 0) {
                dir->has_wildcard = 1;
            }

            strncpy(dir->sources[dir->source_count], s,
                    sizeof(dir->sources[0]) - 1);
            dir->source_count++;
        }

        out->directive_count++;
    }

    return 0;
}

void csp_print(const struct csp_policy *p) {
    printf("CSP Policy (%d directive(s)):\n", p->directive_count);
    for (int i = 0; i < p->directive_count; i++) {
        const struct csp_directive *d = &p->directives[i];
        printf("  [%s] sources=%d wildcard=%d unsafe-inline=%d unsafe-eval=%d\n",
               d->name, d->source_count, d->has_wildcard,
               d->has_unsafe_inline, d->has_unsafe_eval);
        for (int j = 0; j < d->source_count; j++) {
            printf("    '%s'\n", d->sources[j]);
        }
    }
}
