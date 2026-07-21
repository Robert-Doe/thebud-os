# B17 — Cookie & Session Security: Design Decisions

## 1. Why Cookies Are Automatically Sent (Not Opt-In) — the Root of CSRF

**Decision:** Implement `cookie_jar_get()` to automatically include matching cookies based only on domain/path/flags, with no per-request opt-in by the page.

**Why:** HTTP cookies were designed in the early 1990s as a state mechanism for stateless HTTP. The original design attaches cookies to all requests matching their domain and path automatically, because in 1994 the threat model did not include cross-origin web pages triggering requests. This automatic attachment is what enables Cross-Site Request Forgery: when a page at `evil.com` triggers a request to `bank.com`, the browser automatically attaches the victim's `bank.com` cookies — the server sees an authenticated request it cannot distinguish from the user's intentional action. The browser never asks the page initiating the request for permission; it simply sends the matching cookies.

**Trade-off:** Changing this fundamental behavior would break backward compatibility with billions of existing web applications. The SameSite attribute was introduced as an opt-in mitigation precisely because the default behavior cannot be changed.

---

## 2. SameSite=Lax as a Reasonable Default (Chrome Changed This in 2020)

**Decision:** Set `same_site = 1` (Lax) as the default when no SameSite attribute is specified in the parser.

**Why:** Prior to Chrome 80 (February 2020), cookies with no SameSite attribute behaved as `SameSite=None` — they were sent on all cross-site requests. Chrome 80 changed the default to `SameSite=Lax`, which blocks cookies on cross-site sub-resource requests (images, forms, XHR) while still allowing them on top-level navigations (clicking a link from another site). This change eliminated the most common CSRF vectors without requiring explicit opt-in from developers. The intent is to force sites that need `SameSite=None` (e.g., cross-site SSO) to explicitly declare it, while protecting all others by default.

**Trade-off:** `SameSite=Lax` broke some OAuth flows where the redirect from the identity provider to the relying party was treated as cross-site. The 2-minute "lax + POST" grace period (where browsers temporarily allowed POST from a top-level navigation even with `Lax`) was added to mitigate this, but was later removed due to abuse.

---

## 3. Why SameSite=Strict Breaks OAuth Flows (Cross-Site Redirects)

**Decision:** Document the SameSite=Strict tradeoff explicitly because it affects critical authentication flows.

**Why:** OAuth 2.0 and OIDC involve a redirect from the identity provider (e.g., `accounts.google.com`) back to the relying party (e.g., `app.example.com`). From the browser's perspective, this redirect is a cross-site navigation: the previous page was `accounts.google.com`, and the new destination is `app.example.com`. With `SameSite=Strict`, the session cookie at `app.example.com` is not sent on this redirect — meaning the user lands on the OAuth callback page without their session, even if they were already logged in. This causes the login flow to break. Developers who set `SameSite=Strict` on all cookies discover that their OAuth integration silently fails.

**Trade-off:** The fix is to use separate session-identification cookies: a `SameSite=Lax` or `SameSite=None` cookie to carry the OAuth state parameter (short-lived, low value), and a `SameSite=Strict` session cookie that is only set after the OAuth callback completes and the user is verified on the origin. This is more complex but correctly scoped.

---

## 4. HttpOnly: Defense in Depth Against XSS Cookie Theft

**Decision:** Implement `cookie_js_readable()` to enforce the HttpOnly flag and demonstrate XSS protection.

**Why:** `document.cookie` in JavaScript returns all cookies accessible to the current page. Without HttpOnly, an XSS payload can extract the session token and send it to an attacker-controlled endpoint. The attacker then uses the stolen token to hijack the session — all without the victim's knowledge. `HttpOnly` tells the browser to exclude the cookie from `document.cookie` and all JavaScript cookie APIs. The cookie is still sent on HTTP requests (the only path the server needs), but JS cannot read its value. This does not prevent XSS from running, but it removes the most common post-XSS pivot: session hijacking via cookie theft.

**Trade-off:** `HttpOnly` does not prevent all XSS damage. An XSS payload can still make authenticated requests using `fetch()` or `XMLHttpRequest` with `credentials: 'include'` — the browser will attach HttpOnly cookies to these requests automatically. But it does prevent the simpler stolen-token attack where the attacker operates from a different session.

---

## 5. Secure Flag: Prevents Cookie Transmission Over HTTP (Downgrade Attack)

**Decision:** Block cookie sending when `Secure=1` and `scheme != "https"` in `cookie_jar_get()`.

**Why:** Without the `Secure` flag, a session cookie set over HTTPS is also sent over plain HTTP connections to the same domain. An attacker performing a network-level MitM can redirect an HTTPS site visitor to the HTTP version of the site (via a network-level HTTP injection or an HTTP link), receive the session cookie in the cleartext HTTP request, and hijack the session. The `Secure` flag prevents this by instructing the browser to only include the cookie in HTTPS requests. Combined with HTTP Strict Transport Security (HSTS), this eliminates cookie exposure on downgrade attacks.

**Trade-off:** Sites must be fully HTTPS to use `Secure` cookies. If any part of the site serves over HTTP (e.g., a legacy endpoint), those pages will not receive the session cookie and may appear logged out. HSTS with `includeSubDomains` and a long `max-age` is the correct companion policy.

---

## 6. CSRF Token as the Alternative Defense (Double-Submit Cookie, Synchronizer Token)

**Decision:** Document CSRF tokens as the defense for sites that cannot use SameSite (pre-2020 browsers, legacy systems).

**Why:** Before SameSite became a reliable default, the standard CSRF defense was an explicit anti-forgery token. The **synchronizer token pattern** generates a cryptographically random value per session, stores it server-side, embeds it in every HTML form as a hidden field, and validates it on submission. A cross-origin request cannot read this value because of the Same-Origin Policy, so an attacker cannot forge a request with the correct token. The **double-submit cookie** pattern stores the token in a separate non-HttpOnly cookie and also submits it as a request parameter; the server checks that both values match. This is simpler to implement but relies on the attacker not being able to set cookies for the target domain (which subdomain attacks can violate). SameSite is generally preferred today because it requires no application-level changes, but CSRF tokens remain necessary for browsers that predate the SameSite default change.

**Trade-off:** CSRF tokens require server-side state or careful stateless token design (HMAC-based). They are invisible to the developer because the framework usually handles them, but they can break AJAX requests if the token is not included in the `X-CSRF-Token` header.
