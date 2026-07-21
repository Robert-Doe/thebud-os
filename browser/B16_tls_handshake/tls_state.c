#include "tls_state.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Pseudo-random fill for simulation */
static void fake_random(uint8_t *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = (uint8_t)(i * 37 + 0xAB);
}

const char *tls_state_name(tls_state_t state) {
    switch (state) {
        case TLS_STATE_IDLE:                  return "IDLE";
        case TLS_STATE_CLIENT_HELLO_SENT:     return "CLIENT_HELLO_SENT";
        case TLS_STATE_SERVER_HELLO_RECEIVED: return "SERVER_HELLO_RECEIVED";
        case TLS_STATE_HANDSHAKE_ENCRYPTED:   return "HANDSHAKE_ENCRYPTED";
        case TLS_STATE_CERTIFICATE_VERIFIED:  return "CERTIFICATE_VERIFIED";
        case TLS_STATE_HANDSHAKE_DONE:        return "HANDSHAKE_DONE";
        case TLS_STATE_APPLICATION_DATA:      return "APPLICATION_DATA";
        case TLS_STATE_ERROR:                 return "ERROR";
    }
    return "UNKNOWN";
}

void tls_session_init(struct tls_session *s, const char *server_name,
                      int tls_version) {
    memset(s, 0, sizeof(*s));
    strncpy(s->server_name, server_name, sizeof(s->server_name) - 1);
    s->tls_version = tls_version;
    s->state = TLS_STATE_IDLE;
    s->cert_valid = 0;
    fake_random(s->client_random, 32);
    fake_random(s->server_random, 32);
    /* Simulate traffic secret */
    for (int i = 0; i < 32; i++)
        s->traffic_secret[i] = s->client_random[i] ^ s->server_random[i];
}

int tls_handshake_step(struct tls_session *s) {
    switch (s->state) {
        case TLS_STATE_IDLE:
            printf("  [CLIENT → SERVER] ClientHello\n");
            printf("    PLAINTEXT — MitM observer CAN see:\n");
            printf("    * SNI extension: server_name='%s'\n", s->server_name);
            printf("    * Supported cipher suites (e.g. TLS_AES_256_GCM_SHA384)\n");
            printf("    * Supported TLS versions\n");
            printf("    * Client random (32 bytes)\n");
            printf("    * Key share (Diffie-Hellman public key)\n");
            s->state = TLS_STATE_CLIENT_HELLO_SENT;
            return 0;

        case TLS_STATE_CLIENT_HELLO_SENT:
            printf("  [SERVER → CLIENT] ServerHello\n");
            printf("    PLAINTEXT — MitM observer CAN see:\n");
            printf("    * Selected cipher suite\n");
            printf("    * Server random (32 bytes)\n");
            printf("    * Key share (DH public key for key exchange)\n");
            printf("    Key exchange happens here — both sides derive\n");
            printf("    the shared traffic secret without it crossing the wire.\n");
            s->state = TLS_STATE_SERVER_HELLO_RECEIVED;
            return 0;

        case TLS_STATE_SERVER_HELLO_RECEIVED:
            printf("  [Shared secret established via DH key exchange]\n");
            printf("  All subsequent handshake messages are ENCRYPTED.\n");
            if (s->tls_version == 13) {
                printf("  TLS 1.3: Certificate message is ENCRYPTED from here.\n");
                printf("    MitM observer CANNOT see the server certificate.\n");
            } else {
                printf("  TLS 1.2: Certificate message is PLAINTEXT.\n");
                printf("    MitM observer CAN see server certificate & extensions.\n");
            }
            s->state = TLS_STATE_HANDSHAKE_ENCRYPTED;
            return 0;

        case TLS_STATE_HANDSHAKE_ENCRYPTED:
            printf("  [SERVER → CLIENT] Certificate (ENCRYPTED in TLS 1.3)\n");
            if (s->tls_version == 13) {
                printf("    Encrypted — MitM sees only ciphertext.\n");
            } else {
                printf("    Plaintext — MitM sees: subject, issuer, SANs, expiry.\n");
            }
            printf("  [SERVER → CLIENT] CertificateVerify (signature)\n");
            printf("  [SERVER → CLIENT] Finished\n");
            /* Simulate certificate */
            strncpy(s->cert_common_name, s->server_name,
                    sizeof(s->cert_common_name) - 1);
            s->cert_valid = 1;
            s->state = TLS_STATE_CERTIFICATE_VERIFIED;
            return 0;

        case TLS_STATE_CERTIFICATE_VERIFIED:
            printf("  [CLIENT → SERVER] Finished\n");
            printf("    Client confirms handshake integrity.\n");
            s->state = TLS_STATE_HANDSHAKE_DONE;
            return 0;

        case TLS_STATE_HANDSHAKE_DONE:
            printf("  Handshake complete. TLS %s session established.\n",
                   s->tls_version == 13 ? "1.3" : "1.2");
            printf("  [APPLICATION DATA] All traffic encrypted with traffic secret.\n");
            s->state = TLS_STATE_APPLICATION_DATA;
            return 0;

        default:
            return -1;
    }
}

int tls_handshake_run(struct tls_session *s) {
    printf("  Initiating TLS %s handshake to '%s'\n",
           s->tls_version == 13 ? "1.3" : "1.2", s->server_name);
    printf("  ─────────────────────────────────────\n");
    while (s->state != TLS_STATE_APPLICATION_DATA &&
           s->state != TLS_STATE_ERROR) {
        if (tls_handshake_step(s) != 0) {
            s->state = TLS_STATE_ERROR;
            return -1;
        }
    }
    return s->state == TLS_STATE_APPLICATION_DATA ? 0 : -1;
}
