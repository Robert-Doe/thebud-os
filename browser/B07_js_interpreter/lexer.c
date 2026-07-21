#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void lexer_init(lexer_t *l, const char *src) {
    l->src = src;
    l->pos = 0;
    l->len = (int)strlen(src);
}

static void skip_whitespace(lexer_t *l) {
    while (l->pos < l->len && isspace((unsigned char)l->src[l->pos]))
        l->pos++;
}

static token_t make_tok(token_type_t type, const char *text) {
    token_t t;
    t.type = type;
    t.num_val = 0;
    strncpy(t.text, text, sizeof(t.text) - 1);
    t.text[sizeof(t.text) - 1] = '\0';
    return t;
}

token_t lexer_next(lexer_t *l) {
    skip_whitespace(l);
    if (l->pos >= l->len) return make_tok(TOK_EOF, "");

    char c = l->src[l->pos];

    /* Single-char tokens */
    if (c == '+') { l->pos++; return make_tok(TOK_PLUS,      "+"); }
    if (c == '-') { l->pos++; return make_tok(TOK_MINUS,     "-"); }
    if (c == '*') { l->pos++; return make_tok(TOK_STAR,      "*"); }
    if (c == '/') { l->pos++; return make_tok(TOK_SLASH,     "/"); }
    if (c == '(') { l->pos++; return make_tok(TOK_LPAREN,    "("); }
    if (c == ')') { l->pos++; return make_tok(TOK_RPAREN,    ")"); }
    if (c == '{') { l->pos++; return make_tok(TOK_LBRACE,    "{"); }
    if (c == '}') { l->pos++; return make_tok(TOK_RBRACE,    "}"); }
    if (c == ';') { l->pos++; return make_tok(TOK_SEMICOLON, ";"); }

    /* == vs = */
    if (c == '=') {
        l->pos++;
        if (l->pos < l->len && l->src[l->pos] == '=') {
            l->pos++;
            return make_tok(TOK_EQ, "==");
        }
        return make_tok(TOK_ASSIGN, "=");
    }

    /* Number */
    if (isdigit((unsigned char)c) || (c == '.' && l->pos + 1 < l->len && isdigit((unsigned char)l->src[l->pos+1]))) {
        int start = l->pos;
        while (l->pos < l->len && (isdigit((unsigned char)l->src[l->pos]) || l->src[l->pos] == '.'))
            l->pos++;
        token_t t;
        t.type = TOK_NUMBER;
        int len = l->pos - start;
        if (len >= (int)sizeof(t.text)) len = (int)sizeof(t.text) - 1;
        memcpy(t.text, l->src + start, len);
        t.text[len] = '\0';
        t.num_val = atof(t.text);
        return t;
    }

    /* String literal */
    if (c == '"') {
        l->pos++;
        int start = l->pos;
        while (l->pos < l->len && l->src[l->pos] != '"') l->pos++;
        token_t t;
        t.type = TOK_STRING;
        t.num_val = 0;
        int len = l->pos - start;
        if (len >= (int)sizeof(t.text)) len = (int)sizeof(t.text) - 1;
        memcpy(t.text, l->src + start, len);
        t.text[len] = '\0';
        if (l->pos < l->len) l->pos++; /* skip closing " */
        return t;
    }

    /* Identifier or keyword */
    if (isalpha((unsigned char)c) || c == '_') {
        int start = l->pos;
        while (l->pos < l->len && (isalnum((unsigned char)l->src[l->pos]) || l->src[l->pos] == '_'))
            l->pos++;
        token_t t;
        t.type = TOK_IDENT;
        t.num_val = 0;
        int len = l->pos - start;
        if (len >= (int)sizeof(t.text)) len = (int)sizeof(t.text) - 1;
        memcpy(t.text, l->src + start, len);
        t.text[len] = '\0';
        /* Check keywords */
        if (strcmp(t.text, "if")   == 0) t.type = TOK_IF;
        if (strcmp(t.text, "else") == 0) t.type = TOK_ELSE;
        if (strcmp(t.text, "var")  == 0) t.type = TOK_VAR;
        return t;
    }

    l->pos++;
    return make_tok(TOK_UNKNOWN, "?");
}

token_t lexer_peek(lexer_t *l) {
    int saved = l->pos;
    token_t t = lexer_next(l);
    l->pos = saved;
    return t;
}
