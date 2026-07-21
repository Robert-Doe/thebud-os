#ifndef GC_H
#define GC_H

#include "heap.h"

/* ── Mark-and-Sweep Garbage Collector ────────────────────────────────────────
 *
 * Mark phase:  traverse the object graph from `roots`, set marked=1.
 * Sweep phase: scan all allocated objects, free any with marked==0.
 *
 * After a collection, any raw C pointer to a freed object is dangling.
 * Using it is a use-after-free.
 */

/* Run a full GC cycle.
 *   roots      - array of root pointers (local vars, globals)
 *   root_count - number of roots
 */
void gc_collect(struct gc_object **roots, int root_count);

/* Mark a single object and all objects reachable from it */
void gc_mark(struct gc_object *obj);

/* Sweep all unmarked objects from the heap */
void gc_sweep(void);

#endif /* GC_H */
