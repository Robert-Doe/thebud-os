# B16 — TLS Handshake State Machine: Design Decisions

## 1. TLS 1.3 vs 1.2: Certificate Is Now Encrypted

**Decision:** Demonstrate both TLS 1.2 and TLS 1.3 handshakes and highlight the encryption of the Certificate message as the key difference.

**Why:** In TLS 1.2, the ServerCertificate message is sent in cleartext before the ChangeCipherSpec message. This means any passive observer on the network can read the server's certificate, extracting the server's hostname from the Subject Alternative Names (SANs), the issuer, and expiry date. In TLS 1.3, the key exchange (Diffie-Hellman) happens within the ServerHello, and all subsequent handshake messages — EncryptedExtensions, Certificate, CertificateVerify, Finished — are encrypted with the newly derived handshake keys. A passive observer sees only the ServerHello's key share, not the certificate itself.

**Trade-off:** Encrypting the certificate makes debugging TLS handshake failures harder (you need a key log file to decrypt captured traffic in Wireshark). This is a worthwhile trade-off for the privacy gain.

---

## 2. SNI as the Remaining Hostname Leak

**Decision:** Highlight SNI as the primary remaining information leak in TLS 1.3 even after encrypting the Certificate.

**Why:** Server Name Indication (SNI) is an extension in the ClientHello that tells the server which hostname the client wants to connect to, so the server can select the right certificate for virtual hosting. ClientHello is sent before any key material is established, so it is necessarily plaintext. Every observer on the path (ISP, router, corporate firewall, government surveillance system) can read the SNI and know exactly which hostname the client is connecting to — even though the subsequent communication is encrypted. The IP address alone is insufficient for hostname identification in shared hosting environments (CDNs serve thousands of domains from one IP), so SNI remains the definitive hostname leak.

**Trade-off:** SNI is required for the server to function correctly with multiple certificates. There is no backward-compatible way to eliminate it without pre-shared key material. The forward solution is Encrypted Client Hello (ECH).

---

## 3. Certificate Chain Trust: Why Pinning Helps but Is Brittle

**Decision:** Implement chain validation through an intermediate CA to a root CA, and note pinning in the decisions.

**Why:** The root of trust in TLS is the system's certificate store — a collection of root CA certificates that the OS or browser ships. Any root CA can issue a certificate for any domain, creating a large attack surface: a compromised or malicious CA can issue a fraudulent certificate for `example.com`. Certificate pinning (HPKP or application-level pinning) records the specific certificate or CA key that a server is expected to use. A connection presenting a different certificate is rejected, even if the certificate is signed by a trusted root CA. This protects against CA misbehavior. However, pinning requires the operator to update pins before rotating certificates, and a mistake causes a site outage. Several major operators (including Google) abandoned HPKP in favor of Certificate Transparency (CT) logs as a less brittle alternative.

**Trade-off:** CT logs provide public auditing of all issued certificates without the operational risk of pinning. But CT does not prevent a fraudulent certificate from being used in the window before it is detected in the log.

---

## 4. OCSP Stapling vs CRL Download

**Decision:** Simulate an OCSP check and note stapling as the preferred mechanism.

**Why:** When a certificate is revoked (compromised private key, CA error), the browser must know about it. Two mechanisms exist: Certificate Revocation Lists (CRLs) are large files listing all revoked serial numbers, published periodically — they are slow to update and expensive to download. OCSP (Online Certificate Status Protocol) allows the browser to query the CA for a specific certificate's status in real time, but adds a round-trip latency and leaks which sites the user visits to the CA. OCSP stapling solves both problems: the server periodically fetches its own OCSP response, signs it, and includes it in the TLS handshake. The client gets a fresh revocation proof without a separate round-trip or privacy leak.

**Trade-off:** OCSP stapling requires server-side implementation. If the stapled response is missing or expired, browsers typically soft-fail (accept the connection anyway) to avoid breaking the web when OCSP infrastructure is slow. Hard-fail mode is more secure but causes outages when OCSP is unavailable.

---

## 5. Encrypted Client Hello (ECH) — The Future Fix for SNI Leakage

**Decision:** Mention ECH as the architectural solution to the SNI privacy problem.

**Why:** ECH works by encrypting the "inner" ClientHello (containing the real SNI) using a public key that the server publishes via a DNS HTTPS record (`_https._tcp`). The outer ClientHello contains a generic SNI pointing to a CDN or cover domain. An observer sees the outer SNI (e.g., `cloudflare-ech.com`) instead of the actual target hostname. The server's CDN uses its ECH private key to decrypt the inner ClientHello and route the connection. ECH is under active standardization (RFC draft) and is deployed by Cloudflare. It requires DNSSEC or DNS-over-HTTPS to protect the ECH public key from being stripped in transit.

**Trade-off:** ECH centralizes the privacy benefit at large CDN providers who can publish ECH keys at scale. Operators running their own servers must also publish ECH keys, adding operational complexity. And DNS-level censorship could block ECH by refusing to serve the HTTPS record.

---

## 6. Why Self-Signed Certificates Are Dangerous

**Decision:** Demonstrate self-signed certificate rejection in `cert_validation.c` with an explicit error path.

**Why:** A self-signed certificate is signed by its own private key — anyone can generate one for any name in seconds. The certificate carries no meaningful identity guarantee because there is no trusted third party (CA) vouching for it. When a browser accepts a self-signed certificate (after a user click-through), it has no assurance that the server is who it claims to be. An attacker performing a MitM can generate their own self-signed certificate for `bank.com` and present it to a victim. If the victim's software accepts self-signed certs, the attacker achieves a full MitM with traffic decryption. Self-signed certs are acceptable only in closed, controlled environments (internal tooling with a custom root CA distributed to all clients), never for public-facing services.

**Trade-off:** Development environments commonly use self-signed certs to avoid cost and infrastructure. The risk is that developers become accustomed to accepting cert warnings, training bad habits. Tools like `mkcert` (which installs a locally-trusted root CA) are preferable as they validate the full chain without a public CA.
