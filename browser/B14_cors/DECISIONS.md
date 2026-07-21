# B14 — CORS & Fetch: Design Decisions

## 1. Why CORS Is Enforced by the Browser, Not the Server

**Decision:** Implement enforcement in `fetch_sim.c` (the browser-side simulator), not in the server response logic.

**Why:** CORS is a browser security policy. The server communicates its intent via response headers (`Access-Control-Allow-Origin`), but the decision to expose the response to the requesting script is made entirely by the browser. A server cannot "enforce CORS" — it can only declare its policy. A non-browser HTTP client (curl, Postman, server-to-server requests) ignores CORS headers entirely. This means CORS protects against malicious web pages reading cross-origin responses; it does not protect the server from unauthorized access by non-browser clients. The server must still authenticate requests independently.

**Trade-off:** This model means CORS headers on the server are advisory only. Developers sometimes misunderstand this and omit authentication on APIs, believing CORS will protect them from unauthorized clients.

---

## 2. Simple vs Preflighted Requests — What Triggers a Preflight

**Decision:** Implement `cors_is_simple()` to classify requests and show the two-phase preflight path.

**Why:** The CORS spec defines "simple requests" (GET/HEAD/POST with only safe headers) as not requiring a preflight OPTIONS request. This preserves backward compatibility with pre-CORS servers that respond to GET/POST without CORS headers. Any request that could mutate server state and might not be handled gracefully by a non-CORS server — such as PUT, DELETE, or any request with custom headers — triggers a preflight. The preflight asks the server for permission before the real request is sent, ensuring old servers are never surprised by a cross-origin PUT.

**Trade-off:** Preflights add a round-trip latency penalty. High-frequency APIs sometimes design around preflights by restricting themselves to simple request shapes, or by caching preflight results via `Access-Control-Max-Age`.

---

## 3. Why `Access-Control-Allow-Origin: *` with Credentials Is Blocked

**Decision:** Explicitly reject `ACAO: *` + `credentials: true` in `cors_check()`.

**Why:** Cookies and HTTP authentication identify the user. If a server responds `ACAO: *` and a browser allowed credentialed cross-origin requests, any malicious page could call `fetch(victimSite, {credentials: 'include'})` and receive the authenticated response — effectively impersonating the user against the victim site. The spec prohibits this combination: `ACAO: *` is only valid for unauthenticated (anonymous) resources. To allow credentialed cross-origin requests, the server must echo the exact requesting origin (not `*`) and include `Access-Control-Allow-Credentials: true`.

**Trade-off:** This restriction surprises developers who want a "fully open" API and also support cookies. The correct solution is for the server to maintain an allowlist of trusted origins and dynamically set `ACAO` to the requesting origin when it matches.

---

## 4. `null` Origin — When It Appears and Why It Is Dangerous

**Decision:** Handle `null` as a special origin case and warn against `ACAO: null`.

**Why:** The string `"null"` appears as the `Origin` header when a request is made from a sandboxed `<iframe>` (e.g., `<iframe sandbox>`), a `data:` URL, a `file:` URL, or certain redirects. All of these non-web contexts collapse to the same opaque origin string `"null"`. If a server responds `ACAO: null`, it is effectively permitting ALL of these contexts — including an attacker's sandboxed iframe — to read the response. Because `null` is not a real origin, it cannot be meaningfully audited. Servers should avoid `ACAO: null` in all production configurations.

**Trade-off:** There is no safe way to selectively trust sandboxed iframes via `ACAO: null`. If cross-origin communication with a sandboxed iframe is needed, `postMessage` is the correct mechanism.

---

## 5. CORB — Cross-Origin Read Blocking: Content-Type Sniffing Defense

**Decision:** Implement CORB as a separate layer in `fetch_sim()` that fires after CORS succeeds.

**Why:** CORB protects sensitive data formats (JSON, HTML, XML) from being loaded by no-CORS sub-resource requests (`<img>`, `<script>`, `<video>`). Even if an attacker embeds `<img src="https://bank.com/balance.json">`, CORS would normally block script access to the response — but timing side channels, image decoding error paths, and speculative execution could leak information about the response bytes. CORB prevents this by having the browser strip the response body for protected MIME types loaded by incompatible initiators, before any rendering or speculation occurs.

**Trade-off:** CORB requires accurate MIME typing. A server that responds `Content-Type: text/html` for an image will have that image's body stripped. Legacy servers with misconfigured MIME types can break under CORB. The browser mitigates this with "MIME sniffing" to avoid false positives on responses that claim to be HTML but are clearly images.

---

## 6. CORP — Cross-Origin Resource Policy Header as Stronger Alternative

**Decision:** Reference CORP in the demo as the stronger declarative alternative to CORB.

**Why:** CORB is a heuristic defense applied by the browser automatically. The `Cross-Origin-Resource-Policy` (CORP) HTTP header is an explicit opt-in: a resource can declare `CORP: same-origin` or `CORP: same-site` to prevent ANY cross-origin load — including no-CORS loads by `<img>` and `<video>` — regardless of MIME type. CORP closes gaps that CORB cannot address: resources with non-protected MIME types (e.g., a `.js` file used as tracking data) are not shielded by CORB but are shielded by `CORP: same-origin`. Deploying CORP on all internal APIs and sensitive assets is a defense-in-depth measure against Spectre-class attacks.

**Trade-off:** CORP must be explicitly set per resource. It is not a browser default, so every resource that should be protected requires the header to be added. CDN resources intended for cross-origin use must not set CORP, or they will break.
