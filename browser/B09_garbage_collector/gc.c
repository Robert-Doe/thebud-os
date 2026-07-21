#include "gc.h"
#include "heap.h"
#include <stdio.h>
#include <string.h>

void gc_mark(struct gc_object *obj) {
    if (!obj || obj->marked) return;  /* already marked or NULL */
    obj->marked = 1;
    /* Recursively mark all traced references */
    for (int i = 0; i < obj->ref_count; i++)
        gc_mark(obj->refs[i]);
}

static void sweep_visitor(struct gc_object *obj, void *userdata) {
    int *freed = (int *)userdata;
    if (!obj->marked) {
        /* Unreachable — free it */
        (*freed)++;
        heap_free_object(obj);
    } else {
        /* Reachable — clear mark for next cycle */
        obj->marked = 0;
    }
}

void gc_sweep(void) {
    int freed = 0;
    heap_foreach(sweep_visitor, &freed);
    if (freed > 0)
        printf("  GC sweep: freed %d object(s)\n", freed);
    else
        printf("  GC sweep: nothing to free\n");
}

void gc_collect(struct gc_object **roots, int root_count) {
    printf("  GC collect: marking from %d root(s)...\n", root_count);
    /* Mark phase */
    for (int i = 0; i < root_count; i++)
        gc_mark(roots[i]);
    /* Sweep phase */
    gc_sweep();
}
