#include <stdio.h>
#include "value.h"
#include "lexer.h"
#include "parser.h"
#include "eval.h"
#include "confusion_demo.h"

static void run_program(const char *src, env_t *env) {
    parser_t p;
    parser_init(&p, src);
    program_t prog = parser_parse(&p);
    eval_program(&prog, env);
    program_free(&prog);
}

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║       B7 — JavaScript Interpreter & Type Confusion   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Interpreter demo ── */
    printf("=== Part 1: Tree-Walking Interpreter ===\n\n");

    env_t env;
    env_init(&env);

    /* Arithmetic */
    printf("Program: var x = 10; var y = 3; var z = x * y + 2;\n");
    run_program("var x = 10; var y = 3; var z = x * y + 2;", &env);
    printf("  x = "); val_print(env_get(&env, "x")); printf("\n");
    printf("  y = "); val_print(env_get(&env, "y")); printf("\n");
    printf("  z = "); val_print(env_get(&env, "z")); printf("\n\n");

    /* String variable */
    printf("Program: var greeting = \"hello\";\n");
    run_program("var greeting = \"hello\";", &env);
    printf("  greeting = "); val_print(env_get(&env, "greeting")); printf("\n\n");

    /* If/else */
    printf("Program: if (x) { var result = 99; } else { var result = 0; }\n");
    run_program("if (x) { var result = 99; } else { var result = 0; }", &env);
    printf("  result = "); val_print(env_get(&env, "result")); printf("\n\n");

    /* Assignment */
    printf("Program: x = x + 5;\n");
    run_program("x = x + 5;", &env);
    printf("  x = "); val_print(env_get(&env, "x")); printf("\n\n");

    /* ── Part 2: Type confusion vulnerability ── */
    confusion_demo_run();

    return 0;
}
