# B03 — Same-Origin Policy Engine: Design Decisions

## 1. Origin = Scheme + Host + Port (Not URL Path)

**Decision:** The `origin_t` struct contains only `scheme`, `host`, and `port`. The URL path, query string, and fragment are discarded during parsing.

**Why:** The path is part of the resource identifier, not the security boundary. Two pages at `https://example.com/login` and `https://example.com/profile` are from the same origin and are expected to share cookies, localStorage, and DOM access. The security boundary is "which server responded to this request" — identified by scheme + host + port. Anything those three agree on is the same server from the browser's perspective.

**Trade-off:** This means a single compromised page at `https://example.com/comments` can read the DOM of `https://example.com/banking`. The SOP is a coarse-grained boundary at the origin level, not the page level. Finer-grained isolation within an origin requires application-level mechanisms like `Content-Security-Policy` or `Cross-Origin-Opener-Policy`.

---

## 2. Default Port Is Inferred from Scheme

**Decision:** When a URL has no explicit port, `parse_origin()` records `PORT_DEFAULT` and `effective_port()` infers it from the scheme (https → 443, http → 80).

**Why:** `https://example.com/` and `https://example.com:443/` are the same origin — they both refer to the same TCP endpoint. If we stored literal port numbers only, explicit and implicit representations of the same origin would compare as unequal, breaking SOP in subtle ways. The effective-port calculation ensures the two representations are normalised before comparison.

**Trade-off:** The mapping is hard-coded for common schemes. Less common schemes (ws, wss, ftp) need their own entries. The WHATWG URL Standard maintains the canonical table; we implement a simplified subset. Getting this table wrong would cause false "different origins" verdicts for WebSocket connections or FTP resources.

---

## 3. Opaque Origins (null, data:) — Cannot Communicate With Anything

**Decision:** `file://` and `data:` URLs produce an opaque (null) origin, represented by `port == PORT_OPAQUE`. Two opaque origins are never equal to each other.

**Why:** These schemes do not come from any web server, so the "who issued this?" question has no meaningful answer. A `data:` URL is content inline in the URL string — it could be constructed by anyone. Allowing `data:` pages to read each other or communicate with `https:` pages would let an attacker smuggle cross-origin communication through `data:` URLs. The "two opaque origins are never equal" rule means even two `data:` blobs from the same page cannot communicate.

**Trade-off:** This makes `file://` pages completely isolated, which frustrates local development. Developers working offline cannot have a `file://` page load a `file://` sub-resource with XHR. That is an intentional friction: the alternative opens a vector where a malicious local HTML file can read other local files.

---

## 4. Subdomains Are Separate Origins by Default

**Decision:** `https://a.example.com` and `https://b.example.com` have different `host` fields and are blocked from accessing each other.

**Why:** A subdomain is typically a separate service operated by a different team or even a different organisation (think `untrusted-user-content.github.io` vs `github.com`). If subdomains shared an origin, any XSS on a user-content subdomain would give full DOM access to the main product. The subdomain boundary is the correct default isolation level.

**Trade-off:** Legitimate use cases exist where a parent site wants its subdomains to cooperate (e.g., `login.corp.com` and `app.corp.com`). The historical escape hatch was `document.domain = 'corp.com'`, which relaxed the host comparison. This is now deprecated (see Decision 5).

---

## 5. document.domain Escape Hatch and Why It Is Deprecated

**Decision:** The demo does not implement `document.domain` mutation, and the output explicitly notes that it is deprecated.

**Why:** `document.domain` let two pages on different subdomains agree to merge their origins for DOM-sharing purposes. The mechanism is dangerous because it is opt-out: a page sets `document.domain` and immediately becomes accessible to every other page in the same registrable domain that also sets it. This breaks the process-isolation model: if `a.corp.com` and `b.corp.com` share an origin, they must run in the same renderer process, undoing the site-isolation guarantees of B04. Chrome 106 disabled `document.domain` by default; Firefox is following.

**Trade-off:** Removing `document.domain` breaks legacy intranet applications that relied on it. The modern replacement is `postMessage()` for explicit message passing, or `CORS` for cross-origin resource requests. Both are opt-in and auditable — the receiving page decides what to accept.
