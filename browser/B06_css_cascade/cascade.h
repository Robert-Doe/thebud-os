#ifndef CASCADE_H
#define CASCADE_H

/* ---------------------------------------------------------------
 * CSS Specificity
 *
 * Specificity is an ordered triple (ids, classes, elements).
 * It is NOT collapsed to a single integer — that would produce
 * incorrect results for large numbers (see DECISIONS.md #1).
 * --------------------------------------------------------------- */
typedef struct {
    int ids;       /* count of #id selectors   */
    int classes;   /* count of .class, [attr], :pseudo-class selectors */
    int elements;  /* count of tag and ::pseudo-element selectors */
} specificity_t;

/* Compare two specificities.
 * Returns: 1 if a > b, -1 if a < b, 0 if equal. */
int spec_cmp(specificity_t a, specificity_t b);

/* Compute specificity from a selector string */
specificity_t spec_from_selector(const char *selector);

/* Print specificity as (ids, classes, elements) */
void spec_print(specificity_t s);

/* ---------------------------------------------------------------
 * CSS Rule
 * --------------------------------------------------------------- */
#define CSS_ORIGIN_USER_AGENT  0
#define CSS_ORIGIN_USER        1
#define CSS_ORIGIN_AUTHOR      2

typedef struct {
    char          selector[64];
    char          property[32];
    char          value[64];
    specificity_t spec;
    int           important;   /* 1 if !important */
    int           origin;      /* CSS_ORIGIN_* */
    int           source_order; /* position in stylesheet (lower = earlier) */
} css_rule_t;

/* Given an array of rules that all match a given element and all
 * set the same property, cascade() returns the winning rule index.
 *
 * Cascade order (highest wins):
 *   1. !important author rules
 *   2. !important user rules
 *   3. normal author rules (highest specificity wins; ties: last source order)
 *   4. normal user rules
 *   5. user-agent rules
 */
int cascade(const css_rule_t *rules, int count);

#endif
