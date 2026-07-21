#ifndef SITE_REGISTRY_H
#define SITE_REGISTRY_H

#include <unistd.h>

#define MAX_SITES 16

/* A site is the (scheme, registered-domain) pair — broader than an origin.
 * https://a.example.com and https://b.example.com are the SAME site
 * (example.com) but DIFFERENT origins.
 *
 * Each site gets exactly one renderer process. */
typedef struct {
    char  site[64];         /* e.g. "https://example.com" */
    pid_t renderer_pid;     /* OS pid of the renderer; -1 if not yet spawned */
    int   pipe_to[2];       /* parent writes -> renderer reads  */
    int   pipe_from[2];     /* renderer writes -> parent reads  */
    int   active;
} site_entry_t;

typedef struct {
    site_entry_t entries[MAX_SITES];
    int          count;
} site_registry_t;

void site_registry_init(site_registry_t *reg);

/* Extract the registrable site key from a URL (scheme://eTLD+1).
 * e.g. "https://sub.example.com/path" -> "https://example.com" */
void url_to_site(const char *url, char *site_out, int site_len);

/* Look up or create an entry for this site.
 * Returns pointer to the entry, or NULL if the registry is full. */
site_entry_t *site_registry_get_or_create(site_registry_t *reg, const char *site);

/* Print the registry */
void site_registry_print(const site_registry_t *reg);

#endif
