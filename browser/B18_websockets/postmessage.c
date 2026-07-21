#include "postmessage.h"
#include <stdio.h>
#include <string.h>

int pm_should_deliver(const struct pm_message *msg,
                      const char *receiver_origin) {
    /* If targetOrigin is "*", deliver to any origin */
    if (strcmp(msg->target_origin, "*") == 0) return 1;

    /* If targetOrigin is specific, only deliver if receiver matches */
    return strcmp(msg->target_origin, receiver_origin) == 0;
}

void pm_receive_safe(const struct pm_message *msg,
                     const char *my_origin,
                     const char *trusted_origin) {
    printf("  Receiver (%s) got postMessage:\n", my_origin);
    printf("    event.origin = '%s'\n", msg->origin);
    printf("    event.data   = '%s'\n", msg->data);

    /* SAFE: always check origin before processing */
    if (strcmp(msg->origin, trusted_origin) != 0) {
        printf("    Origin check FAILED: expected '%s', got '%s'\n",
               trusted_origin, msg->origin);
        printf("    Message REJECTED (safe receiver)\n");
        return;
    }
    printf("    Origin check PASSED\n");
    printf("    Processing data: '%s'\n", msg->data);
}

void pm_receive_unsafe(const struct pm_message *msg,
                       const char *my_origin) {
    printf("  Receiver (%s) got postMessage:\n", my_origin);
    printf("    event.origin = '%s' (NOT CHECKED!)\n", msg->origin);
    printf("    event.data   = '%s'\n", msg->data);
    printf("    Processing data without origin check: '%s'\n", msg->data);
    printf("    *** UNSAFE: attacker-controlled data executed/processed ***\n");
}
