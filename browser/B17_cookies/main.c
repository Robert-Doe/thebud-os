#include <stdio.h>
#include <string.h>
#include "cookie.h"
#include "session.h"
#include "csrf_demo.h"

static void banner(const char *s) {
    printf("\n══════════════════════════════════════════════\n");
    printf(" %s\n", s);
    printf("══════════════════════════════════════════════\n");
}

int main(void) {
    printf("B17 — Cookie & Session Security\n");

    /* ── Parse cookies ───────────────────────────────────────────── */
    banner("Cookie Parsing");

    const char *hdrs[] = {
        "session=abc123; HttpOnly; Secure; SameSite=Strict; Domain=example.com",
        "tracking=xyz; SameSite=None; Secure",
        "prefs=dark; SameSite=Lax; Path=/settings",
        "insecure=noflags",
        NULL
    };

    struct cookie_jar jar;
    memset(&jar, 0, sizeof(jar));

    for (int i = 0; hdrs[i]; i++) {
        struct cookie c;
        int r = cookie_parse(hdrs[i], &c);
        printf("Set-Cookie: %s\n", hdrs[i]);
        if (r == 0) {
            cookie_print(&c);
            cookie_jar_add(&jar, &c);
        } else {
            printf("  [Parse error]\n");
        }
        printf("\n");
    }

    /* ── SameSite enforcement table ──────────────────────────────── */
    banner("SameSite Enforcement Table");
    printf("%-20s %-16s %-16s %-16s\n",
           "SameSite", "Same-site req", "Cross-site nav", "Cross-site sub-res");
    printf("%-20s %-16s %-16s %-16s\n",
           "None", "SEND", "SEND", "SEND");
    printf("%-20s %-16s %-16s %-16s\n",
           "Lax (default)", "SEND", "SEND", "BLOCK");
    printf("%-20s %-16s %-16s %-16s\n",
           "Strict", "SEND", "BLOCK", "BLOCK");
    printf("\nNote: 'Cross-site nav' = top-level link click from another site.\n");
    printf("Note: 'Cross-site sub-res' = <img>, <form POST>, XHR from another site.\n");

    /* ── Cookie jar get: which cookies sent? ─────────────────────── */
    banner("Cookie Jar: Which Cookies Are Sent?");

    /* Cross-site sub-resource (img load from evil.com) */
    struct cookie sent[8];
    int count;

    printf("Request: same-site, HTTPS, path=/\n");
    count = cookie_jar_get(&jar, "https", "example.com", "/",
                           0, 0, sent, 8);
    printf("  Cookies sent: %d\n", count);
    for (int i = 0; i < count; i++)
        printf("    %s=%s\n", sent[i].name, sent[i].value);

    printf("\nRequest: cross-site sub-resource, HTTPS\n");
    count = cookie_jar_get(&jar, "https", "example.com", "/",
                           1, 0, sent, 8);
    printf("  Cookies sent: %d\n", count);
    for (int i = 0; i < count; i++)
        printf("    %s=%s (SameSite=None)\n", sent[i].name, sent[i].value);

    printf("\nRequest: cross-site top-level navigation, HTTPS\n");
    count = cookie_jar_get(&jar, "https", "example.com", "/",
                           1, 1, sent, 8);
    printf("  Cookies sent: %d\n", count);
    for (int i = 0; i < count; i++)
        printf("    %s=%s\n", sent[i].name, sent[i].value);

    /* ── Session management ──────────────────────────────────────── */
    banner("Session Token Generation & Validation");
    struct session_store store;
    memset(&store, 0, sizeof(store));
    long now = 1750000000L;

    struct session *s1 = session_create(&store, "bob", now);
    printf("Created session for 'bob': token=%.32s...\n", s1->token);

    struct session *found = session_lookup(&store, s1->token, now + 60);
    printf("Lookup (60s later): %s (user=%s)\n",
           found ? "FOUND" : "NOT FOUND",
           found ? found->username : "-");

    struct session *expired = session_lookup(&store, s1->token, now + 4000);
    printf("Lookup (4000s later, past 1hr expiry): %s\n",
           expired ? "FOUND" : "EXPIRED/INVALID");

    /* ── CSRF demos ──────────────────────────────────────────────── */
    csrf_demo_attack();
    csrf_demo_fix();
    csrf_demo_httponly();

    return 0;
}
