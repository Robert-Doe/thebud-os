#ifndef DOM_H
#define DOM_H

#define DOM_MAX_CHILDREN 16
#define DOM_MAX_ATTRS     8

typedef struct dom_attr {
    char name[32];
    char value[128];
} dom_attr_t;

typedef struct dom_node {
    char  tag[32];              /* element tag name, or "" for text nodes */
    char  text[256];            /* text content (only for text nodes) */
    int   is_text;              /* 1 = text node, 0 = element node */
    dom_attr_t attrs[DOM_MAX_ATTRS];
    int   attr_count;
    struct dom_node *children[DOM_MAX_CHILDREN];
    int   child_count;
} dom_node_t;

/* Allocate and zero a new node from a static pool */
dom_node_t *dom_new_node(void);

/* Reset the static node pool (for running multiple parses) */
void dom_reset_pool(void);

/* Append child to parent */
void dom_append_child(dom_node_t *parent, dom_node_t *child);

/* Add an attribute to a node */
void dom_add_attr(dom_node_t *node, const char *name, const char *value);

/* Print the DOM tree with indentation */
void dom_print(const dom_node_t *node, int depth);

/* Build a DOM from an HTML string using the LENIENT parser
 * (puts <script> inside <table> without foster-parenting) */
dom_node_t *dom_parse_lenient(const char *html);

/* Build a DOM from an HTML string using the STRICT parser
 * (implements foster-parenting: moves <script> outside <table>) */
dom_node_t *dom_parse_strict(const char *html);

#endif
