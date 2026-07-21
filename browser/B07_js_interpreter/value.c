#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

js_value_t val_number(double n) {
    js_value_t v;
    v.type = VAL_NUMBER;
    v.u.number = n;
    return v;
}

js_value_t val_string(const char *s) {
    js_value_t v;
    v.type = VAL_STRING;
    v.u.string = strdup(s);
    return v;
}

js_value_t val_object(void *obj) {
    js_value_t v;
    v.type = VAL_OBJECT;
    v.u.object = obj;
    return v;
}

js_value_t val_undefined(void) {
    js_value_t v;
    v.type = VAL_UNDEFINED;
    v.u.number = 0;
    return v;
}

double val_to_number(js_value_t v) {
    if (v.type != VAL_NUMBER) {
        fprintf(stderr, "FATAL: expected number, got %s\n", val_type_name(v.type));
        exit(1);
    }
    return v.u.number;
}

const char *val_to_string(js_value_t v) {
    if (v.type != VAL_STRING) {
        fprintf(stderr, "FATAL: expected string, got %s\n", val_type_name(v.type));
        exit(1);
    }
    return v.u.string;
}

void *val_to_object(js_value_t v) {
    if (v.type != VAL_OBJECT) {
        fprintf(stderr, "FATAL: expected object, got %s\n", val_type_name(v.type));
        exit(1);
    }
    return v.u.object;
}

const char *val_type_name(val_type_t t) {
    switch (t) {
        case VAL_NUMBER:    return "number";
        case VAL_STRING:    return "string";
        case VAL_OBJECT:    return "object";
        case VAL_UNDEFINED: return "undefined";
        default:            return "unknown";
    }
}

void val_print(js_value_t v) {
    switch (v.type) {
        case VAL_NUMBER:    printf("%g", v.u.number); break;
        case VAL_STRING:    printf("\"%s\"", v.u.string); break;
        case VAL_OBJECT:    printf("[object %p]", v.u.object); break;
        case VAL_UNDEFINED: printf("undefined"); break;
    }
}
