# theBud OS — Boot Sequence Simulator (web demo)

An interactive, byte-accurate port of theBud OS's real boot path into
TypeScript: the same memory addresses, GDT struct layout, IDT gate layout,
and VGA text-mode framebuffer format used by the actual bootloader/kernel
modules in this repo (`01_bootloader` through `05_idt_interrupts`) —
re-implemented so a visitor can step through and manipulate them directly
in the browser, not just read about them.

- Stepper walks BIOS handoff → bootloader @ `0x7C00` → disk load → GDT →
  protected-mode switch → IDT → kernel entry / VGA driver.
- The GDT explorer toggles the real access-byte and flags-nibble bits from
  `gdt.c`/`gdt.h` and recomputes the packed 8-byte descriptor live.
- The VGA panel is a real `Uint16Array(80*25)` backing an 80×25 grid, one
  16-bit word per cell (char byte + attribute byte), exactly mirroring the
  real `0xB8000` framebuffer format from `vga.c`/`vga.h`. Type into it, pick
  foreground/background from the 16-color CGA palette, and read the live
  byte values for the cell under the cursor.

## Local development

```bash
cd webapp
npm install
npm run dev
```

## Build

```bash
npm run build
```

Output goes to `webapp/dist/`.

## Static hosting

Deploy on Vercel, Netlify, or Cloudflare Pages with:

- Root directory: `webapp`
- Build command: `npm run build`
- Output directory: `dist`

No backend, no server-side code — it's a fully static site.
