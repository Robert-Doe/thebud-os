#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ast_node_t *new_node(node_type_t type) {
    ast_node_t *n = calloc(1, sizeof(ast_node_t));
    n->type = type;
    return n;
}

void parser_init(parser_t *p, const char *src) {
    lexer_init(&p->lex, src);
    p->cur = lexer_next(&p->lex);
}

static token_t advance(parser_t *p) {
    token_t t = p->cur;
    p->cur = lexer_next(&p->lex);
    return t;
}

static int check(parser_t *p, token_type_t type) {
    return p->cur.type == type;
}

static token_t expect(parser_t *p, token_type_t type) {
    if (!check(p, type)) {
        fprintf(stderr, "Parse error: expected %d got %d ('%s')\n", type, p->cur.type, p->cur.text);
    }
    return advance(p);
}

/* Forward declarations */
static ast_node_t *parse_expr(parser_t *p);
static ast_node_t *parse_stmt(parser_t *p);

static ast_node_t *parse_primary(parser_t *p) {
    if (check(p, TOK_NUMBER)) {
        token_t t = advance(p);
        ast_node_t *n = new_node(NODE_NUMBER);
        n->num_val = t.num_val;
        return n;
    }
    if (check(p, TOK_STRING)) {
        token_t t = advance(p);
        ast_node_t *n = new_node(NODE_STRING);
        strncpy(n->str_val, t.text, sizeof(n->str_val) - 1);
        return n;
    }
    if (check(p, TOK_IDENT)) {
        token_t t = advance(p);
        ast_node_t *n = new_node(NODE_IDENT);
        strncpy(n->str_val, t.text, sizeof(n->str_val) - 1);
        return n;
    }
    if (check(p, TOK_LPAREN)) {
        advance(p);
        ast_node_t *n = parse_expr(p);
        expect(p, TOK_RPAREN);
        return n;
    }
    fprintf(stderr, "Parse error: unexpected token '%s'\n", p->cur.text);
    advance(p);
    return new_node(NODE_NUMBER); /* fallback */
}

static ast_node_t *parse_mul(parser_t *p) {
    ast_node_t *left = parse_primary(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH)) {
        char op = p->cur.text[0];
        advance(p);
        ast_node_t *right = parse_primary(p);
        ast_node_t *n = new_node(NODE_BINOP);
        n->op = op;
        n->children[0] = left;
        n->children[1] = right;
        n->num_children = 2;
        left = n;
    }
    return left;
}

static ast_node_t *parse_add(parser_t *p) {
    ast_node_t *left = parse_mul(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        char op = p->cur.text[0];
        advance(p);
        ast_node_t *right = parse_mul(p);
        ast_node_t *n = new_node(NODE_BINOP);
        n->op = op;
        n->children[0] = left;
        n->children[1] = right;
        n->num_children = 2;
        left = n;
    }
    return left;
}

static ast_node_t *parse_expr(parser_t *p) {
    /* Check for assignment: IDENT = expr */
    if (check(p, TOK_IDENT)) {
        lexer_t saved_lex = p->lex;
        token_t saved_cur = p->cur;
        token_t name_tok = advance(p);
        if (check(p, TOK_ASSIGN)) {
            advance(p); /* consume = */
            ast_node_t *rhs = parse_expr(p);
            ast_node_t *n = new_node(NODE_ASSIGN);
            strncpy(n->str_val, name_tok.text, sizeof(n->str_val) - 1);
            n->children[0] = rhs;
            n->num_children = 1;
            return n;
        }
        /* Not assignment — restore */
        p->lex = saved_lex;
        p->cur = saved_cur;
    }
    return parse_add(p);
}

static ast_node_t *parse_block(parser_t *p) {
    ast_node_t *block = new_node(NODE_BLOCK);
    expect(p, TOK_LBRACE);
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (block->num_children < MAX_CHILDREN) {
            block->children[block->num_children++] = parse_stmt(p);
        } else {
            parse_stmt(p); /* parse and discard */
        }
    }
    expect(p, TOK_RBRACE);
    return block;
}

static ast_node_t *parse_stmt(parser_t *p) {
    /* if statement */
    if (check(p, TOK_IF)) {
        advance(p);
        expect(p, TOK_LPAREN);
        ast_node_t *cond = parse_expr(p);
        expect(p, TOK_RPAREN);
        ast_node_t *then_block = parse_block(p);
        ast_node_t *n = new_node(NODE_IF);
        n->children[0] = cond;
        n->children[1] = then_block;
        n->num_children = 2;
        if (check(p, TOK_ELSE)) {
            advance(p);
            ast_node_t *else_block = parse_block(p);
            n->children[2] = else_block;
            n->num_children = 3;
        }
        return n;
    }
    /* var declaration */
    if (check(p, TOK_VAR)) {
        advance(p);
        token_t name = expect(p, TOK_IDENT);
        ast_node_t *n = new_node(NODE_VAR_DECL);
        strncpy(n->str_val, name.text, sizeof(n->str_val) - 1);
        n->num_children = 0;
        if (check(p, TOK_ASSIGN)) {
            advance(p);
            n->children[0] = parse_expr(p);
            n->num_children = 1;
        }
        if (check(p, TOK_SEMICOLON)) advance(p);
        return n;
    }
    /* Expression statement */
    ast_node_t *n = parse_expr(p);
    if (check(p, TOK_SEMICOLON)) advance(p);
    return n;
}

program_t parser_parse(parser_t *p) {
    program_t prog;
    prog.count = 0;
    while (!check(p, TOK_EOF) && prog.count < MAX_STMTS) {
        prog.stmts[prog.count++] = parse_stmt(p);
    }
    return prog;
}

void ast_free(ast_node_t *node) {
    if (!node) return;
    for (int i = 0; i < node->num_children; i++)
        ast_free(node->children[i]);
    free(node);
}

void program_free(program_t *prog) {
    for (int i = 0; i < prog->count; i++)
        ast_free(prog->stmts[i]);
    prog->count = 0;
}
