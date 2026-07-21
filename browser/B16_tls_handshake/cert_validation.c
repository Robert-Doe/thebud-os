#include "cert_validation.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Case-insensitive strstr */
static const char *stristr(const char *hay, const char *needle) {
    size_t nlen = strlen(needle);
    while (*hay) {
        if (strncasecmp(hay, needle, nlen) == 0) return hay;
        hay++;
    }
    return NULL;
}

static int hostname_matches(const char *pattern, const char *hostname) {
    if (strcmp(pattern, hostname) == 0) return 1;
    /* Wildcard: *.example.com matches www.example.com */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char *dot = strchr(hostname, '.');
        if (dot && strcmp(dot, pattern + 1) == 0) return 1;
    }
    return 0;
}

static int cert_matches_hostname(const struct cert *c, const char *hostname) {
    /* Check SAN first */
    if (c->san[0]) {
        char san_copy[256];
        strncpy(san_copy, c->san, sizeof(san_copy) - 1);
        char *tok = strtok(san_copy, ",");
        while (tok) {
            while (*tok == ' ') tok++;
            if (hostname_matches(tok, hostname)) return 1;
            tok = strtok(NULL, ",");
        }
    }
    /* Fall back to CN */
    return hostname_matches(c->common_name, hostname);
}

const char *cert_result_str(cert_result_t r) {
    switch (r) {
        case CERT_OK:               return "OK";
        case CERT_ERR_EXPIRED:      return "EXPIRED";
        case CERT_ERR_HOSTNAME:     return "HOSTNAME_MISMATCH";
        case CERT_ERR_CHAIN_BROKEN: return "CHAIN_BROKEN";
        case CERT_ERR_SELF_SIGNED:  return "SELF_SIGNED";
        case CERT_ERR_REVOKED:      return "REVOKED";
    }
    return "UNKNOWN";
}

cert_result_t cert_validate(const struct cert_chain *chain,
                             const char *hostname, long now) {
    if (!chain || chain->count < 1) return CERT_ERR_CHAIN_BROKEN;

    const struct cert *leaf = &chain->certs[0];

    printf("  Cert chain validation for '%s':\n", hostname);

    /* Step 1: Self-signed check */
    if (leaf->is_self_signed) {
        printf("    [FAIL] Leaf certificate is self-signed\n");
        printf("           No chain to a trusted root — anyone can create this.\n");
        return CERT_ERR_SELF_SIGNED;
    }

    /* Step 2: Chain verification */
    printf("    Chain: ");
    for (int i = 0; i < chain->count; i++) {
        printf("%s", chain->certs[i].common_name);
        if (i < chain->count - 1) printf(" → ");
    }
    printf("\n");

    for (int i = 0; i < chain->count - 1; i++) {
        /* Each cert's issuer must match the next cert's subject */
        if (strcmp(chain->certs[i].issuer,
                   chain->certs[i + 1].common_name) != 0) {
            printf("    [FAIL] Chain broken: '%s' issuer='%s' != '%s'\n",
                   chain->certs[i].common_name,
                   chain->certs[i].issuer,
                   chain->certs[i + 1].common_name);
            return CERT_ERR_CHAIN_BROKEN;
        }
    }
    printf("    [OK] Chain leads to root '%s'\n",
           chain->certs[chain->count - 1].common_name);

    /* Step 3: Expiry */
    if (now < leaf->not_before || now > leaf->not_after) {
        printf("    [FAIL] Certificate expired or not yet valid\n");
        return CERT_ERR_EXPIRED;
    }
    printf("    [OK] Certificate is within validity period\n");

    /* Step 4: Hostname */
    if (!cert_matches_hostname(leaf, hostname)) {
        printf("    [FAIL] Hostname '%s' not in CN='%s' or SAN='%s'\n",
               hostname, leaf->common_name, leaf->san);
        return CERT_ERR_HOSTNAME;
    }
    printf("    [OK] Hostname matches\n");

    /* Step 5: Simulated OCSP check */
    printf("    [OCSP] Checking revocation status ... not revoked\n");
    printf("    [OK] Certificate valid\n");

    return CERT_OK;
}
