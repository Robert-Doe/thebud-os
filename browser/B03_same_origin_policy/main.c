/*
 * B03 — Same-Origin Policy Engine
 * ================================
 * Implements origin parsing and SOP enforcement from scratch.
 * Runs 10 test cases showing allowed and blocked cross-origin requests.
 *
 * Build:  gcc -o demo main.c origin.c sop.c
 * Run:    ./demo
 */

#include <stdio.h>
#include "sop.h"
#include "origin.h"

int main(void)
{
    printf("=======================================================\n");
    printf(" B03 — Same-Origin Policy Engine Demo\n");
    printf("=======================================================\n\n");
    printf("Rule: same-origin = scheme + host + port ALL match.\n");
    printf("Anything else is cross-origin and BLOCKED.\n\n");

    /* ---- ALLOW cases ---- */
    printf("--- Expected: ALLOW ---\n\n");

    sop_test("Same origin — path differs (path is not part of origin)",
             "https://example.com/page1",
             "https://example.com/page2");

    sop_test("Same origin — query string differs",
             "https://example.com/search?q=hello",
             "https://example.com/search?q=world");

    sop_test("Explicit port matches inferred default (https:443)",
             "https://example.com:443/a",
             "https://example.com/b");

    sop_test("Explicit port matches inferred default (http:80)",
             "http://example.com:80/a",
             "http://example.com/b");

    /* ---- BLOCK cases ---- */
    printf("--- Expected: BLOCK ---\n\n");

    sop_test("Different scheme (https vs http)",
             "https://example.com/",
             "http://example.com/");

    sop_test("Different host (subdomains are separate origins)",
             "https://a.example.com/",
             "https://b.example.com/");

    sop_test("Different port",
             "https://example.com:8443/",
             "https://example.com:9443/");

    sop_test("Completely different domain",
             "https://attacker.com/",
             "https://bank.com/");

    sop_test("file:// has opaque/null origin — cannot communicate",
             "file:///home/user/page.html",
             "https://example.com/");

    sop_test("data: URL has opaque origin — isolated from everything",
             "https://example.com/",
             "data:text/html,<h1>hello</h1>");

    /* ---- Special note on document.domain ---- */
    printf("=======================================================\n");
    printf(" Note on document.domain (deprecated escape hatch):\n");
    printf("  Both a.example.com and b.example.com could set\n");
    printf("  document.domain = 'example.com' to relax SOP and\n");
    printf("  share DOM access.  This is deprecated in modern\n");
    printf("  browsers because it breaks process isolation:\n");
    printf("  if a.example.com is compromised it can now read\n");
    printf("  b.example.com's DOM even across site boundaries.\n");
    printf("  Use postMessage() instead.\n");
    printf("=======================================================\n");

    return 0;
}
