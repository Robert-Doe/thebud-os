/*
 * B05 — HTML Tokenizer & DOM
 * ===========================
 * Builds a real HTML5 tokenizer and DOM tree, then demonstrates
 * parser differentials that enable mutation XSS (mXSS).
 *
 * Build:  gcc -o demo main.c tokenizer.c dom.c
 * Run:    ./demo
 */

#include <stdio.h>
#include <string.h>
#include "tokenizer.h"
#include "dom.h"

/* Print all tokens produced from an HTML string */
static void dump_tokens(const char *html)
{
    tokenizer_t tok;
    tokenizer_init(&tok, html);
    token_t t;
    printf("Tokens for: %s\n", html);
    while (tokenizer_next(&tok, &t) == 0) {
        switch (t.type) {
        case TOK_START_TAG:
            printf("  START_TAG <%s>\n", t.tag); break;
        case TOK_END_TAG:
            printf("  END_TAG   </%s>\n", t.tag); break;
        case TOK_TEXT:
            printf("  TEXT      \"%s\"\n", t.text); break;
        case TOK_ATTR:
            printf("  ATTR      %s=\"%s\"\n", t.attr_name, t.attr_value); break;
        case TOK_EOF:
            printf("  EOF\n"); goto done;
        }
    }
done:
    printf("\n");
}

int main(void)
{
    printf("=======================================================\n");
    printf(" B05 — HTML Tokenizer & DOM Demo\n");
    printf("=======================================================\n\n");

    /* ---- Part 1: basic tokenization ---- */
    printf("--- Part 1: Tokenizer output ---\n\n");
    dump_tokens("<div class=\"foo\">Hello <b>world</b></div>");
    dump_tokens("<table><tr><td>Cell</td></tr></table>");

    /* ---- Part 2: Build and print a DOM tree ---- */
    printf("--- Part 2: DOM tree construction ---\n\n");
    const char *html1 = "<div class=\"container\"><h1>Title</h1><p>Hello <b>world</b></p></div>";
    printf("Input: %s\n\n", html1);
    dom_node_t *doc1 = dom_parse_lenient(html1);
    printf("DOM tree:\n");
    dom_print(doc1, 0);
    printf("\n");

    /* ---- Part 3: Parser differential / mXSS demo ---- */
    printf("=======================================================\n");
    printf(" Part 3: Parser Differential / Mutation XSS Demo\n");
    printf("=======================================================\n\n");
    printf("Input HTML: <table><script>alert(1)</script></table>\n\n");
    printf("This is the classic mXSS vector.  A <script> tag inside\n");
    printf("<table> is invalid HTML5.  Different parsers handle it\n");
    printf("differently, creating a security differential.\n\n");

    const char *mxss = "<table><script>alert(1)</script></table>";

    printf("--- LENIENT parser (naive/buggy sanitiser behaviour) ---\n");
    printf("Puts <script> INSIDE <table> as a child.\n\n");
    dom_reset_pool();
    dom_node_t *doc_lenient = dom_parse_lenient(mxss);
    dom_print(doc_lenient, 0);

    printf("\n--- STRICT parser (HTML5 spec / browser behaviour) ---\n");
    printf("Foster-parents <script> to BEFORE <table>.\n\n");
    dom_reset_pool();
    dom_node_t *doc_strict = dom_parse_strict(mxss);
    dom_print(doc_strict, 0);

    printf("\n=======================================================\n");
    printf(" Why this matters for security:\n");
    printf("\n");
    printf(" 1. A sanitiser runs the LENIENT parser and sees <script>\n");
    printf("    inside <table>.  It may decide 'table content cannot\n");
    printf("    execute scripts' and allow it through.\n");
    printf("\n");
    printf(" 2. The browser runs the STRICT parser (HTML5 spec).\n");
    printf("    It foster-parents <script> OUT of the table, placing\n");
    printf("    it in normal document flow where it DOES execute.\n");
    printf("\n");
    printf(" 3. The sanitiser's output looked safe; the browser's\n");
    printf("    re-parsing of that same string produced a different\n");
    printf("    DOM where the script executes.  This is mXSS.\n");
    printf("\n");
    printf(" Fix: sanitisers must parse with the SAME parser as the\n");
    printf("      target browser.  DOMPurify uses the browser's own\n");
    printf("      parser (via innerHTML) for this exact reason.\n");
    printf("=======================================================\n");

    return 0;
}
