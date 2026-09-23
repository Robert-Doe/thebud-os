/**
 * gdt.ts, a byte-accurate port of thebud-os/04_gdt/gdt.c + gdt.h.
 *
 * Real struct being modeled (gdt.h):
 *
 *   struct gdt_entry {
 *     uint16_t limit_low;    // Limit  bits  0-15
 *     uint16_t base_low;     // Base   bits  0-15
 *     uint8_t  base_mid;     // Base   bits 16-23
 *     uint8_t  access;       // Access byte
 *     uint8_t  flags_limit;  // Flags[7:4] + Limit bits 16-19 [3:0]
 *     uint8_t  base_high;    // Base   bits 24-31
 *   } __attribute__((packed));                     // 8 bytes total
 *
 * Access byte bits (gdt.h):
 *   7   Present
 *   6-5 DPL (privilege ring: 00 = ring0, 11 = ring3)
 *   4   Descriptor type (1 = code/data)
 *   3   Executable (1 = code, 0 = data)
 *   2   DC (direction/conforming)
 *   1   RW (readable for code / writable for data)
 *   0   Accessed (CPU-managed)
 *
 * Flags nibble (packed into the high nibble of flags_limit):
 *   3   Granularity (1 = limit is in 4KB pages)
 *   2   Size (1 = 32-bit protected mode)
 *   1   Long mode (0 for us)
 *   0   Reserved (0)
 */

export interface AccessBits {
  present: boolean;
  dpl: 0 | 1 | 2 | 3;
  descType: boolean; // 1 = code/data, 0 = system
  executable: boolean;
  dc: boolean;
  rw: boolean;
  accessed: boolean;
}

export interface FlagBits {
  granularity4k: boolean;
  size32: boolean;
  longMode: boolean;
}

export interface GdtEntryInput {
  base: number; // 32-bit
  limit: number; // 20-bit
  access: AccessBits;
  flags: FlagBits;
}

export function encodeAccessByte(a: AccessBits): number {
  let b = 0;
  if (a.present) b |= 0x80;
  b |= (a.dpl & 0x3) << 5;
  if (a.descType) b |= 0x10;
  if (a.executable) b |= 0x08;
  if (a.dc) b |= 0x04;
  if (a.rw) b |= 0x02;
  if (a.accessed) b |= 0x01;
  return b;
}

export function encodeFlagsNibble(f: FlagBits): number {
  let n = 0;
  if (f.granularity4k) n |= 0x8;
  if (f.size32) n |= 0x4;
  if (f.longMode) n |= 0x2;
  return n;
}

/** Mirrors gdt_set_entry() in gdt.c exactly: produces the 8 raw bytes. */
export function encodeGdtEntry(input: GdtEntryInput): number[] {
  const { base, limit } = input;
  const access = encodeAccessByte(input.access);
  const flags = encodeFlagsNibble(input.flags);

  const limit_low = limit & 0xffff;
  const base_low = base & 0xffff;
  const base_mid = (base >>> 16) & 0xff;
  const base_high = (base >>> 24) & 0xff;
  const flags_limit = ((flags & 0x0f) << 4) | ((limit >>> 16) & 0x0f);

  return [
    limit_low & 0xff,
    (limit_low >>> 8) & 0xff,
    base_low & 0xff,
    (base_low >>> 8) & 0xff,
    base_mid,
    access,
    flags_limit,
    base_high,
  ];
}

export function hex(n: number, width = 2): string {
  return '0x' + n.toString(16).toUpperCase().padStart(width, '0');
}

export function bin(n: number, width = 8): string {
  return n.toString(2).padStart(width, '0');
}

/** The two real, concrete entries thebud-os builds in gdt_init(). */
export const KERNEL_CODE_ACCESS: AccessBits = {
  present: true,
  dpl: 0,
  descType: true,
  executable: true,
  dc: false,
  rw: true,
  accessed: false,
};

export const KERNEL_DATA_ACCESS: AccessBits = {
  present: true,
  dpl: 0,
  descType: true,
  executable: false,
  dc: false,
  rw: true,
  accessed: false,
};

export const FLAT_FLAGS: FlagBits = {
  granularity4k: true,
  size32: true,
  longMode: false,
};
