#ifndef BROWSER_KERNEL_H
#define BROWSER_KERNEL_H

/* Run the browser kernel event loop.
 * Reads requests from req_read_fd, writes responses to resp_write_fd. */
void browser_kernel_run(int req_read_fd, int resp_write_fd);

#endif
