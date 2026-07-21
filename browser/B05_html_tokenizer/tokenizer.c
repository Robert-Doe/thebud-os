#include "tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void tokenizer_init(tokenizer_t *t, const char *html)
{
    t->src = html;
    t->pos = 0;
    t->len = (int)strlen(html);
}

static char peek(tokenizer_t *t) {
    if (t->pos >= t->len) return '\0';
    return t->src[t->pos];
}
static char consume(tokenizer_t *t) {
    if (t->pos >= t->len) return '\0';
    return t->src[t->pos++];
}

/* Skip whitespace */
static void skip_ws(tokenizer_t *t) {
    while (t->pos < t->len && isspace((unsigned char)t->src[t->pos]))
        t->pos++;
}

/* Read an identifier (tag name, attr name) into buf */
static void read_ident(tokenizer_t *t, char *buf, int max) {
    int i = 0;
    while (t->pos < t->len && i < max - 1) {
        char c = t->src[t->pos];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_' && c != ':') break;
        buf[i++] = (char)tolower((unsigned char)c);
        t->pos++;
    }
    buf[i] = '\0';
}

/* Read a quoted or unquoted attribute value */
static void read_attr_value(tokenizer_t *t, char *buf, int max) {
    int i = 0;
    char delim = '\0';
    if (peek(t) == '"' || peek(t) == '\'') {
        delim = consume(t);
        while (t->pos < t->len && i < max - 1) {
            char c = consume(t);
            if (c == delim) break;
            buf[i++] = c;
        }
    } else {
        while (t->pos < t->len && i < max - 1) {
            char c = t->src[t->pos];
            if (isspace((unsigned char)c) || c == '>') break;
            buf[i++] = c;
            t->pos++;
        }
    }
    buf[i] = '\0';
}

/* ---------------------------------------------------------------
 * We store pending attributes in a small internal queue so that
 * we can emit TOK_ATTR tokens one at a time after TOK_START_TAG.
 * --------------------------------------------------------------- */
#define MAX_ATTRS 8
typedef struct { char name[32]; char value[128]; } attr_t;

static attr_t   pending_attrs[MAX_ATTRS];
static int      pending_attr_count = 0;
static int      pending_attr_idx   = 0;

int tokenizer_next(tokenizer_t *t, token_t *out)
{
    memset(out, 0, sizeof(*out));

    /* Emit any pending attribute tokens first */
    if (pending_attr_idx < pending_attr_count) {
        out->type = TOK_ATTR;
        strncpy(out->attr_name,  pending_attrs[pending_attr_idx].name,  31);
        strncpy(out->attr_value, pending_attrs[pending_attr_idx].value, 127);
        pending_attr_idx++;
        return 0;
    }

    /* Skip whitespace at top level */
    if (t->pos >= t->len) {
        out->type = TOK_EOF;
        return -1;
    }

    if (peek(t) == '<') {
        consume(t); /* eat '<' */

        if (peek(t) == '/') {
            /* End tag </foo> */
            consume(t);
            char tag[32] = {0};
            read_ident(t, tag, sizeof(tag));
            while (t->pos < t->len && peek(t) != '>') consume(t);
            if (peek(t) == '>') consume(t);
            out->type = TOK_END_TAG;
            strncpy(out->tag, tag, 31);
            return 0;
        }

        /* Start tag <foo attr=val ...> */
        char tag[32] = {0};
        read_ident(t, tag, sizeof(tag));
        out->type = TOK_START_TAG;
        strncpy(out->tag, tag, 31);

        /* Parse attributes */
        pending_attr_count = 0;
        pending_attr_idx   = 0;

        while (t->pos < t->len) {
            skip_ws(t);
            if (peek(t) == '>' || peek(t) == '\0') break;
            if (peek(t) == '/') { consume(t); continue; } /* self-closing slash */

            char aname[32] = {0};
            read_ident(t, aname, sizeof(aname));
            if (aname[0] == '\0') { consume(t); continue; }

            char aval[128] = {0};
            skip_ws(t);
            if (peek(t) == '=') {
                consume(t);
                skip_ws(t);
                read_attr_value(t, aval, sizeof(aval));
            }

            if (pending_attr_count < MAX_ATTRS) {
                strncpy(pending_attrs[pending_attr_count].name,  aname, 31);
                strncpy(pending_attrs[pending_attr_count].value, aval,  127);
                pending_attr_count++;
            }
        }
        if (peek(t) == '>') consume(t);
        return 0;

    } else {
        /* Text node: read until next '<' */
        int i = 0;
        while (t->pos < t->len && peek(t) != '<' && i < 255) {
            out->text[i++] = consume(t);
        }
        out->text[i] = '\0';
        /* Collapse leading/trailing whitespace for readability */
        if (i == 0) {
            /* empty text — recurse */
            return tokenizer_next(t, out);
        }
        out->type = TOK_TEXT;
        return 0;
    }
}

const char *token_type_name(token_type_t tt) {
    switch (tt) {
        case TOK_START_TAG: return "START_TAG";
        case TOK_END_TAG:   return "END_TAG";
        case TOK_TEXT:      return "TEXT";
        case TOK_ATTR:      return "ATTR";
        case TOK_EOF:       return "EOF";
        default:            return "?";
    }
}
