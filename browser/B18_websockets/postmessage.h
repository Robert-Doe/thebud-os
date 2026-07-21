#ifndef POSTMESSAGE_H
#define POSTMESSAGE_H

struct pm_message {
    char origin[128];        /* sender's origin */
    char data[1024];         /* message data */
    char target_origin[128]; /* "*" or specific expected origin */
};

/* Returns 1 if the message should be delivered to receiver_origin, 0 if blocked */
int pm_should_deliver(const struct pm_message *msg,
                      const char *receiver_origin);

/* Simulate a receiver that checks event.origin before processing */
void pm_receive_safe(const struct pm_message *msg,
                     const char *my_origin,
                     const char *trusted_origin);

/* Simulate a receiver that does NOT check event.origin — vulnerable */
void pm_receive_unsafe(const struct pm_message *msg,
                       const char *my_origin);

#endif /* POSTMESSAGE_H */
