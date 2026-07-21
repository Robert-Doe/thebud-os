#ifndef CERT_VALIDATION_H
#define CERT_VALIDATION_H

struct cert {
    char common_name[128];
    char issuer[128];
    char san[256];       /* comma-separated Subject Alt Names */
    long not_before;     /* unix timestamp */
    long not_after;      /* unix timestamp */
    int  is_ca;
    int  is_self_signed;
};

struct cert_chain {
    struct cert certs[8];  /* [0]=leaf, [last]=root */
    int count;
};

typedef enum {
    CERT_OK                  = 0,
    CERT_ERR_EXPIRED         = 1,
    CERT_ERR_HOSTNAME        = 2,
    CERT_ERR_CHAIN_BROKEN    = 3,
    CERT_ERR_SELF_SIGNED     = 4,
    CERT_ERR_REVOKED         = 5
} cert_result_t;

/* Validate a certificate chain for a given hostname.
   `now` is the current unix timestamp.
   Returns CERT_OK or an error code. */
cert_result_t cert_validate(const struct cert_chain *chain,
                             const char *hostname, long now);

const char *cert_result_str(cert_result_t r);

#endif /* CERT_VALIDATION_H */
