#ifndef RENDERER_POOL_H
#define RENDERER_POOL_H

#include "site_registry.h"
#include <unistd.h>

/* Spawn a renderer process for the given site entry.
 * Returns the child pid, or -1 on error. */
pid_t renderer_pool_spawn(site_entry_t *entry, const char *site);

/* Send a message to the renderer via IPC pipe. */
void renderer_pool_send(site_entry_t *entry, const char *msg);

/* Receive a response from the renderer. */
int  renderer_pool_recv(site_entry_t *entry, char *out, int len);

/* Send EXIT and wait for the renderer to terminate. */
void renderer_pool_shutdown(site_entry_t *entry);

#endif
