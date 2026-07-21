#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Case-insensitive strncmp */
static int strncasecmp_impl(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
    return 0;
}

/* Find "\r\n" in buffer, return pointer or NULL */
static const char *find_crlf(const char *p, const char *end) {
    while (p + 1 < end) {
        if (p[0] == '\r' && p[1] == '\n') return p;
        p++;
    }
    return NULL;
}

/* Copy line content (up to `max-1` chars) between `start` and the CRLF/end */
static int copy_line(char *dst, int max, const char *start, const char *end_of_line) {
    int len = (int)(end_of_line - start);
    if (len >= max) len = max - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return len;
}

/* Parse headers common to request and response.
   `pos` points just after the request/status line CRLF.
   Returns pointer to start of body (after blank line), or NULL on error. */
static const char *parse_headers(const char *pos, const char *end,
                                  char headers[HTTP_MAX_HEADERS][2][128],
                                  int *header_count,
                                  int *content_length, int *chunked) {
    *header_count = 0;
    *content_length = -1;
    *chunked = 0;

    while (pos < end) {
        const char *crlf = find_crlf(pos, end);
        if (!crlf) break;

        /* Blank line = end of headers */
        if (crlf == pos) {
            pos = crlf + 2;
            return pos;
        }

        if (*header_count >= HTTP_MAX_HEADERS) { pos = crlf + 2; continue; }

        /* Find ':' separator */
        const char *colon = memchr(pos, ':', crlf - pos);
        if (!colon) { pos = crlf + 2; continue; }

        int ni = *header_count;
        copy_line(headers[ni][0], 128, pos, colon);
        const char *val_start = colon + 1;
        while (val_start < crlf && *val_start == ' ') val_start++;
        copy_line(headers[ni][1], 128, val_start, crlf);

        /* Extract well-known headers */
        if (strncasecmp_impl(headers[ni][0], "content-length", 14) == 0) {
            *content_length = atoi(headers[ni][1]);
        }
        if (strncasecmp_impl(headers[ni][0], "transfer-encoding", 17) == 0) {
            if (strstr(headers[ni][1], "chunked")) *chunked = 1;
        }

        (*header_count)++;
        pos = crlf + 2;
    }
    return pos;
}

int http_parse_request(const char *raw, int len, struct http_request *out) {
    if (!raw || !out) return -1;
    memset(out, 0, sizeof(*out));

    const char *end = raw + len;
    const char *crlf = find_crlf(raw, end);
    if (!crlf) return -1;

    /* Parse request line: METHOD SP PATH SP VERSION */
    char req_line[512];
    copy_line(req_line, sizeof(req_line), raw, crlf);

    char *p = req_line;
    char *sp1 = strchr(p, ' ');
    if (!sp1) return -1;
    *sp1 = '\0';
    strncpy(out->method, p, sizeof(out->method) - 1);

    p = sp1 + 1;
    char *sp2 = strchr(p, ' ');
    if (!sp2) return -1;
    *sp2 = '\0';
    strncpy(out->path, p, sizeof(out->path) - 1);

    p = sp2 + 1;
    strncpy(out->version, p, sizeof(out->version) - 1);

    const char *body_start = parse_headers(crlf + 2, end,
                                            out->headers,
                                            &out->header_count,
                                            &out->content_length,
                                            &out->chunked);
    if (!body_start) return -1;

    /* Copy body */
    out->body_len = (int)(end - body_start);
    if (out->body_len < 0) out->body_len = 0;
    if (out->body_len >= HTTP_BODY_SIZE) out->body_len = HTTP_BODY_SIZE - 1;
    memcpy(out->body, body_start, out->body_len);
    out->body[out->body_len] = '\0';

    return 0;
}

int http_parse_response(const char *raw, int len, struct http_response *out) {
    if (!raw || !out) return -1;
    memset(out, 0, sizeof(*out));

    const char *end = raw + len;
    const char *crlf = find_crlf(raw, end);
    if (!crlf) return -1;

    char status_line[256];
    copy_line(status_line, sizeof(status_line), raw, crlf);

    char *p = status_line;
    char *sp1 = strchr(p, ' ');
    if (!sp1) return -1;
    *sp1 = '\0';
    strncpy(out->version, p, sizeof(out->version) - 1);

    p = sp1 + 1;
    out->status = atoi(p);
    char *sp2 = strchr(p, ' ');
    if (sp2) strncpy(out->reason, sp2 + 1, sizeof(out->reason) - 1);

    const char *body_start = parse_headers(crlf + 2, end,
                                            out->headers,
                                            &out->header_count,
                                            &out->content_length,
                                            &out->chunked);
    if (!body_start) return -1;

    out->body_len = (int)(end - body_start);
    if (out->body_len < 0) out->body_len = 0;
    if (out->body_len >= HTTP_BODY_SIZE) out->body_len = HTTP_BODY_SIZE - 1;
    memcpy(out->body, body_start, out->body_len);
    out->body[out->body_len] = '\0';

    return 0;
}

int http_parse_chunked_body(const char *raw, int len,
                             char *out_body, int out_size) {
    const char *p = raw;
    const char *end = raw + len;
    int written = 0;

    while (p < end) {
        /* Read chunk size line (hex) */
        const char *crlf = find_crlf(p, end);
        if (!crlf) break;

        char size_str[32] = {0};
        int sl = (int)(crlf - p);
        if (sl >= (int)sizeof(size_str)) sl = sizeof(size_str) - 1;
        memcpy(size_str, p, sl);
        /* Strip chunk extensions (;...) */
        char *semi = strchr(size_str, ';');
        if (semi) *semi = '\0';

        int chunk_size = (int)strtol(size_str, NULL, 16);
        p = crlf + 2;

        if (chunk_size == 0) break; /* last chunk */

        if (written + chunk_size > out_size - 1) {
            chunk_size = out_size - 1 - written;
        }
        if (p + chunk_size > end) break;

        memcpy(out_body + written, p, chunk_size);
        written += chunk_size;
        p += chunk_size;

        /* Skip trailing CRLF after chunk data */
        if (p + 1 < end && p[0] == '\r' && p[1] == '\n') p += 2;
    }
    out_body[written] = '\0';
    return written;
}

const char *http_get_header(struct http_request *req, const char *name) {
    size_t nlen = strlen(name);
    for (int i = 0; i < req->header_count; i++) {
        if (strncasecmp_impl(req->headers[i][0], name, nlen) == 0 &&
            req->headers[i][0][nlen] == '\0') {
            return req->headers[i][1];
        }
    }
    return NULL;
}

void http_print_request(const struct http_request *req) {
    printf("  %s %s %s\n", req->method, req->path, req->version);
    for (int i = 0; i < req->header_count; i++) {
        printf("  %s: %s\n", req->headers[i][0], req->headers[i][1]);
    }
    if (req->content_length >= 0)
        printf("  [Content-Length: %d]\n", req->content_length);
    if (req->chunked)
        printf("  [Transfer-Encoding: chunked]\n");
    if (req->body_len > 0)
        printf("  Body (%d bytes): %.*s\n", req->body_len,
               req->body_len, req->body);
}
