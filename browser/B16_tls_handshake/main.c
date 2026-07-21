#include <stdio.h>
#include <string.h>
#include "tls_state.h"
#include "cert_validation.h"
#include "mitm_demo.h"

static void banner(const char *s) {
    printf("\n══════════════════════════════════════════════\n");
    printf(" %s\n", s);
    printf("══════════════════════════════════════════════\n");
}

/* Simulate a real certificate chain */
static void build_valid_chain(struct cert_chain *c) {
    c->count = 3;

    /* Leaf */
    strncpy(c->certs[0].common_name, "example.com", 127);
    strncpy(c->certs[0].issuer, "Let's Encrypt R3", 127);
    strncpy(c->certs[0].san, "example.com, www.example.com", 255);
    c->certs[0].not_before   = 1700000000L;
    c->certs[0].not_after    = 1800000000L;
    c->certs[0].is_self_signed = 0;

    /* Intermediate CA */
    strncpy(c->certs[1].common_name, "Let's Encrypt R3", 127);
    strncpy(c->certs[1].issuer, "ISRG Root X1", 127);
    c->certs[1].is_ca = 1;

    /* Root CA */
    strncpy(c->certs[2].common_name, "ISRG Root X1", 127);
    strncpy(c->certs[2].issuer, "ISRG Root X1", 127);
    c->certs[2].is_ca = 1;
    c->certs[2].is_self_signed = 1; /* root is self-signed but trusted by OS */
}

int main(void) {
    printf("B16 — TLS Handshake State Machine\n");

    /* ── TLS 1.3 handshake ───────────────────────────────────────── */
    banner("TLS 1.3 Handshake Simulation");
    struct tls_session s13;
    tls_session_init(&s13, "example.com", 13);
    tls_handshake_run(&s13);

    /* ── TLS 1.2 handshake (for comparison) ─────────────────────── */
    banner("TLS 1.2 Handshake Simulation (for comparison)");
    struct tls_session s12;
    tls_session_init(&s12, "example.com", 12);
    tls_handshake_run(&s12);

    /* ── Certificate chain validation ────────────────────────────── */
    banner("Certificate Chain Validation");

    struct cert_chain valid_chain;
    build_valid_chain(&valid_chain);

    /* Valid chain, current time within validity window */
    long now = 1750000000L;
    cert_result_t r = cert_validate(&valid_chain, "example.com", now);
    printf("  Result: %s\n\n", cert_result_str(r));

    /* Expired cert */
    printf("  -- Expired certificate scenario --\n");
    long expired_now = 1900000000L; /* past not_after */
    r = cert_validate(&valid_chain, "example.com", expired_now);
    printf("  Result: %s\n\n", cert_result_str(r));

    /* Hostname mismatch */
    printf("  -- Hostname mismatch scenario --\n");
    r = cert_validate(&valid_chain, "evil.com", now);
    printf("  Result: %s\n\n", cert_result_str(r));

    /* Broken chain */
    printf("  -- Broken chain (wrong issuer) --\n");
    struct cert_chain broken_chain = valid_chain;
    strncpy(broken_chain.certs[0].issuer, "Unknown CA", 127);
    r = cert_validate(&broken_chain, "example.com", now);
    printf("  Result: %s\n\n", cert_result_str(r));

    /* ── MitM observer comparison ────────────────────────────────── */
    banner("MitM Observer: What the Network Sees");
    mitm_demo_observe(12);
    mitm_demo_observe(13);

    /* ── MitM cert substitution attack ──────────────────────────── */
    banner("MitM Attack: Certificate Substitution");
    mitm_demo_cert_substitution();

    printf("\n── Summary ─────────────────────────────────────────────\n");
    printf("TLS 1.3 improvements over TLS 1.2:\n");
    printf("  * Certificate is encrypted (MitM can't read it)\n");
    printf("  * 1-RTT handshake (vs 2-RTT in TLS 1.2)\n");
    printf("  * Removed weak cipher suites (RC4, 3DES, MD5)\n");
    printf("  * Forward secrecy mandatory (ephemeral DH only)\n");
    printf("Remaining leak: SNI in ClientHello reveals hostname.\n");
    printf("ECH (Encrypted Client Hello) is the future fix.\n");

    return 0;
}
