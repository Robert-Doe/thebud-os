#include "cors.h"
#include <stdio.h>
#include <string.h>

/* Simple methods: GET, POST, HEAD */
static int is_simple_method(const char *m) {
    return strcmp(m, "GET") == 0 ||
           strcmp(m, "POST") == 0 ||
           strcmp(m, "HEAD") == 0;
}

/* Simple headers (subset that doesn't trigger preflight) */
static int has_only_simple_headers(const char *hdrs) {
    if (!hdrs || !*hdrs) return 1;
    /* If any non-simple header is present, preflight is needed */
    const char *simple[] = {
        "accept", "accept-language", "content-language", "content-type", NULL
    };
    /* For demo: if the string is empty or "content-type" only → simple */
    char lower[256];
    strncpy(lower, hdrs, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (int i = 0; lower[i]; i++)
        if (lower[i] >= 'A' && lower[i] <= 'Z') lower[i] += 32;

    for (int i = 0; simple[i]; i++) {
        if (strstr(lower, simple[i])) {
            /* contains a simple header name — continue */
        }
    }
    /* Detect non-simple headers */
    if (strstr(lower, "x-") || strstr(lower, "authorization") ||
        strstr(lower, "x-custom")) {
        return 0;
    }
    return 1;
}

int cors_is_simple(const struct cors_request *req) {
    return is_simple_method(req->method) && has_only_simple_headers(req->headers);
}

int cors_check(struct cors_request *req, struct cors_response *resp) {
    /* Null origin is special — sandboxed/data: context */
    int null_origin = (strcmp(req->origin, "null") == 0);

    /* No origin header → same-origin request, always allow */
    if (!req->origin[0]) return 0;

    /* Check if ACAO permits this origin */
    int origin_ok = 0;
    if (strcmp(resp->allow_origin, "*") == 0) {
        origin_ok = 1;
        /* ACAO:* with credentials is forbidden */
        if (resp->allow_credentials) {
            printf("  [CORS BLOCK] ACAO:* with credentials=true is forbidden by spec\n");
            return -1;
        }
    } else if (strcmp(resp->allow_origin, req->origin) == 0) {
        origin_ok = 1;
    } else if (null_origin && strcmp(resp->allow_origin, "null") == 0) {
        /* Explicitly allow null origin — dangerous but technically valid */
        origin_ok = 1;
    }

    if (!origin_ok) return -1;

    /* For preflight: also check method and headers */
    if (req->is_preflight) {
        int method_ok = 0;
        if (strstr(resp->allow_methods, req->method)) method_ok = 1;
        if (!method_ok) {
            printf("  [CORS BLOCK] Method '%s' not in Allow-Methods\n", req->method);
            return -1;
        }
        if (req->headers[0] && !strstr(resp->allow_headers, req->headers)) {
            printf("  [CORS BLOCK] Header '%s' not in Allow-Headers\n", req->headers);
            return -1;
        }
    }

    return 0;
}

void cors_print_decision(struct cors_request *req, struct cors_response *resp, int result) {
    printf("  Origin:  %s\n", req->origin[0] ? req->origin : "(same-origin)");
    printf("  Method:  %s%s\n", req->method, req->is_preflight ? " (preflight)" : "");
    if (req->headers[0])
        printf("  Headers: %s\n", req->headers);
    printf("  ACAO:    %s\n", resp->allow_origin[0] ? resp->allow_origin : "(none)");
    printf("  Result:  %s\n\n", result == 0 ? "ALLOW" : "BLOCK");
}
