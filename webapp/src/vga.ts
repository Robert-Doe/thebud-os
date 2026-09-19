/**
 * vga.ts — a byte-accurate port of thebud-os/03_vga_driver/vga.c + vga.h.
 *
 * Real hardware fact being modeled: the VGA text-mode framebuffer lives at
 * physical address 0xB8000. It is a flat array of 80*25 = 2000 CELLS, each
 * cell exactly 2 BYTES: [character byte][attribute byte]. The attribute
 * byte packs background (bits 7-4) and foreground (bits 3-0) as 4-bit
 * CGA color indices:
 *
 *   #define VGA_COLOR(bg, fg) (((unsigned char)(bg) << 4) | (unsigned char)(fg))
 *
 * This module stores the buffer as a real Uint16Array of length COLS*ROWS,
 * one 16-bit word per cell — low byte = char code, high byte = attribute —
 * exactly mirroring how the CPU/video hardware sees memory at 0xB8000.
 */

export const VGA_ADDRESS = 0xb8000;
export const VGA_COLS = 80;
export const VGA_ROWS = 25;

export const VGA_COLOR_NAMES = [
  'Black', 'Blue', 'Green', 'Cyan', 'Red', 'Magenta', 'Brown', 'Light Grey',
  'Dark Grey', 'Light Blue', 'Light Green', 'Light Cyan', 'Light Red',
  'Light Magenta', 'Yellow', 'White',
];

// Faithful 16-color CGA/VGA text-mode palette (standard values used by real
// VGA hardware and every text-mode terminal emulator).
export const VGA_PALETTE = [
  '#000000', '#0000AA', '#00AA00', '#00AAAA',
  '#AA0000', '#AA00AA', '#AA5500', '#AAAAAA',
  '#555555', '#5555FF', '#55FF55', '#55FFFF',
  '#FF5555', '#FF55FF', '#FFFF55', '#FFFFFF',
];

export function vgaColorByte(bg: number, fg: number): number {
  return ((bg & 0xf) << 4) | (fg & 0xf);
}

export class VgaBuffer {
  // One 16-bit word per cell, exactly like the real 0xB8000 framebuffer:
  // bits 0-7 = ASCII character, bits 8-15 = attribute byte.
  readonly buf = new Uint16Array(VGA_COLS * VGA_ROWS);
  cursorRow = 0;
  cursorCol = 0;
  color = vgaColorByte(0x0, 0xf); // black bg, white fg — vga_init() default

  constructor() {
    this.clear();
  }

  private idx(row: number, col: number): number {
    return row * VGA_COLS + col;
  }

  clear(): void {
    const blank = (this.color << 8) | 0x20; // space char
    this.buf.fill(blank);
    this.cursorRow = 0;
    this.cursorCol = 0;
  }

  /** Mirrors buf_write() in vga.c: writes char+attribute at (row, col). */
  writeCell(row: number, col: number, ch: number, color: number): void {
    this.buf[this.idx(row, col)] = ((color & 0xff) << 8) | (ch & 0xff);
  }

  getCell(row: number, col: number): { char: number; attr: number } {
    const word = this.buf[this.idx(row, col)];
    return { char: word & 0xff, attr: (word >>> 8) & 0xff };
  }

  private scroll(): void {
    for (let row = 0; row < VGA_ROWS - 1; row++) {
      for (let col = 0; col < VGA_COLS; col++) {
        this.buf[this.idx(row, col)] = this.buf[this.idx(row + 1, col)];
      }
    }
    const blank = (this.color << 8) | 0x20;
    for (let col = 0; col < VGA_COLS; col++) {
      this.buf[this.idx(VGA_ROWS - 1, col)] = blank;
    }
  }

  private advanceCursor(): void {
    this.cursorCol++;
    if (this.cursorCol >= VGA_COLS) {
      this.cursorCol = 0;
      this.cursorRow++;
    }
    if (this.cursorRow >= VGA_ROWS) {
      this.scroll();
      this.cursorRow = VGA_ROWS - 1;
    }
  }

  /** Mirrors vga_putchar() in vga.c, including \n \r \t handling. */
  putChar(c: string): void {
    if (c === '\n') {
      this.cursorCol = 0;
      this.cursorRow++;
      if (this.cursorRow >= VGA_ROWS) {
        this.scroll();
        this.cursorRow = VGA_ROWS - 1;
      }
      return;
    }
    if (c === '\r') {
      this.cursorCol = 0;
      return;
    }
    if (c === '\t') {
      do {
        this.writeCell(this.cursorRow, this.cursorCol, 0x20, this.color);
        this.advanceCursor();
      } while (this.cursorCol % 8 !== 0);
      return;
    }
    this.writeCell(this.cursorRow, this.cursorCol, c.charCodeAt(0) & 0xff, this.color);
    this.advanceCursor();
  }

  putString(s: string): void {
    for (const c of s) this.putChar(c);
  }

  /** Physical byte address of a given cell, exactly like real VGA memory. */
  physicalAddress(row: number, col: number): number {
    return VGA_ADDRESS + this.idx(row, col) * 2;
  }
}
