/*
 * ne2000.c — NE2000-compatible NIC driver
 *
 * The NE2000 uses the DP8390 chip with a 16 KB ring buffer split into
 * TX (pages 0x40-0x45) and RX (pages 0x46-0x7f). We use polled I/O
 * here — no DMA, no interrupts — to keep the code readable.
 *
 * Register map (page 0):
 *   0x00 CR    Command Register
 *   0x01 PSTART Receive buffer start page
 *   0x02 PSTOP  Receive buffer stop page
 *   0x03 BNRY   Boundary pointer (last read page)
 *   0x04 TSR/TPSR Transmit Status / Start Page
 *   0x05 TBCR0  Transmit byte count low
 *   0x06 TBCR1  Transmit byte count high
 *   0x07 ISR    Interrupt Status Register
 *   0x08 RSAR0  Remote start address low (DMA)
 *   0x09 RSAR1  Remote start address high
 *   0x0A RBCR0  Remote byte count low
 *   0x0B RBCR1  Remote byte count high
 *   0x0C RCR    Receive Configuration Register
 *   0x0D TCR    Transmit Configuration Register
 *   0x0E DCR    Data Configuration Register
 *   0x0F IMR    Interrupt Mask Register
 *   0x10 DATA   NIC Data Register (16-bit)
 *   0x1F RESET  Reset port (read to reset)
 */

#include "ne2000.h"
#include "io.h"
#include "vga.h"

#define NE_CR    (NE2000_IOBASE + 0x00)
#define NE_PSTART (NE2000_IOBASE + 0x01)
#define NE_PSTOP  (NE2000_IOBASE + 0x02)
#define NE_BNRY   (NE2000_IOBASE + 0x03)
#define NE_TPSR   (NE2000_IOBASE + 0x04)
#define NE_TBCR0  (NE2000_IOBASE + 0x05)
#define NE_TBCR1  (NE2000_IOBASE + 0x06)
#define NE_ISR    (NE2000_IOBASE + 0x07)
#define NE_RSAR0  (NE2000_IOBASE + 0x08)
#define NE_RSAR1  (NE2000_IOBASE + 0x09)
#define NE_RBCR0  (NE2000_IOBASE + 0x0A)
#define NE_RBCR1  (NE2000_IOBASE + 0x0B)
#define NE_RCR    (NE2000_IOBASE + 0x0C)
#define NE_TCR    (NE2000_IOBASE + 0x0D)
#define NE_DCR    (NE2000_IOBASE + 0x0E)
#define NE_IMR    (NE2000_IOBASE + 0x0F)
#define NE_DATA   (NE2000_IOBASE + 0x10)
#define NE_RESET  (NE2000_IOBASE + 0x1F)

/* Page 1 registers (set CR bits to switch) */
#define NE_P1_CR   (NE2000_IOBASE + 0x00)
#define NE_P1_PAR0 (NE2000_IOBASE + 0x01)  /* physical address bytes 0-5 */
#define NE_P1_CURR (NE2000_IOBASE + 0x07)  /* current RX page pointer */

#define NE_TX_PAGE_START 0x40
#define NE_RX_PAGE_START 0x46
#define NE_RX_PAGE_STOP  0x80

#define NE_CR_STOP  0x01
#define NE_CR_START 0x02
#define NE_CR_TXP   0x04   /* transmit packet */
#define NE_CR_RD0   0x08   /* remote DMA command bits */
#define NE_CR_RD1   0x10
#define NE_CR_RD2   0x20   /* abort remote DMA */
#define NE_CR_PAGE0 0x00
#define NE_CR_PAGE1 0x40
#define NE_CR_PAGE2 0x80

static uint8_t ne_mac[6];
static uint8_t ne_rx_page_next;  /* next page to read from RX ring */

/* Remote DMA read: copy `len` bytes from NIC memory at `src_page:src_off` */
static void ne_remote_read(uint16_t addr, void *buf, uint16_t len) {
    outb(NE_CR,    NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_START);
    outb(NE_RBCR0, len & 0xFF);
    outb(NE_RBCR1, len >> 8);
    outb(NE_RSAR0, addr & 0xFF);
    outb(NE_RSAR1, addr >> 8);
    outb(NE_CR,    NE_CR_PAGE0 | NE_CR_RD0 | NE_CR_START);

    uint16_t *p = (uint16_t *)buf;
    for (uint16_t i = 0; i < (len + 1) / 2; i++)
        p[i] = inw(NE_DATA);
}

/* Remote DMA write: copy `len` bytes from `buf` to NIC memory at `addr` */
static void ne_remote_write(uint16_t addr, const void *buf, uint16_t len) {
    outb(NE_CR,    NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_START);
    outb(NE_RBCR0, len & 0xFF);
    outb(NE_RBCR1, len >> 8);
    outb(NE_RSAR0, addr & 0xFF);
    outb(NE_RSAR1, addr >> 8);
    outb(NE_CR,    NE_CR_PAGE0 | NE_CR_RD1 | NE_CR_START);

    const uint16_t *p = (const uint16_t *)buf;
    for (uint16_t i = 0; i < (len + 1) / 2; i++)
        outw(NE_DATA, p[i]);

    /* Wait for DMA to complete (ISR bit 6) */
    while (!(inb(NE_ISR) & 0x40));
    outb(NE_ISR, 0x40);
}

void ne2000_init(void) {
    /* Hardware reset */
    outb(NE_RESET, inb(NE_RESET));
    for (volatile int i = 0; i < 10000; i++);
    outb(NE_ISR, 0xFF);

    /* Page 0, stop NIC, word DMA */
    outb(NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_STOP);
    outb(NE_DCR, 0x49);   /* word-wide, FIFO threshold=8 bytes */
    outb(NE_RBCR0, 0);
    outb(NE_RBCR1, 0);
    outb(NE_RCR, 0x20);   /* monitor mode during init */
    outb(NE_TCR, 0x02);   /* loopback mode during init */

    /* Configure ring buffer */
    outb(NE_PSTART, NE_RX_PAGE_START);
    outb(NE_PSTOP,  NE_RX_PAGE_STOP);
    outb(NE_BNRY,   NE_RX_PAGE_START);
    outb(NE_TPSR,   NE_TX_PAGE_START);

    /* Clear all interrupt flags */
    outb(NE_ISR, 0xFF);
    outb(NE_IMR, 0x00);   /* mask all interrupts (polled mode) */

    /* Read MAC from PROM (first 6 words at NIC address 0) */
    uint8_t prom[12];
    ne_remote_read(0, prom, 12);
    for (int i = 0; i < 6; i++)
        ne_mac[i] = prom[i * 2];

    /* Switch to page 1 to write PAR (physical address) and CURR */
    outb(NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_STOP);
    for (int i = 0; i < 6; i++)
        outb(NE_P1_PAR0 + i, ne_mac[i]);
    ne_rx_page_next = NE_RX_PAGE_START + 1;
    outb(NE_P1_CURR, ne_rx_page_next);

    /* Page 0, start NIC, normal RX/TX mode */
    outb(NE_CR,  NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_START);
    outb(NE_TCR, 0x00);   /* normal TX mode */
    outb(NE_RCR, 0x04);   /* accept broadcast packets */

    vga_print("[NE2000] init OK, MAC: ");
    for (int i = 0; i < 6; i++) {
        /* print hex byte */
        static const char hex[] = "0123456789ABCDEF";
        char b[3] = { hex[ne_mac[i]>>4], hex[ne_mac[i]&0xF], (i<5)?':':'\n' };
        b[2] = 0;
        vga_print(b);
    }
}

void ne2000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) mac[i] = ne_mac[i];
}

int ne2000_send(const uint8_t *frame, uint16_t len) {
    if (len > ETH_FRAME_MAX) return -1;
    if (len < 60) len = 60;   /* minimum Ethernet frame size */

    /* Copy frame to NIC TX buffer */
    ne_remote_write((uint16_t)NE_TX_PAGE_START << 8, frame, len);

    /* Issue transmit */
    outb(NE_TBCR0, len & 0xFF);
    outb(NE_TBCR1, len >> 8);
    outb(NE_TPSR,  NE_TX_PAGE_START);
    outb(NE_CR,    NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_TXP | NE_CR_START);

    /* Poll for TX complete (ISR bit 1 = PTX or bit 3 = TXE) */
    uint8_t isr;
    do { isr = inb(NE_ISR); } while (!(isr & 0x0A));
    outb(NE_ISR, isr & 0x0A);

    return (isr & 0x02) ? 0 : -1;
}

/* RX packet header prepended by NE2000 ring buffer */
struct ne_rx_hdr {
    uint8_t  status;
    uint8_t  next_page;
    uint16_t len;   /* includes this 4-byte header */
} __attribute__((packed));

int ne2000_recv(uint8_t *buf, uint16_t buflen) {
    /* Check if a packet is available (CURR != BNRY+1) */
    outb(NE_CR, NE_CR_PAGE1 | NE_CR_RD2 | NE_CR_START);
    uint8_t curr = inb(NE_P1_CURR);
    outb(NE_CR, NE_CR_PAGE0 | NE_CR_RD2 | NE_CR_START);
    uint8_t bnry = inb(NE_BNRY);
    uint8_t read_page = bnry + 1;
    if (read_page >= NE_RX_PAGE_STOP) read_page = NE_RX_PAGE_START;
    if (read_page == curr) return 0;  /* no packet */

    /* Read packet header from ring */
    struct ne_rx_hdr hdr;
    ne_remote_read((uint16_t)read_page << 8, &hdr, sizeof(hdr));

    uint16_t frame_len = hdr.len - sizeof(hdr);
    if (frame_len > buflen) frame_len = buflen;

    /* Read packet data (may wrap around ring end) */
    uint16_t data_addr = ((uint16_t)read_page << 8) + sizeof(hdr);
    ne_remote_read(data_addr, buf, frame_len);

    /* Advance boundary */
    uint8_t new_bnry = hdr.next_page - 1;
    if (new_bnry < NE_RX_PAGE_START) new_bnry = NE_RX_PAGE_STOP - 1;
    outb(NE_BNRY, new_bnry);

    return frame_len;
}
