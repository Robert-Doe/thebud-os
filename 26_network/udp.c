/*
 * udp.c — Minimal UDP layer
 *
 * Supports sending and receiving UDP datagrams.  Sockets are just an array
 * of (port → rx_buffer) entries — no dynamic allocation, no TCP, no select.
 * One packet can be buffered per socket (oldest is overwritten if unread).
 */

#include "udp.h"
#include "ip.h"
#include "net.h"
#include "vga.h"
#include <stdint.h>

struct udp_socket {
    uint16_t         port;
    int              bound;
    struct udp_rx_entry rx;
};

static struct udp_socket sockets[UDP_MAX_SOCKETS];

void udp_input(const uint8_t *seg, uint16_t len, uint32_t src_ip) {
    if (len < (uint16_t)sizeof(struct udp_hdr)) return;
    const struct udp_hdr *udp = (const struct udp_hdr *)seg;
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t data_len = (uint16_t)(ntohs(udp->length) - sizeof(struct udp_hdr));
    const uint8_t *data = seg + sizeof(struct udp_hdr);

    /* Find a socket bound to dst_port */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (sockets[i].bound && sockets[i].port == dst_port) {
            struct udp_rx_entry *rx = &sockets[i].rx;
            rx->src_ip   = src_ip;
            rx->src_port = ntohs(udp->src_port);
            rx->data_len = (data_len < UDP_BUF_SIZE) ? data_len : UDP_BUF_SIZE;
            for (uint16_t j = 0; j < rx->data_len; j++)
                rx->data[j] = data[j];
            rx->valid = 1;
            return;
        }
    }
    /* No socket — silently discard */
}

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
             const uint8_t *data, uint16_t dlen) {
    static uint8_t udp_buf[sizeof(struct udp_hdr) + UDP_BUF_SIZE];
    if (dlen > UDP_BUF_SIZE) return -1;

    struct udp_hdr *udp = (struct udp_hdr *)udp_buf;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons((uint16_t)(sizeof(struct udp_hdr) + dlen));
    udp->checksum = 0;   /* checksum optional for UDP over IPv4 */
    for (uint16_t i = 0; i < dlen; i++)
        udp_buf[sizeof(struct udp_hdr) + i] = data[i];

    return ip_send(dst_ip, IP_PROTO_UDP, udp_buf,
                   (uint16_t)(sizeof(struct udp_hdr) + dlen));
}

int udp_bind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!sockets[i].bound) {
            sockets[i].port  = port;
            sockets[i].bound = 1;
            sockets[i].rx.valid = 0;
            return i;
        }
    }
    return -1;
}

int udp_recvfrom(int sock_id, uint8_t *buf, uint16_t buflen,
                 uint32_t *src_ip, uint16_t *src_port) {
    if (sock_id < 0 || sock_id >= UDP_MAX_SOCKETS) return -1;
    struct udp_rx_entry *rx = &sockets[sock_id].rx;
    if (!rx->valid) return 0;

    uint16_t n = (rx->data_len < buflen) ? rx->data_len : buflen;
    for (uint16_t i = 0; i < n; i++) buf[i] = rx->data[i];
    if (src_ip)   *src_ip   = rx->src_ip;
    if (src_port) *src_port = rx->src_port;
    rx->valid = 0;
    return n;
}
