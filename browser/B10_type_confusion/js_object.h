#ifndef JS_OBJECT_H
#define JS_OBJECT_H

#include <stdint.h>
#include <stddef.h>

/* ── Object type system mirroring V8's internal structure ─────────────────
 *
 * Every JavaScript object has a hidden 'type' tag (in V8 this is encoded in
 * the Map pointer — the first field of every HeapObject).
 *
 * The type tag determines which fields are valid and how the engine accesses
 * them.  A type confusion bug lets the JIT skip the type check, so an object
 * of one type is treated as if it were another type.
 */

typedef enum {
    OBJ_ARRAY,        /* JS Array: length + elements backing store */
    OBJ_MAP,          /* JS Map: key-value pair array */
    OBJ_FUNCTION,     /* JS Function: code pointer + closure */
    OBJ_FIXED_ARRAY   /* Internal FixedArray: raw element storage */
} obj_type_t;

/* Maximum elements per object */
#define MAX_ELEMENTS 64

struct js_object {
    obj_type_t  type;
    uint32_t    length;         /* for ARRAY / FIXED_ARRAY: number of elements */
    uint64_t   *elements;       /* pointer to backing store */
    uint64_t    element_buf[MAX_ELEMENTS]; /* inline storage */

    /* For MAP: key-value pairs stored interleaved in element_buf */
    /* For FUNCTION: element_buf[0] = code pointer */

    /* The object's "address" — in a real engine this is the heap pointer.
       We simulate it as the struct pointer cast to uint64_t. */
};

/* Allocate a new JS object (simple malloc wrapper) */
struct js_object *js_object_new(obj_type_t type, uint32_t length);
void              js_object_free(struct js_object *obj);

/* Type name string */
const char *obj_type_name(obj_type_t t);

/* Print object summary */
void js_object_print(const struct js_object *obj, const char *label);

#endif /* JS_OBJECT_H */
