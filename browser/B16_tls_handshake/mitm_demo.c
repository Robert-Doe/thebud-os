#include "mitm_demo.h"
#include "tls_state.h"
#include "cert_validation.h"
#include <stdio.h>
#include <string.h>

void mitm_demo_observe(int tls_version) {
    printf("\n── MitM Observer: TLS %s ──────────────────────────────\n",
           tls_version == 13 ? "1.3" : "1.2");
    printf("An attacker on the network can see:\n\n");

    printf("  ClientHello (PLAINTEXT):\n");
    printf("    SNI: example.com    <-- hostname LEAKS to observer\n");
    printf("    Supported ciphers, key share, TLS versions\n\n");

    printf("  ServerHello (PLAINTEXT):\n");
    printf("    Selected cipher, key share\n\n");

    if (tls_version == 12) {
        printf("  Certificate (PLAINTEXT in TLS 1.2):\n");
        printf("    Subject: example.com\n");
        printf("    Issuer: Let's Encrypt Authority X3\n");
        printf("    SAN: example.com, www.example.com\n");
        printf("    Expiry: 2026-09-01\n");
        printf("    <-- Attacker sees the full cert chain\n\n");
        printf("  ServerKeyExchange (PLAINTEXT in TLS 1.2):\n");
        printf("    DH parameters, signature\n\n");
    } else {
        printf("  {EncryptedExtensions} (ENCRYPTED in TLS 1.3):\n");
        printf("    Attacker sees only ciphertext blob.\n");
        printf("    Certificate, CertVerify, Finished are all encrypted.\n\n");
    }

    printf("  [Application Data] ENCRYPTED — attacker sees only:\n");
    printf("    * Length of each record (approximate)\n");
    printf("    * Timing of packets\n");
    printf("    * Destination IP & port\n");
    printf("    * SNI (hostname) from ClientHello\n");
    printf("  Content, headers, URLs, cookies — all hidden.\n\n");

    if (tls_version == 13) {
        printf("  KEY RESIDUAL LEAK: SNI ('example.com') was in\n");
        printf("  the plaintext ClientHello. An observer knows WHICH\n");
        printf("  server you connected to, just not what you sent.\n");
        printf("  FIX: Encrypted Client Hello (ECH) encrypts the SNI\n");
        printf("  using a public key published in DNS.\n");
    }
}

void mitm_demo_cert_substitution(void) {
    printf("\n── MitM Cert Substitution Attack ──────────────────────\n");
    printf("Scenario: attacker intercepts TCP, presents OWN certificate\n");
    printf("          instead of the real server's certificate.\n\n");

    /* Victim's cert chain: attacker's self-signed */
    struct cert_chain attacker_chain;
    memset(&attacker_chain, 0, sizeof(attacker_chain));
    attacker_chain.count = 1;
    strncpy(attacker_chain.certs[0].common_name, "example.com",
            sizeof(attacker_chain.certs[0].common_name) - 1);
    strncpy(attacker_chain.certs[0].issuer, "example.com", 127);
    strncpy(attacker_chain.certs[0].san, "example.com", 255);
    attacker_chain.certs[0].not_before = 1000000000L;
    attacker_chain.certs[0].not_after  = 9999999999L;
    attacker_chain.certs[0].is_self_signed = 1;

    printf("  Attacker presents self-signed cert for 'example.com':\n");
    cert_result_t r = cert_validate(&attacker_chain, "example.com", 1750000000L);
    printf("  Validation result: %s\n\n", cert_result_str(r));

    printf("  If the client skips cert validation (insecure mode):\n");
    printf("    → Client establishes TLS session WITH THE ATTACKER.\n");
    printf("    → Attacker decrypts all traffic, re-encrypts to real server.\n");
    printf("    → Full MitM: client believes connection is secure.\n");
    printf("  This is why certificate validation MUST NOT be skipped.\n");
}
