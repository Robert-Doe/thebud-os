#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void env_init(env_t *e) {
    e->count = 0;
}

void env_set(env_t *e, const char *name, js_value_t val) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->names[i], name) == 0) {
            e->values[i] = val;
            return;
        }
    }
    if (e->count < ENV_SIZE) {
        strncpy(e->names[e->count], name, 63);
        e->values[e->count] = val;
        e->count++;
    }
}

js_value_t env_get(env_t *e, const char *name) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->names[i], name) == 0)
            return e->values[i];
    }
    return val_undefined();
}

js_value_t eval_node(ast_node_t *node, env_t *env) {
    if (!node) return val_undefined();

    switch (node->type) {
        case NODE_NUMBER:
            return val_number(node->num_val);

        case NODE_STRING:
            return val_string(node->str_val);

        case NODE_IDENT:
            return env_get(env, node->str_val);

        case NODE_BINOP: {
            js_value_t left  = eval_node(node->children[0], env);
            js_value_t right = eval_node(node->children[1], env);
            /* Type-safe coercion to numbers for arithmetic */
            double l = (left.type  == VAL_NUMBER) ? left.u.number  : 0;
            double r = (right.type == VAL_NUMBER) ? right.u.number : 0;
            switch (node->op) {
                case '+': return val_number(l + r);
                case '-': return val_number(l - r);
                case '*': return val_number(l * r);
                case '/': return val_number(r != 0 ? l / r : 0);
            }
            return val_undefined();
        }

        case NODE_ASSIGN: {
            js_value_t rhs = eval_node(node->children[0], env);
            env_set(env, node->str_val, rhs);
            return rhs;
        }

        case NODE_VAR_DECL: {
            js_value_t val = val_undefined();
            if (node->num_children > 0)
                val = eval_node(node->children[0], env);
            env_set(env, node->str_val, val);
            return val;
        }

        case NODE_IF: {
            js_value_t cond = eval_node(node->children[0], env);
            int truthy = 0;
            if (cond.type == VAL_NUMBER)    truthy = (cond.u.number != 0);
            else if (cond.type == VAL_STRING) truthy = (cond.u.string && cond.u.string[0] != '\0');
            else if (cond.type == VAL_OBJECT) truthy = (cond.u.object != NULL);

            if (truthy) {
                return eval_node(node->children[1], env);
            } else if (node->num_children > 2) {
                return eval_node(node->children[2], env);
            }
            return val_undefined();
        }

        case NODE_BLOCK: {
            js_value_t result = val_undefined();
            for (int i = 0; i < node->num_children; i++)
                result = eval_node(node->children[i], env);
            return result;
        }
    }
    return val_undefined();
}

void eval_program(program_t *prog, env_t *env) {
    for (int i = 0; i < prog->count; i++) {
        js_value_t result = eval_node(prog->stmts[i], env);
        (void)result;
    }
}
