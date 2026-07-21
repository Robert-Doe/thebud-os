#include "heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Simple bump-pointer heap ───────────────────────────────────────────────
 *
 * We maintain a flat array of slots.  Each slot is either free or holds a
 * pointer to a gc_object allocated with malloc.  The GC sweep pass calls
 * heap_free_object() on unmarked slots.
 *
 * This design is deliberately simple: a real GC uses a contiguous arena with
 * bump-pointer allocation for cache locality.  Ours uses malloc per object
 * so we can free() individual objects without an arena.
 */

#define MAX_OBJECTS 1024

static struct gc_object *heap_slots[MAX_OBJECTS];
static int               heap_count = 0;
static int               heap_allocs = 0;
static int               heap_frees  = 0;

void heap_init(void) {
    memset(heap_slots, 0, sizeof(heap_slots));
    heap_count  = 0;
    heap_allocs = 0;
    heap_frees  = 0;
}

void heap_destroy(void) {
    for (int i = 0; i < heap_count; i++) {
        if (heap_slots[i]) {
            free(heap_slots[i]);
            heap_slots[i] = NULL;
        }
    }
    heap_count = 0;
}

struct gc_object *gc_alloc(size_t size) {
    if (heap_count >= MAX_OBJECTS) {
        fprintf(stderr, "gc_alloc: heap full\n");
        return NULL;
    }
    /* Allocate header + payload */
    struct gc_object *obj = malloc(sizeof(struct gc_object) + size);
    if (!obj) return NULL;
    memset(obj, 0, sizeof(struct gc_object) + size);
    obj->size      = size;
    obj->marked    = 0;
    obj->ref_count = 0;
    heap_slots[heap_count++] = obj;
    heap_allocs++;
    return obj;
}

void gc_add_ref(struct gc_object *from, int slot, struct gc_object *to) {
    if (!from || slot < 0 || slot >= GC_MAX_REFS) return;
    from->refs[slot] = to;
    if (slot >= from->ref_count)
        from->ref_count = slot + 1;
}

void heap_foreach(heap_visitor_t fn, void *userdata) {
    for (int i = 0; i < heap_count; i++) {
        if (heap_slots[i])
            fn(heap_slots[i], userdata);
    }
}

void heap_free_object(struct gc_object *obj) {
    /* Remove from slots array, free memory */
    for (int i = 0; i < heap_count; i++) {
        if (heap_slots[i] == obj) {
            /* Zero out the data so dangling reads show garbage (0x00) */
            memset(obj->data, 0, obj->size);
            free(obj);
            heap_slots[i] = NULL;
            heap_frees++;
            return;
        }
    }
}

void heap_dump_stats(void) {
    int live = 0;
    for (int i = 0; i < heap_count; i++)
        if (heap_slots[i]) live++;
    printf("  Heap: %d allocs, %d frees, %d live objects\n",
           heap_allocs, heap_frees, live);
}
