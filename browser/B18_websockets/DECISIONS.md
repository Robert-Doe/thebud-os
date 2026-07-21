# B18 — WebSockets & postMessage: Design Decisions

## 1. Why WebSocket Upgrade Bypasses HTTP Request Semantics (No CORS on WS)

**Decision:** Show that the WS upgrade request carries cookies and bypasses CORS, demonstrated in the CSWSH section.

**Why:** CORS is a mechanism that restricts how JavaScript can read cross-origin HTTP **responses**. It does not prevent cross-origin requests from being sent — it only controls whether the JavaScript can access the response body. WebSocket connections, however, are established via an HTTP Upgrade request, and the browser attaches cookies to this request following normal cookie rules (SameSite, Secure, Domain — but not any CORS restriction). Once the WS connection is established, JavaScript can send and receive messages freely — there is no CORS preflight for subsequent WS frames. An attacker page at `evil.com` can open a WebSocket to `chat.example.com`, and if the victim's cookies are attached (SameSite=None or Lax with top-level nav), the server sees an authenticated connection from the attacker.

**Trade-off:** There is no browser-level CORS analog for WebSockets. The only defenses are server-side `Origin` header validation (treat it like a CSRF check) and relying on SameSite=Strict cookies to prevent the cookies from being sent on the upgrade.

---

## 2. WebSocket Masking: Client-to-Server Only — Why (Prevents Cache Poisoning)

**Decision:** Enforce masking on client→server frames in `ws_encode_frame()` and zero it on server→client frames.

**Why:** RFC 6455 mandates that frames sent from client to server MUST be masked with a random 4-byte key, while server→client frames MUST NOT be masked. The reason is subtle: without masking, a malicious web page could craft WebSocket frames that, when received by an intermediary proxy, look like HTTP responses. Proxies that cache `GET` responses might be poisoned into caching a fake response that the attacker embedded in a WS frame. By masking client frames with a per-frame random key, any intermediary that tries to interpret the bytes as HTTP sees random garbage. Server frames don't need masking because the server is not controlled by a potentially malicious script.

**Trade-off:** Masking adds a small CPU overhead (XOR every byte). It is not encryption — an observer who can read the frame header reads the mask key and can trivially unmask the payload. The purpose is solely to prevent proxy cache poisoning, not to provide confidentiality.

---

## 3. The WebSocket GUID in Accept Key — Why It Prevents Non-WS Servers from Accidentally Accepting WS

**Decision:** Include the GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11` in `ws_compute_accept()` and explain its purpose.

**Why:** The Sec-WebSocket-Accept header is computed as `base64(SHA1(key + GUID))`. The GUID is a fixed, publicly known string, but that is intentional — its purpose is not secrecy. A plain HTTP server that does not know about WebSockets would respond to the Upgrade request with a 200 OK or 404 (whatever its routing logic produces), not a 101 Switching Protocols. But if a server accidentally responds 101 without computing the Accept key, the client can detect this: the client computes the expected Accept value and validates the server's response. A non-WS server cannot produce the correct Accept value because it doesn't know to concatenate the GUID with the client's key. The GUID is the sentinel that proves the server explicitly implemented RFC 6455.

**Trade-off:** Because the GUID is public, it provides no security against an attacker who deliberately implements a fake WS server. It only prevents accidental acceptance by naive HTTP servers.

---

## 4. `postMessage` with `*` as Target Origin: When Acceptable vs Dangerous

**Decision:** Show both the wildcard case and the specific-origin case, with clear labeling of when `*` is acceptable.

**Why:** `window.postMessage(data, "*")` delivers the message to any origin that has a reference to the target window. This is acceptable for public, non-sensitive data — for example, an analytics widget that broadcasts a pageview event doesn't care which origins receive it. It becomes dangerous when the message contains sensitive data (session tokens, personal information, authentication state). In that case, any window (including a malicious iframe or a popup opened by an attacker) can receive the message. The sender must specify the exact expected recipient origin so the browser will refuse delivery to any other window.

**Trade-off:** Strict target origins require the sender to know the receiver's origin at call time, which can be difficult in federated scenarios. A common pattern is for the receiver to send its origin to the sender via a separate handshake message, then the sender uses that origin for subsequent `postMessage` calls.

---

## 5. Why `event.origin` Check Is the Receiver's Responsibility

**Decision:** Implement both `pm_receive_safe()` and `pm_receive_unsafe()` to contrast the two patterns.

**Why:** The browser delivers `postMessage` events based on the `targetOrigin` the sender specified. But if the sender uses `"*"`, all windows receive the message. Even when `targetOrigin` is specific, the receiver must still validate `event.origin` independently, because: (1) the same window may receive messages from multiple senders, and (2) the browser's delivery check is about the target, not the source. A receiver that processes `event.data` without checking `event.origin` will execute whatever any origin sends it. If the receiver does something privileged with the data (executes code, changes auth state, navigates to a URL), an attacker at any origin can trigger that action by sending a crafted postMessage.

**Trade-off:** Requiring developers to always check `event.origin` is a documentation and discipline problem, not an enforcement problem. The browser will not automatically reject unverified messages on the receiver's behalf. Frameworks and libraries should wrap `addEventListener('message', ...)` with built-in origin validation to reduce mistakes.

---

## 6. CSWSH — Cross-Site WebSocket Hijacking: Cookies Sent with WS Upgrade → CSRF for WebSockets

**Decision:** Dedicate a full demo section to CSWSH because it is the WS analog of CSRF and is frequently overlooked.

**Why:** When a browser opens a WebSocket connection, it includes cookies for the target domain following the same rules as any cross-origin request — specifically, it does NOT apply CORS restrictions, and SameSite=None or Lax (with certain conditions) cookies are included. This means an attacker page can open a WS connection to `wss://victim.com/ws`, and if the victim's session cookie has `SameSite=None`, the browser attaches it. The server sees an authenticated WebSocket connection. The attacker can then send and receive messages as the victim. This attack was publicly demonstrated against several real-world chat applications. The mitigations are: (1) validate `Origin` header on the WS upgrade request (reject origins not in your allowlist); (2) use `SameSite=Strict` on session cookies (prevents cookies from being sent on cross-site WS upgrades); (3) require a CSRF token in the WS handshake URL or query parameter.

**Trade-off:** Origin header validation is straightforward but the Origin header can be spoofed by non-browser clients. For browser security (the primary CSWSH threat), Origin validation is sufficient since browsers enforce the header accurately.
