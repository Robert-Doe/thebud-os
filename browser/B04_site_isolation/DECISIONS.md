# B04 — Site Isolation: Design Decisions

## 1. Site vs. Origin (Site = Registered Domain, Broader Than Origin)

**Decision:** The site key is the scheme + eTLD+1 (the registered domain), not the full origin. `https://a.example.com` and `https://b.example.com` are different origins but the same site (`https://example.com`).

**Why:** Process-per-origin would require a new renderer process for every subdomain of every site, multiplying process count enormously. Most subdomains of the same registered domain are operated by the same organisation and trust each other — they need to be able to call `document.domain` (legacy) or share `localStorage` key namespaces. Grouping at the registered-domain level balances isolation with practicality.

**Trade-off:** A compromised `evil.example.com` is in the same renderer process as `good.example.com` even though they are different origins. This is weaker than origin-level isolation. Chrome 92 added `COOP`/`COEP` headers to opt into `crossOriginIsolated` mode, which can trigger per-origin process allocation for high-value origins.

---

## 2. Same-Origin Pages Can Share a Renderer; Different Sites Cannot

**Decision:** `site_registry_get_or_create()` returns the same entry (and therefore the same renderer process) for two URLs with the same site key.

**Why:** Pages of the same origin can access each other's DOM via `window.opener` or `window.frames`. They are already capable of sharing information through JavaScript, so there is no security benefit to isolating them in separate processes — only cost. The renderer process is the unit of trust; same-site pages get the same unit of trust.

**Trade-off:** A large site with many open tabs all runs in one renderer process. If the renderer crashes (e.g., due to a memory-exhausting JavaScript loop), all tabs for that site crash together. This is the "tab isolation" problem; it can be mitigated by spawning one renderer per tab within the site, but that gives up the shared-memory benefits that make same-tab `window.frames` communication fast.

---

## 3. Process-per-Site vs. Process-per-Tab Tradeoffs

**Decision:** This demo implements process-per-site. Each unique site key gets exactly one renderer, regardless of how many tabs are open to that site.

**Why:** Process-per-tab is what Chromium used before 2018 (the "default mode"). It isolates crashes between tabs but puts all cross-origin iframes of a tab in the same renderer process, which is the Spectre vulnerability vector. Process-per-site ensures cross-site iframes are always in separate processes, eliminating the cross-site Spectre surface, while using fewer processes than process-per-origin.

**Trade-off:** Process-per-site means two tabs to `https://example.com` share one renderer — an exploited tab can access the DOM of the other. Chrome offers a "Strict Site Isolation" mode that spawns one renderer per site per tab to solve this, at higher memory cost. On low-memory devices Chrome falls back to process-per-tab or even a single renderer for all sites.

---

## 4. How Spectre Motivated Moving from Process-per-Tab to Process-per-Site

**Decision:** The DECISIONS.md notes that Spectre (2018) was the proximate cause for shipping site isolation by default in Chrome 67.

**Why:** Spectre is a CPU speculative-execution vulnerability that lets code in a process read arbitrary memory from the same process via a timing side-channel. Before site isolation, a cross-origin iframe ran in the same renderer process as the main frame. A Spectre gadget in the iframe's JavaScript could read the main frame's heap — including its cookies, localStorage, and private page content — without any software-visible access violation. Moving cross-site iframes to separate processes means their heaps are in separate virtual address spaces; Spectre can only read within the attacker's own process, which contains only attacker-controlled data.

**Trade-off:** Site isolation requires `SharedArrayBuffer` to be gated behind `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy: require-corp` (the `crossOriginIsolated` flag), because `SharedArrayBuffer` plus a timer creates a Spectre gadget. This broke many web apps that relied on shared memory without the COOP/COEP opt-in.

---

## 5. Performance Cost of More Processes

**Decision:** Each `renderer_pool_spawn()` call does a real `fork()`, making the process creation cost observable in the demo output.

**Why:** `fork()` on Linux is cheap (copy-on-write) but not free. Each process needs its own page table, kernel scheduling entry, and file-descriptor table. With 10+ open tabs each navigating to different sites, Chrome can run 15–25 renderer processes simultaneously. On mobile devices with 2 GB RAM, this is significant — each empty renderer uses ~50 MB. Chrome's "memory saver" mode aggressively discards cold renderer processes and reloads them on demand.

**Trade-off:** The demo calls `sleep(0)` between spawning and reading to let child processes print their startup messages before the parent continues. In a real browser the renderer sends a ready signal over IPC; the browser does not assume timing. The pattern here is illustrative only.
