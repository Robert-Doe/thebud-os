#ifndef TOKENIZER_H
#define TOKENIZER_H

/* ---------------------------------------------------------------
 * Simplified HTML5 tokenizer.
 *
 * Handles: start tags, end tags, text nodes, and attributes.
 * Does NOT handle: comments, CDATA, doctype, character references.
 * --------------------------------------------------------------- */

typedef enum {
    TOK_START_TAG,   /* <div class="foo"> */
    TOK_END_TAG,     /* </div>            */
    TOK_TEXT,        /* plain text        */
    TOK_ATTR,        /* attribute key=val (emitted after TOK_START_TAG) */
    TOK_EOF          /* end of input      */
} token_type_t;

typedef struct {
    token_type_t type;
    char tag[32];           /* tag name (start/end tags) */
    char attr_name[32];     /* attribute name (TOK_ATTR only) */
    char attr_value[128];   /* attribute value (TOK_ATTR only) */
    char text[256];         /* text content (TOK_TEXT only) */
} token_t;

/* Tokenizer state (opaque to caller) */
typedef struct {
    const char *src;   /* input string */
    int         pos;   /* current read position */
    int         len;   /* total length */
} tokenizer_t;

void tokenizer_init(tokenizer_t *t, const char *html);

/* Advance to the next token. Returns 0 normally, -1 on EOF. */
int tokenizer_next(tokenizer_t *t, token_t *out);

const char *token_type_name(token_type_t tt);

#endif
