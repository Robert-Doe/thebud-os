#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define HTTP_MAX_HEADERS 32
#define HTTP_BODY_SIZE   4096

struct http_request {
    char method[16];
    char path[256];
    char version[16];
    char headers[HTTP_MAX_HEADERS][2][128]; /* [n][0=name, 1=value] */
    int  header_count;
    char body[HTTP_BODY_SIZE];
    int  body_len;
    int  content_length;  /* from Content-Length header, -1 if absent */
    int  chunked;         /* 1 if Transfer-Encoding: chunked */
};

struct http_response {
    int  status;
    char reason[64];
    char version[16];
    char headers[HTTP_MAX_HEADERS][2][128];
    int  header_count;
    char body[HTTP_BODY_SIZE];
    int  body_len;
    int  content_length;
    int  chunked;
};

/* Parse a raw HTTP/1.1 request.
   Returns 0 on success, -1 on error. */
int http_parse_request(const char *raw, int len, struct http_request *out);

/* Parse a raw HTTP/1.1 response.
   Returns 0 on success, -1 on error. */
int http_parse_response(const char *raw, int len, struct http_response *out);

/* Decode a chunked body.
   Returns number of decoded bytes, or -1 on error. */
int http_parse_chunked_body(const char *raw, int len,
                            char *out_body, int out_size);

/* Look up a header value by name (case-insensitive).
   Returns pointer to value string, or NULL if not found. */
const char *http_get_header(struct http_request *req, const char *name);

void http_print_request(const struct http_request *req);

#endif /* HTTP_PARSER_H */
