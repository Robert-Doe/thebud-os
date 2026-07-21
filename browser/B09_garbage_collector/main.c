#include <stdio.h>
#include "heap.h"
#include "gc.h"
#include "uaf_demo.h"

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║      B9 — Garbage Collector & Use-After-Free         ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Part 1: Mark-and-sweep GC basics ── */
    printf("=== Part 1: Mark-and-Sweep GC ===\n\n");
    heap_init();

    struct gc_object *root1 = gc_alloc(16);
    struct gc_object *root2 = gc_alloc(16);
    struct gc_object *orphan = gc_alloc(16);

    gc_add_ref(root1, 0, root2);
    /* orphan has no reference from any root */

    printf("Allocated: root1, root2, orphan\n");
    printf("root1 -> root2 (GC ref), orphan has no root\n\n");

    struct gc_object *roots[] = {root1};
    printf("Running GC (root: root1 only):\n");
    gc_collect(roots, 1);
    printf("  Expected: orphan freed, root1 and root2 survive\n\n");

    heap_dump_stats();
    heap_destroy();

    /* ── Part 2: UAF demo ── */
    uaf_demo_run();

    return 0;
}
