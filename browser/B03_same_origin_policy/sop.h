#ifndef SOP_H
#define SOP_H

#include "origin.h"

typedef enum {
    SOP_ALLOW,
    SOP_BLOCK
} sop_decision_t;

/* Check whether a request from `from` to `to` should be allowed.
 * Returns SOP_ALLOW or SOP_BLOCK and sets reason (must be >= 64 bytes). */
sop_decision_t sop_check(const origin_t *from, const origin_t *to, char *reason);

/* Helper: parse two URLs and run the check.  Prints a formatted result line. */
void sop_test(const char *label, const char *from_url, const char *to_url);

#endif
