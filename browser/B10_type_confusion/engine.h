#ifndef ENGINE_H
#define ENGINE_H

#include "js_object.h"

/* ── Simplified engine operations ────────────────────────────────────────────
 *
 * The "engine" provides get/set operations on JS objects.  Safe versions
 * check the type tag; vulnerable versions (matching what a bugged JIT emits)
 * skip the check.
 */

/* Safe: checks obj->type before accessing elements */
uint64_t engine_get_element_safe(struct js_object *obj, uint32_t index);
void     engine_set_element_safe(struct js_object *obj, uint32_t index, uint64_t val);

/* Vulnerable (bugged JIT path): skips the type check.
   This is what the CVE-2021-21220 JIT bug actually emitted. */
uint64_t engine_get_element_confused(struct js_object *obj, uint32_t index);
void     engine_set_element_confused(struct js_object *obj, uint32_t index, uint64_t val);

/* Get the simulated "address" of an object (addrof primitive) */
uint64_t engine_addrof(struct js_object *obj);

/* Create a fake object at a given "address" (fakeobj primitive) */
struct js_object *engine_fakeobj(uint64_t addr);

#endif /* ENGINE_H */
