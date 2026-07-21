#include <stdio.h>
#include <string.h>
#include "cors.h"
#include "fetch_sim.h"

static void banner(const char *title) {
    printf("\n══════════════════════════════════════════════\n");
    printf(" %s\n", title);
    printf("══════════════════════════════════════════════\n");
}

int main(void) {
    printf("B14 — CORS & Fetch\n");

    /* ── Scenario 1: Same-origin ─────────────────────────────────── */
    banner("1. Same-origin request (always allowed)");
    struct fetch_request r1 = {
        .url            = "https://example.com/api/data",
        .origin         = "https://example.com",
        .method         = "GET",
        .initiator_type = "fetch",
        .response_type  = "application/json"
    };
    struct cors_response resp_none = { .allow_origin = "" };
    fetch_sim(&r1, &resp_none);

    /* ── Scenario 2: Simple cross-origin GET ─────────────────────── */
    banner("2. Simple cross-origin GET");
    struct fetch_request r2 = {
        .url            = "https://api.other.com/data",
        .origin         = "https://app.example.com",
        .method         = "GET",
        .initiator_type = "fetch",
        .response_type  = "application/json"
    };
    struct cors_response resp2 = {
        .allow_origin       = "https://app.example.com",
        .allow_methods      = "GET, POST",
        .allow_credentials  = 0
    };
    fetch_sim(&r2, &resp2);

    /* Same scenario but server doesn't set ACAO */
    printf("  Without ACAO header:\n");
    struct cors_response resp2b = { .allow_origin = "" };
    fetch_sim(&r2, &resp2b);

    /* ── Scenario 3: Preflighted PUT + custom header ─────────────── */
    banner("3. Preflighted request (PUT + X-Custom-Header)");
    struct fetch_request r3 = {
        .url            = "https://api.other.com/resource",
        .origin         = "https://app.example.com",
        .method         = "PUT",
        .custom_headers = "X-Custom-Header",
        .initiator_type = "fetch",
        .response_type  = "application/json"
    };
    struct cors_response resp3 = {
        .allow_origin       = "https://app.example.com",
        .allow_methods      = "GET, POST, PUT, DELETE",
        .allow_headers      = "Content-Type, X-Custom-Header",
        .allow_credentials  = 0
    };
    fetch_sim(&r3, &resp3);

    /* Preflight fails: header not in Allow-Headers */
    struct cors_response resp3b = {
        .allow_origin   = "https://app.example.com",
        .allow_methods  = "GET, POST, PUT",
        .allow_headers  = "Content-Type"   /* missing X-Custom-Header */
    };
    printf("  Server doesn't allow X-Custom-Header:\n");
    fetch_sim(&r3, &resp3b);

    /* ── Scenario 4: null origin ─────────────────────────────────── */
    banner("4. null origin (sandboxed iframe / data: URL)");
    printf("Context: a sandboxed <iframe sandbox> or data: URL has origin='null'.\n");
    printf("Danger: if server responds ACAO:* with sensitive data,\n");
    printf("        attacker-controlled iframe can read it.\n\n");

    struct fetch_request r4 = {
        .url              = "https://api.example.com/sensitive",
        .origin           = "null",
        .method           = "GET",
        .with_credentials = 0,
        .initiator_type   = "fetch",
        .response_type    = "application/json"
    };
    struct cors_response resp4_star = {
        .allow_origin      = "*",
        .allow_methods     = "GET",
        .allow_credentials = 0
    };
    printf("  ACAO:* (no credentials required):\n");
    fetch_sim(&r4, &resp4_star);

    struct cors_response resp4_null = {
        .allow_origin      = "null",
        .allow_methods     = "GET",
        .allow_credentials = 1
    };
    printf("  Dangerous: ACAO:null with credentials=true:\n");
    /* Show CORS check directly */
    struct cors_request cr4 = {
        .origin = "null", .method = "GET", .is_preflight = 0
    };
    int d4 = cors_check(&cr4, &resp4_null);
    cors_print_decision(&cr4, &resp4_null, d4);
    printf("  Explanation: ACAO:'null' + credentials lets any sandboxed\n"
           "  page (e.g. attacker's <iframe sandbox src=data:...>)\n"
           "  make credentialed requests and read the response.\n");

    /* ── Scenario 5: CORB ────────────────────────────────────────── */
    banner("5. CORB — Cross-Origin Read Blocking");
    printf("An <img> tag tries to load a JSON resource cross-origin.\n");
    printf("Even if CORS permits it, the browser withholds the body.\n\n");

    struct fetch_request r5 = {
        .url            = "https://api.other.com/secrets.json",
        .origin         = "https://attacker.com",
        .method         = "GET",
        .initiator_type = "img",               /* <img src=...> */
        .response_type  = "application/json"   /* JSON response */
    };
    struct cors_response resp5 = {
        .allow_origin  = "*",
        .allow_methods = "GET"
    };
    fetch_result_t res5 = fetch_sim(&r5, &resp5);
    printf("  fetch_result = %s\n",
           res5 == FETCH_CORB ? "FETCH_CORB (body withheld)" :
           res5 == FETCH_BLOCKED ? "FETCH_BLOCKED" : "FETCH_OK");
    printf("  Explanation: CORB prevents cross-origin <img>/<script> from\n"
           "  reading JSON/HTML even when ACAO permits the load.\n"
           "  This stops side-channel attacks that read response bytes\n"
           "  via img.width tricks or error timing.\n");

    return 0;
}
