#include "session.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Simulated random bytes — not cryptographically secure (demo only) */
static void fake_random_bytes(unsigned char *buf, int len) {
    static unsigned int seed = 0xDEADBEEF;
    for (int i = 0; i < len; i++) {
        seed = seed * 1664525 + 1013904223;
        buf[i] = (unsigned char)(seed >> 16);
    }
}

void session_generate_token(char *out, int out_size) {
    unsigned char bytes[SESSION_TOKEN_LEN];
    fake_random_bytes(bytes, SESSION_TOKEN_LEN);
    int wrote = 0;
    for (int i = 0; i < SESSION_TOKEN_LEN && wrote + 2 < out_size; i++) {
        wrote += snprintf(out + wrote, out_size - wrote, "%02x", bytes[i]);
    }
    out[wrote] = '\0';
}

struct session *session_create(struct session_store *store,
                               const char *username, long now) {
    if (store->count >= MAX_SESSIONS) return NULL;
    struct session *s = &store->sessions[store->count++];
    memset(s, 0, sizeof(*s));
    session_generate_token(s->token, sizeof(s->token));
    strncpy(s->username, username, sizeof(s->username) - 1);
    s->created_at = now;
    s->expires_at = now + 3600; /* 1 hour */
    s->valid = 1;
    return s;
}

struct session *session_lookup(struct session_store *store,
                               const char *token, long now) {
    for (int i = 0; i < store->count; i++) {
        struct session *s = &store->sessions[i];
        if (s->valid && strcmp(s->token, token) == 0) {
            if (now > s->expires_at) {
                s->valid = 0;
                return NULL; /* expired */
            }
            return s;
        }
    }
    return NULL;
}

void session_invalidate(struct session_store *store, const char *token) {
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->sessions[i].token, token) == 0) {
            store->sessions[i].valid = 0;
        }
    }
}
