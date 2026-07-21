#ifndef FETCH_SIM_H
#define FETCH_SIM_H

#include "cors.h"

typedef enum {
    FETCH_OK      = 0,
    FETCH_BLOCKED = 1,
    FETCH_CORB    = 2   /* Cross-Origin Read Blocking */
} fetch_result_t;

/* Simulated fetch() request */
struct fetch_request {
    char url[256];
    char origin[128];       /* initiating document origin */
    char method[16];
    char custom_headers[256];
    int  with_credentials;
    char initiator_type[32]; /* "script", "img", "xhr", "fetch" */
    char response_type[64];  /* MIME type of response */
};

/* Run a simulated fetch with CORS + CORB enforcement.
   Returns FETCH_OK, FETCH_BLOCKED, or FETCH_CORB. */
fetch_result_t fetch_sim(struct fetch_request *req,
                         struct cors_response *server_resp);

#endif /* FETCH_SIM_H */
