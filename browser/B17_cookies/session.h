#ifndef SESSION_H
#define SESSION_H

#define SESSION_TOKEN_LEN 32
#define MAX_SESSIONS      16

struct session {
    char token[SESSION_TOKEN_LEN * 2 + 1]; /* hex string */
    char username[64];
    long created_at;
    long expires_at;
    int  valid;
};

struct session_store {
    struct session sessions[MAX_SESSIONS];
    int count;
};

/* Generate a new session token (simulated random) */
void session_generate_token(char *out, int out_size);

/* Create a session for a user. Returns pointer to session or NULL. */
struct session *session_create(struct session_store *store,
                               const char *username, long now);

/* Look up a session by token. Returns NULL if not found or expired. */
struct session *session_lookup(struct session_store *store,
                               const char *token, long now);

/* Invalidate (logout) a session. */
void session_invalidate(struct session_store *store, const char *token);

#endif /* SESSION_H */
