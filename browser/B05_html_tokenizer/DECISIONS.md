# B05 — HTML Tokenizer & DOM: Design Decisions

## 1. State Machine Tokenizer (Not Regex) — Why

**Decision:** The tokenizer is implemented as an explicit state machine that consumes characters one at a time, rather than as a series of regular expressions applied to the input.

**Why:** HTML is not a regular language. It contains context-dependent rules: the same character sequence means different things depending on whether the parser is inside a `<script>` tag, a `<style>` block, a comment, or normal markup. Regex cannot express these context dependencies without growing to unmaintainable size. The HTML5 specification itself defines the tokenizer as a state machine with 80+ states. Our simplified version handles the common subset (start tags, end tags, text, attributes) using the same character-by-character dispatch pattern. This scales to the full spec in a way that regex does not.

**Trade-off:** A state machine is more code than a regex. For the simple HTML we parse here, a handful of regexes would have been shorter. The payoff is correctness under adversarial input: a regex-based parser fails on edge cases like `<div class=foo'bar>` or `<!-- - ->` that real web content contains. Security-sensitive code (a sanitiser) must use the full state machine to avoid being confused by adversarial inputs.

---

## 2. Foster Parenting / Implicit Error Recovery

**Decision:** The strict parser implements HTML5 foster-parenting: when a tag that is illegal as table content appears inside `<table>`, it is "foster-parented" to before the table in the DOM.

**Why:** The HTML5 parsing algorithm has explicit error recovery rules for every invalid construct. These rules exist because the web contains enormous amounts of malformed HTML that browsers must render consistently. The foster-parenting rule for `<table>` is one of the trickiest: `<table><b>text</b></table>` is invalid, but the spec says the `<b>` should appear before the table, not inside it. This is not a quirk — it is the specified behaviour, and all compliant browsers implement it. Any tool that re-serialises and re-parses HTML must implement the same rules.

**Trade-off:** Foster parenting makes the parser significantly more complex for a narrow class of inputs. A simpler approach — "just put the bad tag wherever it appears" — is easier to implement and works for well-formed input. But it creates parser differentials (see Decision 3) that attackers actively exploit.

---

## 3. Why Parser Differentials Create Security Bugs (mXSS)

**Decision:** The demo explicitly shows the same HTML string producing different DOM trees in the lenient and strict parsers, with the explanation that this is the mXSS (mutation XSS) attack class.

**Why:** A sanitiser parses HTML, removes dangerous tags, and re-serialises the result. If the sanitiser's parser and the browser's parser handle edge cases differently, the sanitiser may produce output that looks safe to it but is re-parsed into a dangerous DOM by the browser. The `<table><script>` case is canonical: the sanitiser may decide the script is inside a table (where it seems innocuous) and allow it; the browser foster-parents it out of the table into normal document flow where it executes. The XSS payload "mutates" during the browser's re-parse — hence "mutation XSS."

**Trade-off:** The only complete defence is to sanitise using the exact same parser as the target rendering environment. In practice this means running DOMPurify inside the browser itself (using the browser's `innerHTML` setter as the parser), or using a well-tested server-side library that implements the full HTML5 parsing algorithm.

---

## 4. innerHTML Re-parsing Creates a Different DOM Than Original Parsing

**Decision:** The demo notes (in the mXSS explanation) that serialising a DOM to a string and re-parsing it via `innerHTML` can produce a different DOM than the original parse of the source HTML.

**Why:** The HTML5 spec defines the serialisation algorithm (`outerHTML`) and the parsing algorithm separately, and they are not inverses of each other. Round-tripping through serialisation can introduce differences because: (a) the serialiser may produce canonical HTML that triggers different parser error recovery than the original malformed source; (b) the parsing context changes when inserting via `innerHTML` vs. top-level document parsing — `<table>` has different insertion modes in each context. This is the precise mechanism exploited by mXSS attacks on sanitisers.

**Trade-off:** This property is not a bug in the spec — it is a necessary consequence of the spec handling millions of malformed documents consistently. The implication for security engineers is that "sanitise, serialise, and hand to innerHTML" is not safe unless the sanitiser uses the same contextual parsing mode as the final innerHTML call.

---

## 5. Why Sanitisers Must Run in the Same Parser as the Target Page

**Decision:** The DECISIONS.md explicitly states that DOMPurify's approach (running inside the browser) is the correct architectural choice.

**Why:** Server-side sanitisers written in Python, Java, or C use their own HTML parser implementations. Even with perfect specification compliance, subtle differences between implementations — character encoding handling, null-byte handling, attribute parsing edge cases — create parser differentials. The only parser guaranteed to be identical to the browser's parser is the browser's own parser. DOMPurify, the most widely deployed client-side sanitiser, creates a detached `div` element and sets its `innerHTML` to the untrusted string, letting the browser parse it, then walks the resulting DOM to remove dangerous nodes. It then serialises the safe DOM back to a string. Both the parsing and the serialisation use the browser's own engine, eliminating differential risk.

**Trade-off:** Running the sanitiser in the browser requires JavaScript execution, which is not always available (e.g., server-side rendering). For server-side use, the safest option is a library like `Bleach` (Python) that uses an HTML5-spec-compliant parser (`html5lib`) rather than a hand-rolled regex or SGML parser.
