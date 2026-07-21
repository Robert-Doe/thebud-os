#include <stdio.h>
#include <string.h>
#include "http_parser.h"
#include "smuggling_demo.h"

static void banner(const char *s) {
    printf("\n══════════════════════════════════════════\n");
    printf(" %s\n", s);
    printf("══════════════════════════════════════════\n");
}

int main(void) {
    printf("B15 — HTTP/1.1 Parser & Request Smuggling\n");

    /* ── Basic GET request ────────────────────────────────────────── */
    banner("Parse: simple GET request");
    const char *get_raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: BobBrowser/1.0\r\n"
        "Accept: text/html\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    struct http_request req_get;
    int r = http_parse_request(get_raw, (int)strlen(get_raw), &req_get);
    printf("parse result: %d\n", r);
    http_print_request(&req_get);

    /* ── POST with body ───────────────────────────────────────────── */
    banner("Parse: POST with Content-Length body");
    const char *post_raw =
        "POST /login HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "username=bob&password=s3cr3t";
    struct http_request req_post;
    http_parse_request(post_raw, (int)strlen(post_raw), &req_post);
    http_print_request(&req_post);
    printf("  Content-Length field: %d\n", req_post.content_length);

    /* ── Chunked transfer encoding ────────────────────────────────── */
    banner("Parse: chunked transfer encoding");
    const char *chunked_raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "6\r\n"
        " World\r\n"
        "0\r\n"
        "\r\n";
    struct http_request req_chunked;
    http_parse_request(chunked_raw, (int)strlen(chunked_raw), &req_chunked);
    printf("  Chunked flag: %d\n", req_chunked.chunked);
    /* Decode the body */
    char decoded[256] = {0};
    int dlen = http_parse_chunked_body(req_chunked.body, req_chunked.body_len,
                                       decoded, sizeof(decoded));
    printf("  Decoded body (%d bytes): '%s'\n", dlen, decoded);

    /* ── HTTP response parse ──────────────────────────────────────── */
    banner("Parse: HTTP/1.1 response");
    const char *resp_raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 13\r\n"
        "Set-Cookie: session=abc123; HttpOnly\r\n"
        "\r\n"
        "<h1>Hello</h1>";
    struct http_response resp;
    http_parse_response(resp_raw, (int)strlen(resp_raw), &resp);
    printf("  Status: %d %s\n", resp.status, resp.reason);
    printf("  Headers (%d):\n", resp.header_count);
    for (int i = 0; i < resp.header_count; i++)
        printf("    %s: %s\n", resp.headers[i][0], resp.headers[i][1]);
    printf("  Body: %s\n", resp.body);

    /* ── Smuggling attacks ────────────────────────────────────────── */
    demo_cl_te();
    demo_te_cl();
    smuggling_summary();

    return 0;
}
