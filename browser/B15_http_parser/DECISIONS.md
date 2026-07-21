# B15 — HTTP/1.1 Parser & Request Smuggling: Design Decisions

## 1. Why Ambiguous Message Framing Creates Smuggling (CL vs TE)

**Decision:** Implement two independent framing interpretations of the same byte stream to show divergence.

**Why:** HTTP/1.1 has two message-framing mechanisms: `Content-Length` (byte count) and `Transfer-Encoding: chunked` (self-delimiting chunk stream). When both headers are present in a single request, RFC 7230 §3.3.3 requires that `Transfer-Encoding` takes precedence and `Content-Length` be ignored. However, many real-world proxies and load balancers were written before this rule was clarified or chose not to implement it strictly. When the front-end parser uses one rule and the back-end uses the other, a single TCP byte stream is parsed as two different request sequences. Bytes the front-end considers part of request N become the beginning of request N+1 at the back-end — a "smuggled" prefix that the back-end appends to the next real request.

**Trade-off:** Strictly enforcing RFC 7230 §3.3.3 at both tiers would eliminate the ambiguity. The challenge is that the rule was lightly enforced for years, and retrofitting it to legacy middleware at scale is an operational challenge.

---

## 2. RFC 7230 Rule: If Both CL and TE Present, TE Wins — But Proxies Don't Always Implement This

**Decision:** Document the RFC requirement explicitly and show that the vulnerability exists precisely because the rule is not universally enforced.

**Why:** RFC 7230 §3.3.3 is unambiguous: "If a message is received that has multiple Content-Length header fields with field-values consisting of the same decimal value, or a single Content-Length header field with a field value containing a list of identical decimal values … the recipient MUST either reject the message … or replace … with a single valid Content-Length." And: if Transfer-Encoding is present, Content-Length must be removed. A proxy that forwards both headers verbatim creates the attack surface. The rule's intent is precisely to prevent divergent parsing — its non-enforcement is the vulnerability, not a design flaw in the spec itself.

**Trade-off:** Enforcing the rule at the proxy requires inspecting and potentially rewriting headers, adding latency. Proxies designed for throughput sometimes skip this for performance. The correct trade-off is to accept the small overhead.

---

## 3. Why Request Smuggling Bypasses WAFs and Access Controls

**Decision:** Explain why smuggling is particularly dangerous at the WAF boundary.

**Why:** A Web Application Firewall sits at the front-end tier and inspects the request it parses. In a CL.TE attack, the WAF sees request N as complete (it used Content-Length). The smuggled prefix that will be appended to request N+1 at the back-end is never inspected by the WAF — it was part of request N's body from the WAF's perspective. The back-end then constructs request N+1 as `[smuggled prefix][N+1 headers]`, where the smuggled prefix can control the method, path, and headers. This lets an attacker inject a crafted request to a protected path (e.g., `/admin`) that the WAF never examines.

**Trade-off:** This attack requires the attacker to share a back-end TCP connection with victim requests, which is typical in connection-pooled reverse proxy architectures. It is not exploitable in direct client→server connections without a proxy tier.

---

## 4. Chunked Encoding Parsing Pitfalls

**Decision:** Implement a strict chunked decoder in `http_parse_chunked_body()` that handles size lines, CRLF terminators, and the zero-size terminator.

**Why:** Chunked encoding is a source of many parsing bugs. Each chunk is prefixed with its hex size on a line ending with `\r\n`, followed by exactly that many bytes, followed by another `\r\n`. A parser that accepts `\n` instead of `\r\n`, or that miscounts bytes due to off-by-one errors, or that ignores chunk extensions (`;ext=value`), will diverge from a strict parser on the same byte stream — creating another smuggling vector. Trailer headers after the terminal `0\r\n` are another source of divergence: some parsers process them as headers, others ignore them.

**Trade-off:** A paranoid parser that rejects any deviation from the exact ABNF is correct but may break compatibility with some legitimate servers that produce slightly non-conformant chunked bodies. Logging and rejecting is safer than silently accepting.

---

## 5. HTTP/2 Eliminates Smuggling at L7 — But H2→H1 Downgrade Re-Introduces It

**Decision:** Note the HTTP/2 mitigation and the downgrade caveat.

**Why:** HTTP/2 uses binary framing with explicit, fixed-length frame headers. Every frame has a type and length embedded in its header — there is no ambiguity between Content-Length and Transfer-Encoding because neither concept exists at the HTTP/2 framing layer. Request smuggling in the classic CL.TE sense is impossible between two HTTP/2 speakers. However, when a front-end accepts HTTP/2 from clients but downgrades to HTTP/1.1 when connecting to the back-end (a common pattern), it must serialize HTTP/2 pseudo-headers into HTTP/1.1 headers. If a client sends a malformed HTTP/2 request with a `:content-length` pseudo-header that disagrees with the actual DATA frame length, and the proxy does not validate this before downgrading, the smuggling attack re-emerges at the H1 back-end.

**Trade-off:** The fix is to use HTTP/2 end-to-end, or to have the H2→H1 proxy normalize all framing information before forwarding.

---

## 6. Mitigations: Reject Ambiguous Requests, Normalize at Proxy

**Decision:** List concrete mitigations at the end of the demo output.

**Why:** The definitive mitigations are: (1) At every proxy/middleware tier, reject any request that contains both a `Content-Length` and a `Transfer-Encoding` header. RFC 7230 permits this. (2) If rejecting is not feasible, strip `Content-Length` before forwarding whenever `Transfer-Encoding: chunked` is present. (3) Use HTTP/2 end-to-end where possible. (4) Configure back-end servers to close, rather than reuse, TCP connections after each request when operating behind a proxy that may not normalize framing. (5) Apply strict timeouts on connection reuse to reduce the window for cross-request pollution.

**Trade-off:** Rejecting ambiguous requests can cause false positives with misconfigured legitimate clients. Monitoring rejection rates before enforcing is advisable.
