/*
 * net.c — Network stack pump
 *
 * Calls ne2000_recv() to pull frames off the NIC, then dispatches
 * to arp_input() or ip_input() based on ethertype.
 */

#include "net.h"
#include "ne2000.h"
#include "ip.h"
#include "vga.h"
#include <stdint.h>

static uint8_t rx_buf[ETH_FRAME_MAX];

void net_init(void) {
    ne2000_init();
    vga_print("[NET] stack ready  IP=10.0.0.1\n");
}

void net_poll(void) {
    int len;
    while ((len = ne2000_recv(rx_buf, sizeof(rx_buf))) > 0) {
        if (len < ETH_HEADER_LEN) continue;
        struct eth_frame *eth = (struct eth_frame *)rx_buf;
        uint16_t et = htons(eth->ethertype);
        if (et == ETHERTYPE_IP)
            ip_input(eth->payload, (uint16_t)(len - ETH_HEADER_LEN));
        /* ARP is handled inside ip.c for simplicity */
    }
}
