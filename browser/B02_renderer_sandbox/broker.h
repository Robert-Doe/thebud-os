#ifndef BROKER_H
#define BROKER_H

/* Run the broker event loop.
 * Reads requests from req_read_fd, writes responses to resp_write_fd. */
void broker_run(int req_read_fd, int resp_write_fd);

#endif
