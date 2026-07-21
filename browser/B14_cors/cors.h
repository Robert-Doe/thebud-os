#ifndef CORS_H
#define CORS_H

struct cors_request {
    char origin[128];
    char method[16];      /* GET, POST, PUT, DELETE, OPTIONS */
    char headers[256];    /* comma-separated custom headers */
    int  is_preflight;    /* 1 if this is an OPTIONS preflight */
};

struct cors_response {
    char allow_origin[128];   /* Access-Control-Allow-Origin value */
    char allow_methods[64];
    char allow_headers[128];
    int  allow_credentials;   /* Access-Control-Allow-Credentials */
};

/* Returns 0=allow, -1=block */
int cors_check(struct cors_request *req, struct cors_response *resp);

/* Returns 1 if the request is a "simple" request (no preflight needed) */
int cors_is_simple(const struct cors_request *req);

/* Print a human-readable CORS decision */
void cors_print_decision(struct cors_request *req, struct cors_response *resp, int result);

#endif /* CORS_H */
