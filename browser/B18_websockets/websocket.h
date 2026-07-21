#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>

/* WebSocket handshake request (HTTP Upgrade) */
struct ws_handshake_req {
    char upgrade[32];      /* "websocket" */
    char connection[32];   /* "Upgrade" */
    char ws_key[64];       /* base64-encoded 16 random bytes */
    char ws_version[8];    /* "13" */
    char origin[128];
    char host[128];
};

/* WebSocket handshake response */
struct ws_handshake_resp {
    int  status;           /* 101 Switching Protocols */
    char ws_accept[64];    /* base64(SHA1(key + GUID)) — simulated */
};

/* WebSocket frame */
struct ws_frame {
    int      fin;          /* 1 = final fragment */
    int      opcode;       /* 1=text, 2=binary, 8=close, 9=ping, 10=pong */
    int      masked;       /* 1 = payload is masked (client→server) */
    uint8_t  mask[4];
    uint64_t payload_len;
    char     payload[4096];
    int      payload_actual_len;
};

/* WS opcodes */
#define WS_OP_TEXT   1
#define WS_OP_BINARY 2
#define WS_OP_CLOSE  8
#define WS_OP_PING   9
#define WS_OP_PONG   10

/* Perform the WebSocket key hash (simulated, not real SHA1).
   In production: base64(SHA1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
   Here we simulate the process conceptually. */
void ws_compute_accept(const char *key, char *out, int out_size);

/* Complete a WS handshake.
   Returns 0 on success (101 status), -1 on failure. */
int ws_handshake(struct ws_handshake_req *req, struct ws_handshake_resp *resp);

/* Encode a frame into raw bytes. Returns bytes written. */
int ws_encode_frame(struct ws_frame *f, uint8_t *out, int out_len);

/* Decode raw bytes into a frame. Returns bytes consumed, or -1 on error. */
int ws_decode_frame(const uint8_t *data, int len, struct ws_frame *out);

/* Apply/remove masking to payload */
void ws_apply_mask(char *payload, int len, const uint8_t mask[4]);

void ws_print_frame(const struct ws_frame *f);
void ws_print_handshake(const struct ws_handshake_req *req,
                        const struct ws_handshake_resp *resp);

#endif /* WEBSOCKET_H */
