#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

/* ── GC-managed object ──────────────────────────────────────────────────────
 *
 * Every object the GC knows about has this header.
 * The flexible array member `data` is the user payload.
 *
 * refs[] holds GC-visible inter-object pointers.  If you store a pointer to
 * another gc_object only in refs[], the GC can trace it.  If you stash it in
 * data[] or a raw C pointer, the GC CANNOT see it — that's the UAF root cause.
 */

#define GC_MAX_REFS 4

struct gc_object {
    size_t          size;              /* payload size in bytes */
    int             marked;            /* GC mark bit */
    struct gc_object *refs[GC_MAX_REFS]; /* traced references */
    int             ref_count;         /* how many refs[] slots are used */
    char            data[];            /* flexible array: user payload */
};

/* Initialise / tear down the managed heap */
void heap_init(void);
void heap_destroy(void);

/* Allocate a new object with `size` bytes of payload.
   Returns NULL if heap is full. */
struct gc_object *gc_alloc(size_t size);

/* Add a GC-visible reference from `from` slot `slot` to `to` */
void gc_add_ref(struct gc_object *from, int slot, struct gc_object *to);

/* Iterate all live objects (for the GC sweep phase) */
typedef void (*heap_visitor_t)(struct gc_object *obj, void *userdata);
void heap_foreach(heap_visitor_t fn, void *userdata);

/* Return object to the free pool (called by GC sweep) */
void heap_free_object(struct gc_object *obj);

/* Print heap stats */
void heap_dump_stats(void);

#endif /* HEAP_H */
