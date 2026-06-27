/*
 * net.h — Top-level network stack API
 *
 * Ties together the NE2000 driver, ARP, IP, and UDP layers.
 * The kernel calls net_init() once, then net_poll() from the scheduler
 * or a dedicated kernel thread to process incoming frames.
 */

#ifndef NET_H
#define NET_H

#include <stdint.h>

/* Our IP address and subnet — hardcoded for simplicity */
#define MY_IP   0x0A000001u   /* 10.0.0.1 */
#define MY_MASK 0xFFFFFF00u   /* /24      */
#define MY_GW   0x0A000002u   /* 10.0.0.2 — QEMU default gateway */

void net_init(void);
void net_poll(void);  /* call periodically to pump incoming frames */

/* Utility: big-endian (network) byte order conversions */
static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x000000FFu) << 24);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

#endif /* NET_H */
