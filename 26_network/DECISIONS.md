# Module 26 — Network Stack: Design Decisions

## 1. NE2000 over virtio — familiarity and documentation win

**Decision:** Use the NE2000 ISA NIC emulated by QEMU (`-nic user,model=ne2k_isa`), not virtio-net.

**Why:** The NE2000's DP8390 register map has been documented in dozens of OS textbooks since 1993. Every register has a name, a purpose, and published errata. virtio requires understanding virtqueues, shared memory descriptors, and a notification protocol — correct and fast, but opaque for a first network driver. The NE2000 teaches you exactly what a NIC driver does: DMA read/write from a ring buffer, polling for TX complete, checking ISR for RX availability.

**Trade-off:** The NE2000 is 1990s ISA hardware. In a real modern OS you'd write virtio-net or an Intel e1000 driver. The NE2000 ring buffer mechanics (TX pages 0x40–0x45, RX pages 0x46–0x7F) are identical in concept to how virtio ring buffers work — the abstraction transfers.

## 2. Polled I/O, not interrupt-driven receive

**Decision:** `ne2000_recv()` checks the ring buffer boundary pointer directly. `net_poll()` is called from a kernel thread rather than from the IRQ handler.

**Why:** Interrupt-driven receive requires the NIC IRQ to be wired into the IDT, a handler to save state, and careful interaction with the scheduler to wake the right blocked process. Polled I/O separates the concern: understand the NIC ring buffer first, add interrupt wake-up later. The scheduler's kernel thread calls `net_poll()` every tick — slightly wasteful but totally correct.

**Trade-off:** Polling burns CPU time even when no packets arrive. A production driver uses IRQ to wake the receive thread and polls only when the IRQ fires (NAPI in Linux). Module 27's driver framework is the right place to add that.

## 3. No ARP — broadcast MAC for all outgoing frames

**Decision:** All outgoing IP frames use destination MAC `FF:FF:FF:FF:FF:FF` (Ethernet broadcast). No ARP table, no ARP request/reply.

**Why:** ARP requires maintaining a MAC cache, handling async replies, and queuing packets while waiting for an ARP response. That machinery is separate from the core IP/UDP stack and distracts from the lesson. QEMU's user-mode network stack responds to broadcast frames correctly — packets still arrive at the QEMU-internal gateway (`10.0.0.2`).

**Trade-off:** Broadcast flooding wastes Ethernet bandwidth and would fail on a real switched network (switches filter broadcasts by VLAN). A real stack caches learned MACs from ARP replies and uses directed frames. The `arp.c` file is the natural next step.

## 4. UDP only — no TCP

**Decision:** Only UDP (IP protocol 17) is implemented. TCP is not.

**Why:** TCP requires connection state machines (SYN/SYN-ACK/ACK), sequence numbers, retransmission timers, congestion control, and half-close semantics. That is an entire project on its own. UDP is stateless: send a datagram, receive a datagram. It demonstrates all the interesting IP/Ethernet framing mechanics without state machine complexity.

**Trade-off:** No HTTP, no SSH, no TLS over this stack. For a kernel demo — echo server over UDP — that is fine.

## 5. One rx buffer per socket — oldest packet overwritten on arrival

**Decision:** Each `udp_socket` holds one `udp_rx_entry`. When a new packet arrives for a bound port, it overwrites the existing entry regardless of whether it has been read.

**Why:** A proper receive queue requires a circular buffer (or linked list) with producer/consumer synchronization. At this stage, the lesson is "how do frames arrive at a socket," not "how do you build a concurrent bounded queue." The single-slot design makes the data flow obvious: NIC → ip_input → udp_input → socket.rx.valid=1 → sys_recvfrom.

**Trade-off:** Under packet bursts, all but the latest packet are dropped silently. For a ping-pong UDP echo demo this is never observable.

## 6. Syscalls 21–23 added for socket, sendto, recvfrom

**Decision:** Three new syscalls map directly to udp_bind, udp_send, udp_recvfrom. No generic socket() / connect() / bind() abstraction yet.

**Why:** BSD sockets are a multi-protocol abstraction with address families, socket types, and protocol numbers — correct for a production OS, heavy for a first network syscall. The thin wrappers make the mapping from user-space call to kernel function explicit and easy to trace.

**Trade-off:** Module 28 (POSIX compliance) is where these get wrapped in a BSD-compatible socket interface. For now, use them directly.
