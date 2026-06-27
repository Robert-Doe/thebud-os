/*
 * ne2000.h — NE2000-compatible NIC driver (ISA/PCI, QEMU e1000 compat mode)
 *
 * The NE2000 was the de-facto standard ISA NIC of the 1990s. QEMU emulates
 * it on ISA IRQ 9, I/O base 0x300. Every OS textbook uses it first because
 * the register map is simple and well-documented.
 */

#ifndef NE2000_H
#define NE2000_H

#include <stdint.h>

#define NE2000_IOBASE  0x300   /* default ISA I/O base */
#define NE2000_IRQ     9       /* ISA IRQ line */

/* Maximum Ethernet frame size (payload only, no FCS) */
#define ETH_MTU        1500
#define ETH_HEADER_LEN 14     /* 6 dst + 6 src + 2 ethertype */
#define ETH_FRAME_MAX  (ETH_MTU + ETH_HEADER_LEN)

/* Ethertype constants */
#define ETHERTYPE_IP   0x0800
#define ETHERTYPE_ARP  0x0806

/* Ethernet frame header */
struct eth_frame {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
    uint8_t  payload[];
} __attribute__((packed));

/* Public API */
void    ne2000_init(void);
int     ne2000_send(const uint8_t *frame, uint16_t len);
int     ne2000_recv(uint8_t *buf, uint16_t buflen);
void    ne2000_get_mac(uint8_t mac[6]);
void    ne2000_irq_handler(void);

#endif /* NE2000_H */
