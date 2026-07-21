#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "websocket.h"
#include "postmessage.h"

static void banner(const char *s) {
    printf("\n══════════════════════════════════════════════\n");
    printf(" %s\n", s);
    printf("══════════════════════════════════════════════\n");
}

int main(void) {
    printf("B18 — WebSockets & postMessage\n");

    /* ── Demo 1: WebSocket Handshake ─────────────────────────────── */
    banner("1. WebSocket HTTP Upgrade Handshake");

    struct ws_handshake_req req = {
        .upgrade    = "websocket",
        .connection = "Upgrade",
        .ws_key     = "dGhlIHNhbXBsZSBub25jZQ==",   /* RFC 6455 example key */
        .ws_version = "13",
        .origin     = "https://example.com",
        .host       = "server.example.com"
    };

    struct ws_handshake_resp resp;
    int r = ws_handshake(&req, &resp);
    ws_print_handshake(&req, &resp);
    printf("\n  Handshake result: %s\n", r == 0 ? "SUCCESS (101)" : "FAILED");
    printf("  Accept key (simulated SHA1+base64): %s\n", resp.ws_accept);
    printf("  Note: real Accept = base64(SHA1(key + WS_GUID))\n");
    printf("  The GUID prevents non-WS servers from accidentally\n");
    printf("  accepting a WS upgrade (they wouldn't know the GUID).\n");

    /* Invalid handshake */
    printf("\n  -- Invalid handshake (wrong version) --\n");
    struct ws_handshake_req bad_req = req;
    strcpy(bad_req.ws_version, "12");
    ws_handshake(&bad_req, &resp);

    /* ── Demo 2: Frame Encode/Decode ─────────────────────────────── */
    banner("2. WebSocket Frame Encode & Decode (Text, Masked)");

    struct ws_frame send_frame = {
        .fin                = 1,
        .opcode             = WS_OP_TEXT,
        .masked             = 1,
        .mask               = {0x37, 0xfa, 0x21, 0x3d},
        .payload_actual_len = 16
    };
    memcpy(send_frame.payload, "Hello WebSocket!", 16);

    uint8_t wire[256];
    int wire_len = ws_encode_frame(&send_frame, wire, sizeof(wire));
    printf("Encoded frame (%d bytes on wire):\n  ", wire_len);
    for (int i = 0; i < wire_len; i++) printf("%02x ", wire[i]);
    printf("\n\n");

    /* Decode it back */
    struct ws_frame recv_frame;
    int consumed = ws_decode_frame(wire, wire_len, &recv_frame);
    printf("Decoded frame (consumed %d bytes):\n", consumed);
    ws_print_frame(&recv_frame);

    /* Unmasked server→client frame */
    printf("\nServer→client frame (unmasked, as per RFC):\n");
    struct ws_frame server_frame = {
        .fin                = 1,
        .opcode             = WS_OP_TEXT,
        .masked             = 0,
        .payload_actual_len = 5
    };
    memcpy(server_frame.payload, "Hello", 5);
    int slen = ws_encode_frame(&server_frame, wire, sizeof(wire));
    printf("  Wire (%d bytes): ", slen);
    for (int i = 0; i < slen; i++) printf("%02x ", wire[i]);
    printf("\n");

    struct ws_frame server_recv;
    ws_decode_frame(wire, slen, &server_recv);
    ws_print_frame(&server_recv);

    /* ── Demo 3: postMessage — Good Usage ────────────────────────── */
    banner("3. postMessage: Good — Specific Target Origin");

    struct pm_message good_msg = {
        .origin        = "https://example.com",
        .data          = "userData={name:'Alice',role:'admin'}",
        .target_origin = "https://app.example.com"
    };

    printf("Sender calls: window.postMessage(data, 'https://app.example.com')\n\n");
    int deliver = pm_should_deliver(&good_msg, "https://app.example.com");
    printf("  Deliver to 'https://app.example.com': %s\n", deliver ? "YES" : "NO");
    pm_receive_safe(&good_msg, "https://app.example.com", "https://example.com");

    /* Wrong receiver */
    printf("\n  Message sent to wrong receiver:\n");
    deliver = pm_should_deliver(&good_msg, "https://evil.com");
    printf("  Deliver to 'https://evil.com': %s\n", deliver ? "YES" : "NO");

    /* ── Demo 4: postMessage — Bad: wildcard target ───────────────── */
    banner("4. postMessage: Bad — Wildcard Target '*'");

    struct pm_message wild_msg = {
        .origin        = "https://example.com",
        .data          = "apiToken=SECRET_TOKEN_12345",
        .target_origin = "*"   /* dangerous with sensitive data */
    };

    printf("Sender: window.postMessage(sensitiveData, '*')\n\n");
    printf("  Delivers to 'https://app.example.com': %s\n",
           pm_should_deliver(&wild_msg, "https://app.example.com") ? "YES" : "NO");
    printf("  Delivers to 'https://evil.com': %s\n",
           pm_should_deliver(&wild_msg, "https://evil.com") ? "YES (DANGER!)" : "NO");
    printf("\n  If evil.com's iframe is embedded on the page, it receives\n");
    printf("  the sensitive token. '*' should NEVER be used with secrets.\n");

    /* ── Demo 5: Missing Origin Check ────────────────────────────── */
    banner("5. postMessage Exploit: Missing event.origin Check");

    /* Legitimate message from trusted origin */
    struct pm_message legit = {
        .origin        = "https://trusted.com",
        .data          = "action=login&user=alice",
        .target_origin = "*"
    };

    /* Attacker's message from evil.com */
    struct pm_message attack = {
        .origin        = "https://evil.com",
        .data          = "action=deleteAccount&user=alice",
        .target_origin = "*"
    };

    printf("Trusted message from 'https://trusted.com':\n");
    pm_receive_safe(&legit, "https://app.com", "https://trusted.com");

    printf("\nAttacker message from 'https://evil.com':\n");
    printf("Safe receiver (checks origin):\n");
    pm_receive_safe(&attack, "https://app.com", "https://trusted.com");

    printf("\nUnsafe receiver (no origin check):\n");
    pm_receive_unsafe(&attack, "https://app.com");

    /* ── Demo 6: CSWSH ────────────────────────────────────────────── */
    banner("6. CSWSH — Cross-Site WebSocket Hijacking");
    printf("WebSocket upgrade requests include cookies (same-origin rules\n");
    printf("do NOT apply to WS upgrades like they do to CORS fetch()).\n\n");

    struct ws_handshake_req cswsh_req = {
        .upgrade    = "websocket",
        .connection = "Upgrade",
        .ws_key     = "rBE39dKoMm6JXpT1SyxfVA==",
        .ws_version = "13",
        .origin     = "https://evil.com",   /* attacker's page */
        .host       = "chat.example.com"
    };

    printf("Attacker page at evil.com opens:\n");
    printf("  new WebSocket('wss://chat.example.com/ws')\n\n");
    printf("  The browser sends the WS Upgrade request with:\n");
    printf("    Origin: %s\n", cswsh_req.origin);
    printf("    Cookie: session=VICTIM_TOKEN  (auto-attached!)\n\n");
    printf("  If chat.example.com doesn't validate the Origin header:\n");
    printf("    → Server accepts the connection authenticated as victim\n");
    printf("    → Attacker reads/writes victim's chat messages\n");
    printf("\nFix: server must validate Origin header on WS upgrade.\n");
    printf("     Treat WS upgrade like a CSRF-sensitive request.\n");
    printf("     Require a CSRF token or SameSite cookie.\n");

    printf("\n── Summary ─────────────────────────────────────────────\n");
    printf("WebSocket security rules:\n");
    printf("  1. Server MUST validate Origin header on upgrade.\n");
    printf("  2. Client frames MUST be masked (prevents cache poisoning).\n");
    printf("  3. The WS GUID ensures WS-unaware servers reject upgrades.\n");
    printf("postMessage security rules:\n");
    printf("  1. Sender: use specific targetOrigin, never '*' with secrets.\n");
    printf("  2. Receiver: ALWAYS check event.origin before processing.\n");
    printf("  3. Never eval() or execute event.data without validation.\n");

    return 0;
}
