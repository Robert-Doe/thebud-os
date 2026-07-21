/*
 * B06 — CSS Cascade & Timing Side-Channels
 * ==========================================
 * Implements CSS specificity calculation and cascade ordering,
 * then demonstrates two CSS-based timing side-channel attacks.
 *
 * Build:  gcc -o demo main.c css_parser.c cascade.c timing_demo.c
 * Run:    ./demo
 */

#include <stdio.h>
#include <string.h>
#include "css_parser.h"
#include "cascade.h"
#include "timing_demo.h"

/* Filter rules matching a simulated element */
static int matches_element(const css_rule_t *rule,
                            const char *elem_tag,
                            const char *elem_class,
                            const char *elem_id)
{
    const char *sel = rule->selector;
    if (sel[0] == '#') return (strcmp(sel + 1, elem_id) == 0);
    if (sel[0] == '.') return (strcmp(sel + 1, elem_class) == 0);
    if (sel[0] == '*') return 1;
    /* tag match */
    return (strcmp(sel, elem_tag) == 0);
}

static void cascade_demo(void)
{
    printf("=======================================================\n");
    printf(" Part 1: CSS Specificity & Cascade\n");
    printf("=======================================================\n\n");

    /* ---- Specificity comparison demo ---- */
    printf("--- Specificity ordering ---\n");
    struct { const char *sel; } tests[] = {
        {"div"},
        {".warning"},
        {"#header"},
        {"div.warning"},
        {"#nav .item"},
        {"#nav #sub .item::before"},
    };
    int ntests = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < ntests; i++) {
        specificity_t s = spec_from_selector(tests[i].sel);
        printf("  %-30s  spec=", tests[i].sel);
        spec_print(s);
        printf("\n");
    }
    printf("\n");

    /* ---- Cascade demo ---- */
    printf("--- Cascade: which rule wins? ---\n\n");

    const char *css =
        "/* UA */ div { color: black; }\n"
        "p { color: blue; }\n"
        ".warning { color: orange; }\n"
        "#header { color: red; }\n"
        "div { color: green !important; }\n";

    css_rule_t rules[MAX_RULES];
    int n = css_parse(css, rules, MAX_RULES);
    printf("Parsed %d rules:\n", n);
    for (int i = 0; i < n; i++) css_rule_print(&rules[i]);
    printf("\n");

    /* Find all rules matching a <div id="header" class="warning"> element */
    printf("Element: <div id=\"header\" class=\"warning\">\n");
    printf("Matching rules for property 'color':\n");

    css_rule_t matching[MAX_RULES];
    int mc = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(rules[i].property, "color") == 0 &&
            matches_element(&rules[i], "div", "warning", "header")) {
            matching[mc++] = rules[i];
            printf("  [%d] ", mc);
            css_rule_print(&rules[i]);
        }
    }

    if (mc > 0) {
        int w = cascade(matching, mc);
        printf("\nWinner: rule %d -> '%s: %s'  (", w,
               matching[w].property, matching[w].value);
        spec_print(matching[w].spec);
        printf(")\n");
        printf("Reason: %s\n\n",
               matching[w].important ? "!important author rule beats all" :
               "highest specificity among non-important rules");
    }
}

int main(void)
{
    printf("=======================================================\n");
    printf(" B06 — CSS Cascade & Timing Side-Channels\n");
    printf("=======================================================\n\n");

    cascade_demo();
    timing_demo_run();

    return 0;
}
