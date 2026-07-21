#ifndef TLS_STATE_H
#define TLS_STATE_H

#include <stdint.h>

typedef enum {
    TLS_STATE_IDLE,
    TLS_STATE_CLIENT_HELLO_SENT,
    TLS_STATE_SERVER_HELLO_RECEIVED,
    TLS_STATE_HANDSHAKE_ENCRYPTED,   /* after key exchange */
    TLS_STATE_CERTIFICATE_VERIFIED,
    TLS_STATE_HANDSHAKE_DONE,
    TLS_STATE_APPLICATION_DATA,
    TLS_STATE_ERROR
} tls_state_t;

struct tls_session {
    tls_state_t state;
    char        server_name[128];     /* SNI */
    uint8_t     client_random[32];
    uint8_t     server_random[32];
    uint8_t     traffic_secret[32];   /* simulated — not real crypto */
    char        cert_common_name[128];
    int         cert_valid;
    int         tls_version;          /* 12 = TLS 1.2, 13 = TLS 1.3 */
};

/* Initialize a new TLS session */
void tls_session_init(struct tls_session *s, const char *server_name, int tls_version);

/* Step the handshake forward one message.
   Returns 0 if the step succeeded, -1 on error/abort. */
int tls_handshake_step(struct tls_session *s);

/* Run the full handshake to completion.
   Returns 0 on success, -1 if any step fails. */
int tls_handshake_run(struct tls_session *s);

const char *tls_state_name(tls_state_t state);

#endif /* TLS_STATE_H */
