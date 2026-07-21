# Lessons

Deep-dive companion material for bob_os — each module's `tutorial.html` explains
what the code *does*; these explain the concepts *behind* it (history, hardware
mechanism, security relevance, real-world usage) in more depth than one module's
code alone motivates.

Most clusters here span more than one module, which is why they live centrally
instead of inside a single module folder.

| Cluster | Relates to modules |
|---|---|
| `01_kernel_userspace/` | `02_kernel_entry`, `11_syscalls`, `14_usermode` |
| `02_browser_xray/` | `browser/B01_process_model`, `B02_renderer_sandbox`, `B04_site_isolation`, `B07_js_interpreter`, `B08_jit_compiler` |
| `03_pcb_threads_scheduling/` | `10_processes`, `24_threads`, `25_scheduler` |
| `04_memory_addressing/` | `06_pmm`, `07_paging`, `15_address_spaces`, `21_cow`, `22_demand_paging` |

The one exception: Segmentation Hardware + GDT lessons live in
[`../04_gdt/lessons/`](../04_gdt/lessons/) instead of here, since that cluster
maps cleanly onto exactly one existing module.

Each cluster folder starts with `01_explainer.html` (the core concept, matching
the style/scope of a module's `tutorial.html`), followed by numbered deep dives
that go further into one specific facet.
