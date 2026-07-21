#include "sop.h"
#include <stdio.h>
#include <string.h>

sop_decision_t sop_check(const origin_t *from, const origin_t *to, char *reason)
{
    /* Opaque origins cannot communicate with anything */
    if (origin_is_opaque(from)) {
        snprintf(reason, 64, "source is opaque origin — always isolated");
        return SOP_BLOCK;
    }
    if (origin_is_opaque(to)) {
        snprintf(reason, 64, "destination is opaque origin — always isolated");
        return SOP_BLOCK;
    }

    /* Scheme mismatch */
    if (strcmp(from->scheme, to->scheme) != 0) {
        snprintf(reason, 64, "scheme differs ('%s' vs '%s')",
                 from->scheme, to->scheme);
        return SOP_BLOCK;
    }

    /* Host mismatch */
    if (strcmp(from->host, to->host) != 0) {
        snprintf(reason, 64, "host differs ('%s' vs '%s')",
                 from->host, to->host);
        return SOP_BLOCK;
    }

    /* Port mismatch */
    if (effective_port(from) != effective_port(to)) {
        snprintf(reason, 64, "port differs (%d vs %d)",
                 effective_port(from), effective_port(to));
        return SOP_BLOCK;
    }

    snprintf(reason, 64, "scheme + host + port all match");
    return SOP_ALLOW;
}

void sop_test(const char *label, const char *from_url, const char *to_url)
{
    origin_t from, to;
    char reason[64];

    parse_origin(from_url, &from);
    parse_origin(to_url,   &to);

    sop_decision_t d = sop_check(&from, &to, reason);

    printf("  [%s] %s\n", d == SOP_ALLOW ? " ALLOW" : " BLOCK", label);
    printf("         from: "); print_origin(&from); printf("\n");
    printf("         to:   "); print_origin(&to);   printf("\n");
    printf("         why:  %s\n\n", reason);
}
