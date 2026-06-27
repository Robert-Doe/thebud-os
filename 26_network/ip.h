/*
 * ip.h — IPv4 header and input/output API
 */

#ifndef IP_H
#define IP_H

#include <stdint.h>

#define IP_PROTO_UDP  17
#define IP_PROTO_ICMP  1

struct ip_hdr {
    uint8_t  ver_ihl;    /* version (4) | IHL (5) */
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} __attribute__((packed));

uint16_t ip_checksum(const void *data, uint16_t len);
void     ip_input(const uint8_t *pkt, uint16_t len);
int      ip_send(uint32_t dst_ip, uint8_t proto,
                 const uint8_t *payload, uint16_t plen);

#endif /* IP_H */
