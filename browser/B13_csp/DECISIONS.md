# B13 — Content Security Policy: Design Decisions

## 1. CSP as HTTP Header (Not HTML Attribute)

**Decision:** Model CSP as an HTTP response header (`Content-Security-Policy:`) rather than a `<meta>` tag attribute.

**Why:** The HTTP header is the authoritative delivery mechanism. It reaches the browser before the HTML parser begins, ensuring the policy applies to the very first resources the parser requests. A `<meta>` tag is parsed mid-document, so any resources referenced before that tag (external stylesheets, scripts in `<head>`) load without enforcement. The header also cannot be stripped by injected content, because an attacker who can inject HTML cannot modify the already-delivered response headers.

**Trade-off:** `<meta http-equiv>` is valid for some directives and useful when you cannot control server headers (e.g., static file hosting). But `frame-ancestors`, `sandbox`, and `report-uri` are **not** supported via `<meta>`, so the HTTP header is always the complete solution.

---

## 2. Why `'unsafe-inline'` Collapses Most of the Policy

**Decision:** Flag `has_unsafe_inline` explicitly and warn whenever it is present.

**Why:** The primary goal of CSP's `script-src` is to block injected `<script>` content — the most common XSS vector. `'unsafe-inline'` permits every inline script: `<script>…</script>`, `onclick=`, `javascript:` URLs, and inline event handlers. Because XSS payloads typically inject HTML with inline JS, the presence of `'unsafe-inline'` means an attacker who achieves HTML injection also achieves JS execution, exactly as if no CSP existed for scripts.

**Trade-off:** Many legacy applications rely heavily on inline scripts and cannot migrate to nonces or hashes without significant refactoring. `'unsafe-inline'` is a transitional allowance, but it should be accompanied by other layers (WAF, strict input validation) and a migration plan.

---

## 3. Why Wildcard Domains Allow Subdomain Takeover Bypasses

**Decision:** Treat `*` and `*.example.com` as critically dangerous and highlight them in enforcement output.

**Why:** `*.example.com` permits any subdomain, including ones the site owner does not control. If an attacker can register `evil.example.com` (via expired subdomain, cloud storage bucket, or DNS misconfiguration), CSP will permit scripts loaded from that attacker-controlled origin. This is called a subdomain takeover bypass. Additionally, bare `*` in `img-src` or `connect-src` allows exfiltration of data to any endpoint via image pixels or XHR.

**Trade-off:** Wildcards are convenient when you legitimately serve from many subdomains (e.g., a CDN with per-user subdomains). The correct fix is to enumerate only the exact origins you control, or use a strict `Content-Disposition: attachment` on user-controlled subdomains.

---

## 4. JSONP Endpoints: Domain Is Trusted but Content Is Attacker-Controlled

**Decision:** Demonstrate the JSONP bypass as a class of "trusted origin, untrusted content" failures.

**Why:** CSP allowlisting operates at the granularity of origins, not URL paths or response content. If `cdn.example.com` hosts a JSONP endpoint that reflects a `callback=` query parameter into a `<script>` response body, then `script-src https://cdn.example.com` permits loading that reflected JS. The browser cannot distinguish between `cdn.example.com/jquery.min.js` (safe) and `cdn.example.com/api?callback=alert(1)` (attacker payload). This bypasses CSP without any injection into the victim page.

**Trade-off:** The only complete fix is to eliminate JSONP endpoints and replace them with CORS-enabled JSON APIs. Short-term mitigations (path restrictions via `https://cdn.example.com/static/`) help but are fragile.

---

## 5. Nonces and Hashes as the Correct Alternatives to `'unsafe-inline'`

**Decision:** Document nonces and hashes as the architecturally sound replacement for `'unsafe-inline'`.

**Why:** A **nonce** (`script-src 'nonce-r4nd0mBase64'`) allows only `<script nonce="r4nd0mBase64">` tags to execute. The nonce is generated fresh per response, so an attacker who injects a `<script>` tag cannot know the nonce. A **hash** (`'sha256-base64hash'`) allows only scripts whose exact content matches the precomputed hash. Neither mechanism can be exploited by HTML injection alone, restoring the original XSS protection intent of CSP.

**Trade-off:** Nonces require server-side templating to inject the nonce value into every `<script>` tag, and the nonce must be cryptographically random per response. Hashes are simpler for static scripts but impractical for dynamically generated inline scripts. Both are significantly harder to implement on legacy codebases than simply adding `'unsafe-inline'`.

---

## 6. `report-uri` vs `report-to` and Why CSP Violations Must Be Monitored

**Decision:** Explain both reporting mechanisms and emphasize that CSP without reporting is blind enforcement.

**Why:** `report-uri` (deprecated but widely supported) sends a JSON POST to a URL when a policy is violated. `report-to` (modern, uses the Reporting API) batches reports and supports other security report types. Without violation reporting, a site operator cannot know whether their CSP is blocking legitimate resources (causing breakage) or whether attackers are probing for bypasses. A policy deployed in `Content-Security-Policy-Report-Only` mode enforces nothing but sends all violations to the report endpoint — essential for testing a new policy before enforcement.

**Trade-off:** Report endpoints become a target themselves. An attacker who can flood the report endpoint with fake violations (via crafted pages that load resources from the victim origin inside an iframe) can perform a denial-of-service on the reporting pipeline. Rate limiting and authentication on the report endpoint are necessary.
