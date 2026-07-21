#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t engine_get_element_safe(struct js_object *obj, uint32_t index) {
    /* Type check: only Arrays and FixedArrays have numeric elements */
    if (obj->type != OBJ_ARRAY && obj->type != OBJ_FIXED_ARRAY) {
        fprintf(stderr, "TypeError: %s is not indexable\n", obj_type_name(obj->type));
        return 0;
    }
    /* Bounds check */
    if (index >= obj->length) {
        fprintf(stderr, "RangeError: index %u >= length %u\n", index, obj->length);
        return 0;
    }
    return obj->elements[index];
}

void engine_set_element_safe(struct js_object *obj, uint32_t index, uint64_t val) {
    if (obj->type != OBJ_ARRAY && obj->type != OBJ_FIXED_ARRAY) {
        fprintf(stderr, "TypeError: %s is not indexable\n", obj_type_name(obj->type));
        return;
    }
    if (index >= obj->length) {
        fprintf(stderr, "RangeError: index %u >= length %u\n", index, obj->length);
        return;
    }
    obj->elements[index] = val;
}

/* ── Vulnerable path: no type check ──────────────────────────────────────────
 *
 * A JIT optimisation might cache the type check result and assume it holds
 * across loop iterations.  If the type changes mid-loop (via a callback, a
 * Proxy, or the CVE-2021-21220 Map corruption), the JIT-emitted code runs
 * this path — no type check, treating any object as if it were a FixedArray.
 */
uint64_t engine_get_element_confused(struct js_object *obj, uint32_t index) {
    /* BUG: no type check — treats obj->elements as valid regardless of type */
    /* BUG: no bounds check — uses the raw length field of the (wrong) type */
    return obj->elements[index];
}

void engine_set_element_confused(struct js_object *obj, uint32_t index, uint64_t val) {
    obj->elements[index] = val;
}

uint64_t engine_addrof(struct js_object *obj) {
    /* In V8, addrof() is obtained by:
     *   1. Put the target object into a typed array's internal array object.
     *   2. Use type confusion to read that internal array's element as a number.
     *   3. The number IS the object's pointer value.
     * We simulate this directly. */
    return (uint64_t)(uintptr_t)obj;
}

struct js_object *engine_fakeobj(uint64_t addr) {
    /* In V8, fakeobj() is the inverse:
     *   1. Write an attacker-controlled number into a typed array element.
     *   2. Use type confusion to read that element back as an object reference.
     *   3. The engine now treats the attacker's number as a heap pointer.
     * We simulate this by casting the integer back to a pointer. */
    return (struct js_object *)(uintptr_t)addr;
}
