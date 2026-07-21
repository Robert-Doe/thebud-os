#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* Simple AST for a subset of JavaScript */

typedef enum {
    NODE_NUMBER,
    NODE_STRING,
    NODE_IDENT,
    NODE_BINOP,
    NODE_ASSIGN,
    NODE_IF,
    NODE_BLOCK,
    NODE_VAR_DECL
} node_type_t;

#define MAX_CHILDREN 4
#define MAX_STMTS    32

typedef struct ast_node ast_node_t;
struct ast_node {
    node_type_t  type;
    char         op;            /* for NODE_BINOP: '+', '-', '*', '/' */
    double       num_val;       /* for NODE_NUMBER */
    char         str_val[128];  /* for NODE_STRING and NODE_IDENT */
    ast_node_t  *children[MAX_CHILDREN];
    int          num_children;
};

typedef struct {
    ast_node_t *stmts[MAX_STMTS];
    int         count;
} program_t;

typedef struct {
    lexer_t  lex;
    token_t  cur;
} parser_t;

void      parser_init(parser_t *p, const char *src);
program_t parser_parse(parser_t *p);

/* Free all AST nodes */
void ast_free(ast_node_t *node);
void program_free(program_t *prog);

#endif /* PARSER_H */
