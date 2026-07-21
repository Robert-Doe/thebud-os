#ifndef RENDERER_H
#define RENDERER_H

/* Run the renderer event loop.
 * Writes requests to req_write_fd, reads responses from resp_read_fd. */
void renderer_run(int req_write_fd, int resp_read_fd);

#endif
