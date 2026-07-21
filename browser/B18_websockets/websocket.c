#include "websocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* The magic GUID required by RFC 6455 */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* Simulated base64 alphabet for display */
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *src, int len,
                           char *dst, int dst_size) {
    int i = 0, j = 0;
    while (i < len && j + 4 < dst_size) {
        unsigned int v =
            (i < len ? (unsigned char)src[i++] : 0) << 16 |
            (i < len ? (unsigned char)src[i++] : 0) << 8  |
            (i < len ? (unsigned char)src[i++] : 0);
        dst[j++] = B64[(v >> 18) & 0x3F];
        dst[j++] = B64[(v >> 12) & 0x3F];
        dst[j++] = B64[(v >>  6) & 0x3F];
        dst[j++] = B64[(v      ) & 0x3F];
    }
    dst[j] = '\0';
}

/* Simulated SHA1: for demo, XOR-fold the key+GUID string.
   Not cryptographically meaningful — just shows the structure. */
static void fake_sha1(const char *input, int len,
                       unsigned char out[20]) {
    for (int i = 0; i < 20; i++) out[i] = 0;
    for (int i = 0; i < len; i++) out[i % 20] ^= (unsigned char)input[i];
    /* Mix for visual variety */
    for (int i = 0; i < 19; i++) out[i + 1] ^= out[i];
}

void ws_compute_accept(const char *key, char *out, int out_size) {
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    unsigned char sha1[20];
    fake_sha1(concat, (int)strlen(concat), sha1);
    base64_encode(sha1, 20, out, out_size);
}

int ws_handshake(struct ws_handshake_req *req,
                 struct ws_handshake_resp *resp) {
    memset(resp, 0, sizeof(*resp));

    /* Validate upgrade headers */
    if (strcmp(req->upgrade, "websocket") != 0) {
        printf("  [WS] Handshake failed: Upgrade header is not 'websocket'\n");
        return -1;
    }
    if (strcmp(req->ws_version, "13") != 0) {
        printf("  [WS] Handshake failed: Sec-WebSocket-Version must be 13\n");
        return -1;
    }
    if (!req->ws_key[0]) {
        printf("  [WS] Handshake failed: missing Sec-WebSocket-Key\n");
        return -1;
    }

    resp->status = 101;
    ws_compute_accept(req->ws_key, resp->ws_accept, sizeof(resp->ws_accept));
    return 0;
}

void ws_apply_mask(char *payload, int len, const uint8_t mask[4]) {
    for (int i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

int ws_encode_frame(struct ws_frame *f, uint8_t *out, int out_len) {
    int pos = 0;
    if (out_len < 2) return -1;

    /* Byte 0: FIN + opcode */
    out[pos++] = (uint8_t)((f->fin ? 0x80 : 0) | (f->opcode & 0x0F));

    /* Byte 1: MASK bit + payload length */
    uint64_t plen = (uint64_t)f->payload_actual_len;
    uint8_t mask_bit = f->masked ? 0x80 : 0;

    if (plen < 126) {
        if (pos + 1 > out_len) return -1;
        out[pos++] = mask_bit | (uint8_t)plen;
    } else if (plen <= 0xFFFF) {
        if (pos + 3 > out_len) return -1;
        out[pos++] = mask_bit | 126;
        out[pos++] = (uint8_t)(plen >> 8);
        out[pos++] = (uint8_t)(plen);
    } else {
        if (pos + 9 > out_len) return -1;
        out[pos++] = mask_bit | 127;
        for (int i = 7; i >= 0; i--)
            out[pos++] = (uint8_t)(plen >> (i * 8));
    }

    /* Masking key */
    if (f->masked) {
        if (pos + 4 > out_len) return -1;
        memcpy(out + pos, f->mask, 4);
        pos += 4;
    }

    /* Payload */
    if (pos + (int)plen > out_len) return -1;
    memcpy(out + pos, f->payload, (int)plen);
    if (f->masked) {
        ws_apply_mask((char *)(out + pos), (int)plen, f->mask);
    }
    pos += (int)plen;
    return pos;
}

int ws_decode_frame(const uint8_t *data, int len, struct ws_frame *out) {
    if (len < 2) return -1;
    memset(out, 0, sizeof(*out));

    int pos = 0;
    out->fin    = (data[pos] & 0x80) != 0;
    out->opcode = (data[pos] & 0x0F);
    pos++;

    out->masked     = (data[pos] & 0x80) != 0;
    uint64_t plen   = (data[pos] & 0x7F);
    pos++;

    if (plen == 126) {
        if (pos + 2 > len) return -1;
        plen = ((uint64_t)data[pos] << 8) | data[pos + 1];
        pos += 2;
    } else if (plen == 127) {
        if (pos + 8 > len) return -1;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | data[pos + i];
        pos += 8;
    }
    out->payload_len = plen;

    if (out->masked) {
        if (pos + 4 > len) return -1;
        memcpy(out->mask, data + pos, 4);
        pos += 4;
    }

    if (plen >= sizeof(out->payload)) plen = sizeof(out->payload) - 1;
    if (pos + (int)plen > len) return -1;
    memcpy(out->payload, data + pos, (int)plen);
    out->payload[(int)plen] = '\0';
    out->payload_actual_len = (int)plen;
    pos += (int)plen;

    if (out->masked) {
        ws_apply_mask(out->payload, (int)plen, out->mask);
    }
    return pos;
}

static const char *opcode_name(int op) {
    switch (op) {
        case WS_OP_TEXT:   return "TEXT";
        case WS_OP_BINARY: return "BINARY";
        case WS_OP_CLOSE:  return "CLOSE";
        case WS_OP_PING:   return "PING";
        case WS_OP_PONG:   return "PONG";
        default:           return "UNKNOWN";
    }
}

void ws_print_frame(const struct ws_frame *f) {
    printf("  Frame: FIN=%d opcode=%s masked=%d payload_len=%llu\n",
           f->fin, opcode_name(f->opcode), f->masked,
           (unsigned long long)f->payload_len);
    if (f->payload_actual_len > 0)
        printf("  Payload: '%.*s'\n", f->payload_actual_len, f->payload);
}

void ws_print_handshake(const struct ws_handshake_req *req,
                         const struct ws_handshake_resp *resp) {
    printf("  --> GET /ws HTTP/1.1\n");
    printf("      Host: %s\n", req->host);
    printf("      Upgrade: %s\n", req->upgrade);
    printf("      Connection: %s\n", req->connection);
    printf("      Sec-WebSocket-Key: %s\n", req->ws_key);
    printf("      Sec-WebSocket-Version: %s\n", req->ws_version);
    printf("      Origin: %s\n", req->origin);
    if (resp->status == 101) {
        printf("  <-- HTTP/1.1 101 Switching Protocols\n");
        printf("      Upgrade: websocket\n");
        printf("      Connection: Upgrade\n");
        printf("      Sec-WebSocket-Accept: %s\n", resp->ws_accept);
    } else {
        printf("  <-- HTTP/1.1 400 Bad Request (handshake failed)\n");
    }
}
