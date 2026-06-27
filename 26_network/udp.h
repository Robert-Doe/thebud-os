/*
 * udp.h — Minimal UDP layer
 */

#ifndef UDP_H
#define UDP_H

#include <stdint.h>

#define UDP_MAX_SOCKETS 8
#define UDP_BUF_SIZE    512

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;    /* header + data */
    uint16_t checksum;  /* 0 = disabled */
} __attribute__((packed));

/* Receive buffer entry for a bound socket */
struct udp_rx_entry {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t data_len;
    uint8_t  data[UDP_BUF_SIZE];
    int      valid;
};

void udp_input(const uint8_t *seg, uint16_t len, uint32_t src_ip);
int  udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const uint8_t *data, uint16_t dlen);

/* Socket-level bind / recv (used by syscall layer) */
int  udp_bind(uint16_t port);            /* returns socket id or -1 */
int  udp_recvfrom(int sock_id, uint8_t *buf, uint16_t buflen,
                  uint32_t *src_ip, uint16_t *src_port);

#endif /* UDP_H */
