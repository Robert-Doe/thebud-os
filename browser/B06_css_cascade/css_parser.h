#ifndef CSS_PARSER_H
#define CSS_PARSER_H

#include "cascade.h"

#define MAX_RULES 64

/* Parse a CSS string into an array of css_rule_t.
 * Handles: simple tag, .class, #id selectors and combinations.
 * Returns the number of rules parsed. */
int css_parse(const char *css, css_rule_t *rules, int max_rules);

/* Print a single CSS rule */
void css_rule_print(const css_rule_t *rule);

#endif
