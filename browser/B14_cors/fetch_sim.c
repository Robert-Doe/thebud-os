#include "fetch_sim.h"
#include <stdio.h>
#include <string.h>

/* CORB-protected MIME types: if an <img> or <script> tag loads one of
   these, the response body is withheld even if CORS permits it. */
static int is_corb_protected(const char *mime) {
    return strstr(mime, "application/json") ||
           strstr(mime, "text/html") ||
           strstr(mime, "text/xml") ||
           strstr(mime, "application/xml");
}

/* Returns 1 if the resource origin differs from initiator origin */
static int is_cross_origin(const char *url, const char *initiator) {
    /* Simple heuristic: extract scheme+host from url */
    char url_origin[128] = {0};
    const char *sep = strstr(url, "://");
    if (!sep) return 0;
    sep += 3;
    const char *slash = strchr(sep, '/');
    size_t len = slash ? (size_t)(slash - url) : strlen(url);
    if (len >= sizeof(url_origin)) len = sizeof(url_origin) - 1;
    strncpy(url_origin, url, len);
    return strcmp(url_origin, initiator) != 0;
}

fetch_result_t fetch_sim(struct fetch_request *req,
                         struct cors_response *server_resp) {
    printf("  fetch('%s')\n", req->url);
    printf("  Initiator: %s @ %s\n", req->initiator_type, req->origin);

    /* Same-origin: always allow */
    if (!is_cross_origin(req->url, req->origin)) {
        printf("  Same-origin → ALLOW\n\n");
        return FETCH_OK;
    }

    /* Build a cors_request for the main request */
    struct cors_request cr;
    memset(&cr, 0, sizeof(cr));
    strncpy(cr.origin, req->origin, sizeof(cr.origin) - 1);
    strncpy(cr.method, req->method, sizeof(cr.method) - 1);
    strncpy(cr.headers, req->custom_headers, sizeof(cr.headers) - 1);
    cr.is_preflight = 0;

    /* Determine if preflight is needed */
    if (!cors_is_simple(&cr)) {
        printf("  Non-simple request → sending preflight OPTIONS\n");
        struct cors_request pre = cr;
        pre.is_preflight = 1;
        int pre_result = cors_check(&pre, server_resp);
        if (pre_result != 0) {
            printf("  Preflight denied → BLOCK\n\n");
            return FETCH_BLOCKED;
        }
        printf("  Preflight approved\n");
    }

    /* Credentials + ACAO:* check */
    if (req->with_credentials && strcmp(server_resp->allow_origin, "*") == 0) {
        printf("  BLOCK: credentials=true with ACAO:* is forbidden\n\n");
        return FETCH_BLOCKED;
    }

    /* CORS check on actual request */
    int result = cors_check(&cr, server_resp);
    if (result != 0) {
        printf("  CORS check failed → BLOCK\n\n");
        return FETCH_BLOCKED;
    }

    /* CORB check: if a no-CORS sub-resource request (img, script)
       receives a protected MIME type, block the response body */
    if ((strcmp(req->initiator_type, "img") == 0 ||
         strcmp(req->initiator_type, "script") == 0) &&
        is_corb_protected(req->response_type)) {
        printf("  CORB: %s loaded by <%s> → response body withheld\n\n",
               req->response_type, req->initiator_type);
        return FETCH_CORB;
    }

    printf("  CORS OK → ALLOW\n\n");
    return FETCH_OK;
}
