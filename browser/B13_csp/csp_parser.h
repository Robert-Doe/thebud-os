#ifndef CSP_PARSER_H
#define CSP_PARSER_H

#define MAX_SOURCES    16
#define MAX_DIRECTIVES 8

struct csp_directive {
    char name[32];
    char sources[MAX_SOURCES][128];
    int  source_count;
    int  has_unsafe_inline;
    int  has_unsafe_eval;
    int  has_wildcard;
};

struct csp_policy {
    struct csp_directive directives[MAX_DIRECTIVES];
    int directive_count;
};

/* Parse a full CSP header value string into a policy struct.
   Returns 0 on success, -1 on parse error. */
int csp_parse(const char *header_value, struct csp_policy *out);

/* Print a human-readable dump of the parsed policy. */
void csp_print(const struct csp_policy *p);

#endif /* CSP_PARSER_H */
