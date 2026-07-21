#include "cascade.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Specificity computation from a selector string.
 *
 * Selector syntax handled:
 *   #foo           -> ids++
 *   .bar           -> classes++
 *   [attr]         -> classes++
 *   :hover         -> classes++
 *   ::before       -> elements++
 *   div            -> elements++
 *   div.foo#bar    -> ids=1, classes=1, elements=1
 * --------------------------------------------------------------- */
specificity_t spec_from_selector(const char *selector)
{
    specificity_t s = {0, 0, 0};
    const char *p = selector;

    while (*p) {
        if (*p == '#') {
            s.ids++;
            p++;
            /* skip the id name */
            while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && *p != ' ')
                p++;
        } else if (*p == '.') {
            s.classes++;
            p++;
            while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && *p != ' ')
                p++;
        } else if (*p == '[') {
            s.classes++;
            while (*p && *p != ']') p++;
            if (*p == ']') p++;
        } else if (*p == ':') {
            p++;
            if (*p == ':') {
                /* pseudo-element (::before, ::after) -> elements */
                s.elements++;
                p++;
            } else {
                /* pseudo-class (:hover, :focus) -> classes */
                s.classes++;
            }
            while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && *p != ' ')
                p++;
        } else if (*p == ' ' || *p == '>' || *p == '+' || *p == '~') {
            /* combinator — skip */
            p++;
        } else if (*p == '*') {
            /* universal selector — no specificity */
            p++;
        } else {
            /* element type selector */
            s.elements++;
            while (*p && *p != '.' && *p != '#' && *p != '[' && *p != ':' && *p != ' ')
                p++;
        }
    }
    return s;
}

int spec_cmp(specificity_t a, specificity_t b)
{
    /* Compare as ordered triple: ids > classes > elements */
    if (a.ids != b.ids)      return a.ids      > b.ids      ? 1 : -1;
    if (a.classes != b.classes) return a.classes > b.classes ? 1 : -1;
    if (a.elements != b.elements) return a.elements > b.elements ? 1 : -1;
    return 0;
}

void spec_print(specificity_t s)
{
    printf("(%d,%d,%d)", s.ids, s.classes, s.elements);
}

/* ---------------------------------------------------------------
 * Cascade algorithm
 *
 * Priority (descending):
 *   1. !important author
 *   2. !important user
 *   3. !important user-agent
 *   4. normal author (highest spec; tie -> last source order)
 *   5. normal user
 *   6. normal user-agent
 *
 * Returns the index of the winning rule, or -1 if count == 0.
 * --------------------------------------------------------------- */
int cascade(const css_rule_t *rules, int count)
{
    if (count == 0) return -1;

    int winner = 0;

    for (int i = 1; i < count; i++) {
        const css_rule_t *w = &rules[winner];
        const css_rule_t *c = &rules[i];

        /* Step 1: !important author beats everything */
        int w_imp_auth = (w->important && w->origin == CSS_ORIGIN_AUTHOR);
        int c_imp_auth = (c->important && c->origin == CSS_ORIGIN_AUTHOR);
        if (c_imp_auth && !w_imp_auth) { winner = i; continue; }
        if (w_imp_auth && !c_imp_auth) continue;

        /* Step 2: !important user */
        int w_imp_usr = (w->important && w->origin == CSS_ORIGIN_USER);
        int c_imp_usr = (c->important && c->origin == CSS_ORIGIN_USER);
        if (c_imp_usr && !w_imp_usr) { winner = i; continue; }
        if (w_imp_usr && !c_imp_usr) continue;

        /* Step 3: both !important or neither — compare origin */
        if (c->origin > w->origin) { winner = i; continue; }
        if (w->origin > c->origin) continue;

        /* Step 4: same origin — compare specificity */
        int sc = spec_cmp(c->spec, w->spec);
        if (sc > 0) { winner = i; continue; }
        if (sc < 0) continue;

        /* Step 5: same specificity — later source order wins */
        if (c->source_order > w->source_order) { winner = i; continue; }
    }

    return winner;
}
