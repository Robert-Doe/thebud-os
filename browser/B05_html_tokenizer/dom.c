#include "dom.h"
#include "tokenizer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * Static node pool — no malloc needed.
 * --------------------------------------------------------------- */
#define POOL_SIZE 128
static dom_node_t node_pool[POOL_SIZE];
static int        pool_idx = 0;

void dom_reset_pool(void) { pool_idx = 0; }

dom_node_t *dom_new_node(void)
{
    if (pool_idx >= POOL_SIZE) return NULL;
    dom_node_t *n = &node_pool[pool_idx++];
    memset(n, 0, sizeof(*n));
    return n;
}

void dom_append_child(dom_node_t *parent, dom_node_t *child)
{
    if (!parent || !child) return;
    if (parent->child_count < DOM_MAX_CHILDREN)
        parent->children[parent->child_count++] = child;
}

void dom_add_attr(dom_node_t *node, const char *name, const char *value)
{
    if (!node || node->attr_count >= DOM_MAX_ATTRS) return;
    strncpy(node->attrs[node->attr_count].name,  name,  31);
    strncpy(node->attrs[node->attr_count].value, value, 127);
    node->attr_count++;
}

/* ---------------------------------------------------------------
 * DOM printer
 * --------------------------------------------------------------- */
void dom_print(const dom_node_t *node, int depth)
{
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");

    if (node->is_text) {
        /* Only print non-whitespace text nodes */
        int has_nonws = 0;
        for (int i = 0; node->text[i]; i++)
            if (node->text[i] != ' ' && node->text[i] != '\n' &&
                node->text[i] != '\t' && node->text[i] != '\r')
            { has_nonws = 1; break; }
        if (has_nonws)
            printf("[TEXT] \"%s\"\n", node->text);
        else {
            /* skip whitespace-only text */
            return;
        }
    } else {
        printf("<%s", node->tag);
        for (int i = 0; i < node->attr_count; i++)
            printf(" %s=\"%s\"", node->attrs[i].name, node->attrs[i].value);
        printf(">\n");
        for (int i = 0; i < node->child_count; i++)
            dom_print(node->children[i], depth + 1);
    }
}

/* ---------------------------------------------------------------
 * Parse stack helpers
 * --------------------------------------------------------------- */
#define STACK_MAX 32
typedef struct { dom_node_t *nodes[STACK_MAX]; int top; } parse_stack_t;

static void stack_push(parse_stack_t *s, dom_node_t *n) {
    if (s->top < STACK_MAX) s->nodes[s->top++] = n;
}
static dom_node_t *stack_peek(parse_stack_t *s) {
    return s->top > 0 ? s->nodes[s->top - 1] : NULL;
}
static dom_node_t *stack_pop(parse_stack_t *s) {
    return s->top > 0 ? s->nodes[--s->top] : NULL;
}
/* Find the table node on the stack (for foster-parenting) */
static dom_node_t *stack_find_table(parse_stack_t *s) {
    for (int i = s->top - 1; i >= 0; i--)
        if (strcmp(s->nodes[i]->tag, "table") == 0)
            return s->nodes[i];
    return NULL;
}
/* Find the node just before the table on the stack */
static dom_node_t *stack_before_table(parse_stack_t *s) {
    for (int i = s->top - 1; i >= 0; i--)
        if (strcmp(s->nodes[i]->tag, "table") == 0)
            return i > 0 ? s->nodes[i - 1] : NULL;
    return NULL;
}

/* ---------------------------------------------------------------
 * LENIENT parser — puts <script> inside <table> as-is.
 * This is what a naive/buggy sanitiser might do.
 * --------------------------------------------------------------- */
dom_node_t *dom_parse_lenient(const char *html)
{
    dom_reset_pool();
    dom_node_t *root = dom_new_node();
    strncpy(root->tag, "#document", 31);

    parse_stack_t stk = {0};
    stack_push(&stk, root);

    tokenizer_t tok;
    tokenizer_init(&tok, html);
    token_t t;

    dom_node_t *current_element = NULL;

    while (tokenizer_next(&tok, &t) == 0) {
        dom_node_t *parent = stack_peek(&stk);
        if (!parent) break;

        switch (t.type) {
        case TOK_START_TAG: {
            dom_node_t *elem = dom_new_node();
            strncpy(elem->tag, t.tag, 31);
            dom_append_child(parent, elem);
            /* void elements do not get pushed onto stack */
            if (strcmp(t.tag, "br") != 0 && strcmp(t.tag, "img") != 0 &&
                strcmp(t.tag, "input") != 0 && strcmp(t.tag, "meta") != 0)
                stack_push(&stk, elem);
            current_element = elem;
            break;
        }
        case TOK_ATTR:
            if (current_element)
                dom_add_attr(current_element, t.attr_name, t.attr_value);
            break;
        case TOK_END_TAG: {
            /* Pop until we find the matching tag */
            for (int i = stk.top - 1; i >= 1; i--) {
                if (strcmp(stk.nodes[i]->tag, t.tag) == 0) {
                    stk.top = i;
                    break;
                }
            }
            current_element = stack_peek(&stk);
            break;
        }
        case TOK_TEXT: {
            dom_node_t *tn = dom_new_node();
            tn->is_text = 1;
            strncpy(tn->text, t.text, 255);
            dom_append_child(parent, tn);
            break;
        }
        case TOK_EOF:
            goto done_lenient;
        }
    }
done_lenient:
    return root;
}

/* ---------------------------------------------------------------
 * STRICT parser — implements HTML5 foster parenting.
 * When a <script> (or other non-table content) appears inside a
 * <table>, the parser "foster parents" it to before the table.
 * This mirrors the behaviour of browsers per the HTML5 spec.
 * --------------------------------------------------------------- */

/* Tags that are legal children of <table> */
static int is_table_content(const char *tag) {
    return (strcmp(tag, "thead") == 0 || strcmp(tag, "tbody") == 0 ||
            strcmp(tag, "tfoot") == 0 || strcmp(tag, "tr")    == 0 ||
            strcmp(tag, "td")    == 0 || strcmp(tag, "th")    == 0 ||
            strcmp(tag, "caption") == 0 || strcmp(tag, "colgroup") == 0 ||
            strcmp(tag, "col") == 0);
}

dom_node_t *dom_parse_strict(const char *html)
{
    /* Use a separate pool region — shift pool_idx to avoid clobbering lenient's nodes */
    int save_pool = pool_idx;
    (void)save_pool; /* not resetting — caller prints both trees before reset */

    dom_node_t *root = dom_new_node();
    strncpy(root->tag, "#document", 31);

    parse_stack_t stk = {0};
    stack_push(&stk, root);

    tokenizer_t tok;
    tokenizer_init(&tok, html);
    token_t t;

    dom_node_t *current_element = NULL;

    while (tokenizer_next(&tok, &t) == 0) {
        dom_node_t *parent = stack_peek(&stk);
        if (!parent) break;

        switch (t.type) {
        case TOK_START_TAG: {
            dom_node_t *elem = dom_new_node();
            strncpy(elem->tag, t.tag, 31);

            /* Foster parenting: if we're inside a <table> and the new tag
             * is NOT legal table content, insert it BEFORE the table. */
            dom_node_t *table = stack_find_table(&stk);
            if (table && !is_table_content(t.tag)) {
                dom_node_t *before = stack_before_table(&stk);
                dom_node_t *foster_parent = before ? before : root;
                printf("  [STRICT PARSER] Foster parenting <%s> to before <table>\n", t.tag);
                dom_append_child(foster_parent, elem);
            } else {
                dom_append_child(parent, elem);
            }
            if (strcmp(t.tag, "br") != 0 && strcmp(t.tag, "img") != 0 &&
                strcmp(t.tag, "input") != 0 && strcmp(t.tag, "meta") != 0)
                stack_push(&stk, elem);
            current_element = elem;
            break;
        }
        case TOK_ATTR:
            if (current_element)
                dom_add_attr(current_element, t.attr_name, t.attr_value);
            break;
        case TOK_END_TAG: {
            for (int i = stk.top - 1; i >= 1; i--) {
                if (strcmp(stk.nodes[i]->tag, t.tag) == 0) {
                    stk.top = i;
                    break;
                }
            }
            current_element = stack_peek(&stk);
            break;
        }
        case TOK_TEXT: {
            dom_node_t *table = stack_find_table(&stk);
            dom_node_t *tn = dom_new_node();
            tn->is_text = 1;
            strncpy(tn->text, t.text, 255);
            if (table) {
                dom_node_t *before = stack_before_table(&stk);
                dom_node_t *foster_parent = before ? before : root;
                dom_append_child(foster_parent, tn);
            } else {
                dom_append_child(parent, tn);
            }
            break;
        }
        case TOK_EOF:
            goto done_strict;
        }
    }
done_strict:
    return root;
}
