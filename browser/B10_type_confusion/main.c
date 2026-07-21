#include <stdio.h>
#include "js_object.h"
#include "engine.h"
#include "confusion_exploit.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║       B10 — Type Confusion CVE Reproduction          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("=== Part 1: Object Model ===\n\n");

    struct js_object *arr  = js_object_new(OBJ_ARRAY,       4);
    struct js_object *map  = js_object_new(OBJ_MAP,         4);
    struct js_object *fa   = js_object_new(OBJ_FIXED_ARRAY, 4);
    struct js_object *fn   = js_object_new(OBJ_FUNCTION,    1);

    for (int i = 0; i < 4; i++) arr->elements[i] = (uint64_t)(i * 10);
    for (int i = 0; i < 4; i++) map->elements[i] = (uint64_t)(0xFF00 + i);
    for (int i = 0; i < 4; i++) fa->elements[i]  = (uint64_t)(0xAB00 + i);
    fn->elements[0] = 0xDEADC0DEULL;

    js_object_print(arr, "Array      ");
    js_object_print(map, "Map        ");
    js_object_print(fa,  "FixedArray ");
    js_object_print(fn,  "Function   ");
    printf("\n");

    printf("=== Part 2: Safe Engine Operations ===\n\n");
    printf("  engine_get_element_safe(arr, 2) = %llu\n",
           (unsigned long long)engine_get_element_safe(arr, 2));
    printf("  engine_get_element_safe(map, 0) = %llu (type error)\n",
           (unsigned long long)engine_get_element_safe(map, 0));
    printf("  engine_get_element_safe(arr, 99) = %llu (bounds error)\n\n",
           (unsigned long long)engine_get_element_safe(arr, 99));

    js_object_free(arr);
    js_object_free(map);
    js_object_free(fa);
    js_object_free(fn);

    /* ── Full exploit walkthrough ── */
    confusion_exploit_run();

    return 0;
}
