#include "js_object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct js_object *js_object_new(obj_type_t type, uint32_t length) {
    struct js_object *obj = calloc(1, sizeof(struct js_object));
    if (!obj) return NULL;
    obj->type    = type;
    obj->length  = length;
    obj->elements = obj->element_buf;  /* use inline storage */
    return obj;
}

void js_object_free(struct js_object *obj) {
    free(obj);
}

const char *obj_type_name(obj_type_t t) {
    switch (t) {
        case OBJ_ARRAY:       return "Array";
        case OBJ_MAP:         return "Map";
        case OBJ_FUNCTION:    return "Function";
        case OBJ_FIXED_ARRAY: return "FixedArray";
        default:              return "Unknown";
    }
}

void js_object_print(const struct js_object *obj, const char *label) {
    printf("  %s: type=%s  length=%u  addr=0x%llX\n",
           label,
           obj_type_name(obj->type),
           obj->length,
           (unsigned long long)(uintptr_t)obj);
    /* Print first 4 elements */
    uint32_t show = obj->length < 4 ? obj->length : 4;
    for (uint32_t i = 0; i < show; i++)
        printf("    [%u] = 0x%016llX\n", i, (unsigned long long)obj->elements[i]);
    if (obj->length > 4)
        printf("    ... (%u more elements)\n", obj->length - 4);
}
