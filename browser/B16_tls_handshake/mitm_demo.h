#ifndef MITM_DEMO_H
#define MITM_DEMO_H

/* Show what a passive MitM observer sees during a TLS handshake.
   tls_version: 12 = TLS 1.2, 13 = TLS 1.3 */
void mitm_demo_observe(int tls_version);

/* Demonstrate why cert validation is critical (attacker substitutes cert) */
void mitm_demo_cert_substitution(void);

#endif /* MITM_DEMO_H */
