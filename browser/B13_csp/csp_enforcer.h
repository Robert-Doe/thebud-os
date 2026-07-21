#ifndef CSP_ENFORCER_H
#define CSP_ENFORCER_H

#include "csp_parser.h"

/* Check whether loading `source_url` is permitted under directive
   `directive_name` in policy `p`.
   Returns 1 if ALLOWED, 0 if BLOCKED. */
int csp_check(struct csp_policy *p, const char *directive_name,
              const char *source_url);

/* Check whether inline script execution is allowed under script-src. */
int csp_check_inline_script(struct csp_policy *p);

/* Check whether eval() is allowed under script-src. */
int csp_check_eval(struct csp_policy *p);

#endif /* CSP_ENFORCER_H */
