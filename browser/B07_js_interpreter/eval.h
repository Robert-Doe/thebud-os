#ifndef EVAL_H
#define EVAL_H

#include "value.h"
#include "parser.h"

#define ENV_SIZE 64

/* Simple variable environment: name -> js_value_t */
typedef struct {
    char       names[ENV_SIZE][64];
    js_value_t values[ENV_SIZE];
    int        count;
} env_t;

void       env_init(env_t *e);
void       env_set(env_t *e, const char *name, js_value_t val);
js_value_t env_get(env_t *e, const char *name);

/* Evaluate AST node, return result */
js_value_t eval_node(ast_node_t *node, env_t *env);

/* Evaluate full program */
void eval_program(program_t *prog, env_t *env);

#endif /* EVAL_H */
