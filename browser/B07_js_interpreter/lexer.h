#ifndef LEXER_H
#define LEXER_H

/* Minimal lexer for simple JS-like expressions:
   numbers, strings, identifiers, operators, punctuation */

typedef enum {
    TOK_EOF,
    TOK_NUMBER,      /* 42, 3.14 */
    TOK_STRING,      /* "hello" */
    TOK_IDENT,       /* variable name */
    TOK_PLUS,        /* + */
    TOK_MINUS,       /* - */
    TOK_STAR,        /* * */
    TOK_SLASH,       /* / */
    TOK_ASSIGN,      /* = */
    TOK_EQ,          /* == */
    TOK_LPAREN,      /* ( */
    TOK_RPAREN,      /* ) */
    TOK_LBRACE,      /* { */
    TOK_RBRACE,      /* } */
    TOK_SEMICOLON,   /* ; */
    TOK_IF,          /* if */
    TOK_ELSE,        /* else */
    TOK_VAR,         /* var */
    TOK_UNKNOWN
} token_type_t;

typedef struct {
    token_type_t type;
    char         text[128];   /* raw text of the token */
    double       num_val;     /* populated for TOK_NUMBER */
} token_t;

typedef struct {
    const char *src;
    int         pos;
    int         len;
} lexer_t;

void    lexer_init(lexer_t *l, const char *src);
token_t lexer_next(lexer_t *l);
token_t lexer_peek(lexer_t *l);

#endif /* LEXER_H */
