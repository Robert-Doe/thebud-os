#include "css_parser.h"
#include "cascade.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * Minimal CSS parser.
 *
 * Parses rules of the form:
 *   selector { property: value; }
 *   selector { property: value !important; }
 *
 * Origin is set by a comment prefix before the rule block:
 *   UA  annotation -> user-agent origin
 *   U   annotation -> user origin
 *   Default origin = author.
 * --------------------------------------------------------------- */

static void trim(char *s)
{
    /* Leading whitespace */
    int i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s) - i + 1);
    /* Trailing whitespace */
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

int css_parse(const char *css, css_rule_t *rules, int max_rules)
{
    int count = 0;
    const char *p = css;

    while (*p && count < max_rules) {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        /* Check for origin annotation comment: UA or U annotation */
        int origin = CSS_ORIGIN_AUTHOR;
        if (strncmp(p, "/*", 2) == 0) {
            const char *end_comment = strstr(p, "*/");
            if (end_comment) {
                char comment[32] = {0};
                int clen = (int)(end_comment - p - 2);
                if (clen > 0 && clen < 31) {
                    strncpy(comment, p + 2, (size_t)clen);
                    trim(comment);
                }
                if (strcmp(comment, "UA") == 0) origin = CSS_ORIGIN_USER_AGENT;
                else if (strcmp(comment, "U")  == 0) origin = CSS_ORIGIN_USER;
                p = end_comment + 2;
                while (*p && isspace((unsigned char)*p)) p++;
            }
        }

        /* Read selector: up to '{' */
        const char *brace = strchr(p, '{');
        if (!brace) break;
        char selector[64] = {0};
        int slen = (int)(brace - p);
        if (slen >= 64) slen = 63;
        strncpy(selector, p, (size_t)slen);
        trim(selector);
        if (selector[0] == '\0') { p = brace + 1; continue; }

        p = brace + 1;

        /* Read declaration block: up to '}' */
        const char *close = strchr(p, '}');
        if (!close) break;
        char decl[256] = {0};
        int dlen = (int)(close - p);
        if (dlen >= 256) dlen = 255;
        strncpy(decl, p, (size_t)dlen);
        p = close + 1;

        /* Parse each declaration "property: value;" */
        char *dp = decl;
        while (*dp) {
            while (*dp && isspace((unsigned char)*dp)) dp++;
            if (!*dp) break;
            char *semi = strchr(dp, ';');
            char one[256] = {0};
            if (semi) {
                int olen = (int)(semi - dp);
                if (olen >= 256) olen = 255;
                strncpy(one, dp, (size_t)olen);
                dp = semi + 1;
            } else {
                strncpy(one, dp, 255);
                dp += strlen(dp);
            }
            trim(one);
            if (one[0] == '\0') continue;

            /* Split on ':' */
            char *colon = strchr(one, ':');
            if (!colon) continue;
            char prop[32] = {0};
            char val[64]  = {0};
            int plen = (int)(colon - one);
            if (plen >= 32) plen = 31;
            strncpy(prop, one, (size_t)plen);
            trim(prop);
            strncpy(val, colon + 1, 63);
            trim(val);

            /* Check for !important */
            int important = 0;
            char *imp = strstr(val, "!important");
            if (imp) {
                important = 1;
                *imp = '\0';
                trim(val);
            }

            if (count >= max_rules) break;
            css_rule_t *r = &rules[count];
            strncpy(r->selector, selector, 63);
            strncpy(r->property, prop,     31);
            strncpy(r->value,    val,      63);
            r->spec         = spec_from_selector(selector);
            r->important    = important;
            r->origin       = origin;
            r->source_order = count;
            count++;
        }
    }

    return count;
}

void css_rule_print(const css_rule_t *rule)
{
    const char *origins[] = {"UA", "User", "Author"};
    printf("  selector='%s'  %s: %s%s  spec=",
           rule->selector,
           rule->property, rule->value,
           rule->important ? " !important" : "");
    spec_print(rule->spec);
    printf("  origin=%s  order=%d\n",
           origins[rule->origin], rule->source_order);
}
