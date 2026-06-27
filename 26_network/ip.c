/*
 * ip.c — Minimal IPv4 layer
 *
 * Handles:
 *   - Receiving IPv4 packets and dispatching to UDP
 *   - Building and sending IPv4 packets via the NE2000
 *   - IP checksum computation (one's complement sum)
 *
 * Does NOT handle: fragmentation, ICMP, routing beyond the default gateway.
 */

#include "ip.h"
#include "net.h"
#include "ne2000.h"
#include "udp.h"
#include "vga.h"
#include <stdint.h>

static uint8_t tx_frame[ETH_FRAME_MAX];
static uint16_t ip_id_counter = 1;

/* One's complement checksum — used for IP and UDP headers */
uint16_t ip_checksum(const void *data, uint16_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

void ip_input(const uint8_t *pkt, uint16_t len) {
    if (len < (uint16_t)sizeof(struct ip_hdr)) return;
    const struct ip_hdr *ip = (const struct ip_hdr *)pkt;

    /* Only handle IPv4, no options (IHL=5) */
    if ((ip->ver_ihl >> 4) != 4) return;
    uint16_t ihl = (uint16_t)((ip->ver_ihl & 0x0F) * 4);
    if (ihl < 20 || len < ihl) return;

    /* Verify destination is us (or broadcast) */
    uint32_t dst = ntohl(ip->dst);
    if (dst != MY_IP && dst != 0xFFFFFFFFu) return;

    if (ip->proto == IP_PROTO_UDP)
        udp_input(pkt + ihl, (uint16_t)(len - ihl), ntohl(ip->src));
}

int ip_send(uint32_t dst_ip, uint8_t proto,
            const uint8_t *payload, uint16_t plen) {
    uint16_t total = (uint16_t)(sizeof(struct ip_hdr) + plen);
    if (ETH_HEADER_LEN + total > ETH_FRAME_MAX) return -1;

    /* Build Ethernet header */
    struct eth_frame *eth = (struct eth_frame *)tx_frame;
    /* Destination MAC: broadcast — ARP would go here in a real stack */
    for (int i = 0; i < 6; i++) eth->dst[i] = 0xFF;
    ne2000_get_mac(eth->src);
    eth->ethertype = htons(ETHERTYPE_IP);

    /* Build IP header */
    struct ip_hdr *ip = (struct ip_hdr *)(tx_frame + ETH_HEADER_LEN);
    ip->ver_ihl   = 0x45;
    ip->tos       = 0;
    ip->total_len = htons(total);
    ip->id        = htons(ip_id_counter++);
    ip->frag_off  = 0;
    ip->ttl       = 64;
    ip->proto     = proto;
    ip->checksum  = 0;
    ip->src       = htonl(MY_IP);
    ip->dst       = htonl(dst_ip);
    ip->checksum  = ip_checksum(ip, sizeof(struct ip_hdr));

    /* Copy payload */
    uint8_t *data = tx_frame + ETH_HEADER_LEN + sizeof(struct ip_hdr);
    for (uint16_t i = 0; i < plen; i++) data[i] = payload[i];

    return ne2000_send(tx_frame, (uint16_t)(ETH_HEADER_LEN + total));
}
