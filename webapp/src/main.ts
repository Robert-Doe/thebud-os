import './style.css';
import {
  AccessBits,
  FlagBits,
  KERNEL_CODE_ACCESS,
  encodeAccessByte,
  encodeFlagsNibble,
  encodeGdtEntry,
  hex,
  bin,
} from './gdt';
import { VgaBuffer, VGA_PALETTE, VGA_COLOR_NAMES, vgaColorByte, VGA_COLS, VGA_ROWS } from './vga';
import { renderModuleLibrary } from './library/render';
import { renderRunItYourself } from './run-guide';

const app = document.getElementById('app')!;

// ---------------------------------------------------------------------------
// Shell: topbar, hero, view tabs, footer
// ---------------------------------------------------------------------------

type View = 'boot' | 'library' | 'run';
let activeView: View = 'boot';

const VIEW_TABS: { id: View; label: string }[] = [
  { id: 'boot', label: 'Boot Simulator' },
  { id: 'library', label: 'Module Library' },
  { id: 'run', label: 'Run It Yourself' },
];

app.innerHTML = `
  <div class="topbar">
    <div class="brand">theBud OS</div>
    <div class="links">
      <a href="https://github.com/Robert-Doe/thebud-os" target="_blank" rel="noopener">GitHub</a>
      <a href="https://robertdoe.com">&larr; robertdoe.com</a>
    </div>
  </div>
  <div class="hero">
    <h1>theBud OS &mdash; Interactive Course</h1>
    <p class="tagline">
      An x86 operating system built by hand, from the bootloader to a
      POSIX-flavored kernel, plus a browser built on top of it. Step through
      the boot sequence, browse the full 46-module course library, or set up
      the toolchain and boot it yourself.
    </p>
  </div>
  <div class="view-tabs" id="view-tabs">
    ${VIEW_TABS.map((t) => `<button class="view-tab ${t.id === activeView ? 'active' : ''}" data-view="${t.id}">${t.label}</button>`).join('')}
  </div>
  <main id="view-main"></main>
  <footer>
    Ported from <a href="https://github.com/Robert-Doe/thebud-os" target="_blank" rel="noopener">Robert-Doe/thebud-os</a>
    &mdash; real addresses, real structs, real bytes.
  </footer>
`;

const viewMain = document.getElementById('view-main')!;

function renderView(): void {
  document.querySelectorAll<HTMLButtonElement>('.view-tab').forEach((btn) => {
    btn.classList.toggle('active', btn.dataset.view === activeView);
  });

  if (activeView === 'boot') {
    viewMain.innerHTML = `
      <div class="demo-grid">
        <nav class="stepper" id="stepper"></nav>
        <div id="stage-content"></div>
      </div>
    `;
    renderAll();
  } else if (activeView === 'library') {
    viewMain.innerHTML = `<div class="lib-root" id="lib-root"></div>`;
    void renderModuleLibrary(document.getElementById('lib-root')!);
  } else {
    viewMain.innerHTML = `<div id="run-root"></div>`;
    renderRunItYourself(document.getElementById('run-root')!);
  }
}

document.getElementById('view-tabs')!.addEventListener('click', (e) => {
  const btn = (e.target as HTMLElement).closest<HTMLButtonElement>('.view-tab');
  if (!btn) return;
  activeView = btn.dataset.view as View;
  renderView();
  window.scrollTo({ top: 0, behavior: 'smooth' });
});

// ---------------------------------------------------------------------------
// Stage definitions
// ---------------------------------------------------------------------------

interface Stage {
  title: string;
  render: () => string;
  mount?: () => void;
}

const stages: Stage[] = [
  {
    title: 'BIOS POST & handoff',
    render: () => `
      <div class="card">
        <h2>0. BIOS reads the boot sector</h2>
        <p class="desc">
          BIOS performs Power-On Self Test, then loads sector 1 (512 bytes)
          of the boot drive into RAM at physical address <code>0x7C00</code>,
          and jumps to it &mdash; but only if the last two bytes of that
          sector are the boot signature <code>0x55 0xAA</code>.
        </p>
        <div class="terminal">
<span class="dim">BIOS POST...</span>
<span class="dim">Checking boot devices...</span>
<span class="dim">Sector 1 bytes[510..511] = </span><span class="hi">55 AA</span> <span class="dim">-- valid boot signature</span>
<span class="dim">Loading 512 bytes to</span> <span class="addr">0x0000:0x7C00</span>
<span class="dim">CPU jumps to</span> <span class="addr">0x7C00</span><span class="dim">, real mode, CS=0000</span>
        </div>
        <p class="desc" style="margin-top:12px">
          This is real, from <code>boot.asm</code>:
          <code>times 510 - ($ - $$) db 0</code> pads the sector, then
          <code>dw 0xAA55</code> writes the signature little-endian
          (stored as bytes <code>55 AA</code>).
        </p>
      </div>
    `,
  },
  {
    title: 'Bootloader @ 0x7C00',
    render: () => `
      <div class="card">
        <h2>1. Stage 1 bootloader takes over</h2>
        <p class="desc">
          Real mode, 16-bit. The bootloader zeroes its segment registers and
          points the stack at the same address it was loaded &mdash; the
          stack grows <em>down</em>, so <code>0x7C00</code> is a safe
          top-of-stack right below the loaded code.
        </p>
        <div class="terminal">
<span class="dim">[BITS 16]  [ORG</span> <span class="addr">0x7C00</span><span class="dim">]</span>

<span class="hi">xor ax, ax</span>
<span class="hi">mov ds, ax</span>      <span class="dim">; Data Segment  = 0x0000</span>
<span class="hi">mov es, ax</span>      <span class="dim">; Extra Segment = 0x0000</span>
<span class="hi">mov ss, ax</span>      <span class="dim">; Stack Segment = 0x0000</span>
<span class="hi">mov sp,</span> <span class="addr">0x7C00</span>  <span class="dim">; SP = top of stack (grows down)</span>

<span class="dim">real-mode address = segment*16 + offset</span>
<span class="dim">-&gt; DS:0 = 0x0000*16 + 0 =</span> <span class="addr">0x00000</span>
        </div>
        <p class="desc" style="margin-top:12px">
          It then prints <code>"BobOS Booting..."</code> via BIOS
          <code>INT 0x10</code>, function <code>AH=0x0E</code> (teletype
          output) &mdash; one character per interrupt call.
        </p>
      </div>
    `,
  },
  {
    title: 'Load kernel from disk',
    render: () => `
      <div class="card">
        <h2>2. Reading the kernel off disk</h2>
        <p class="desc">
          The updated bootloader (Component 2) uses BIOS
          <code>INT 0x13</code>, function <code>AH=0x02</code>
          ("read sectors") to pull the kernel into RAM at
          <code>0x1000</code> &mdash; chosen because it sits safely above the
          real-mode Interrupt Vector Table and BIOS Data Area.
        </p>
        <table class="byte-table">
          <tr><th>Register</th><th>Value</th><th>Meaning</th></tr>
          <tr><td>AH</td><td>0x02</td><td>function: read sectors</td></tr>
          <tr><td>AL</td><td>15</td><td>sectors to read (7680 bytes)</td></tr>
          <tr><td>CH</td><td>0</td><td>cylinder 0</td></tr>
          <tr><td>CL</td><td>2</td><td>start at sector 2 (sector 1 = bootloader)</td></tr>
          <tr><td>DH</td><td>0</td><td>head 0</td></tr>
          <tr><td>DL</td><td>0x00</td><td>drive (floppy A)</td></tr>
          <tr><td>ES:BX</td><td>0000:1000</td><td>destination = physical 0x1000</td></tr>
        </table>
        <p class="desc">
          BIOS sets the Carry Flag on failure &mdash; the real code checks
          it with <code>jc disk_error</code> immediately after
          <code>int 0x13</code>.
        </p>
      </div>
    `,
  },
  {
    title: 'Build & load the GDT',
    render: () => `
      <div class="card">
        <h2>3. The Global Descriptor Table</h2>
        <p class="desc">
          Before switching to protected mode, the CPU needs a GDT to
          consult on every memory access. Every entry is exactly 8 bytes,
          <code>__attribute__((packed))</code> so the compiler never adds
          padding. Toggle the bits below &mdash; this recomputes the real
          access byte and the full 8-byte struct exactly the way
          <code>gdt_set_entry()</code> in <code>gdt.c</code> does.
        </p>
        <div id="gdt-explorer"></div>
      </div>
    `,
    mount: mountGdtExplorer,
  },
  {
    title: 'Switch to Protected Mode',
    render: () => `
      <div class="card">
        <h2>4. Flipping CR0.PE</h2>
        <p class="desc">
          Protected mode is one bit: bit 0 (PE, "Protection Enable") of
          control register <code>CR0</code>. It can't be written directly,
          so the value is round-tripped through a general-purpose register.
        </p>
        <div class="terminal">
<span class="hi">mov eax, cr0</span>
<span class="hi">or  eax,</span> <span class="addr">0x1</span>       <span class="dim">; set PE bit</span>
<span class="hi">mov cr0, eax</span>

<span class="dim">CR0 before:</span> <span class="addr">...0</span>  <span class="dim">(bit0 = 0, real mode)</span>
<span class="dim">CR0 after: </span> <span class="addr">...1</span>  <span class="dim">(bit0 = 1, protected mode ENABLED)</span>

<span class="dim">; the CPU has prefetched real-mode instructions --</span>
<span class="dim">; a FAR JUMP flushes the pipeline and reloads CS:</span>
<span class="hi">jmp</span> <span class="addr">0x08</span><span class="hi">:init_protected_mode</span>
<span class="dim">          ^^^^ GDT selector for the kernel code segment</span>
        </div>
        <p class="desc" style="margin-top:12px">
          <code>0x08</code> is not arbitrary: selector = (GDT index) &times; 8.
          Index 1 (the code segment, entry after the mandatory null
          descriptor) &times; 8 = <code>0x08</code>.
        </p>
      </div>
    `,
  },
  {
    title: 'Install the IDT',
    render: () => `
      <div class="card">
        <h2>5. The Interrupt Descriptor Table</h2>
        <p class="desc">
          Same pattern as the GDT: a table the CPU consults, this time for
          interrupts and CPU exceptions. Up to 256 entries ("gates"), each
          8 bytes, loaded with <code>lidt</code> instead of <code>lgdt</code>.
        </p>
        <div id="idt-explorer"></div>
      </div>
    `,
    mount: mountIdtExplorer,
  },
  {
    title: 'Kernel entry & VGA output',
    render: () => `
      <div class="card">
        <h2>6. Live VGA text-mode buffer</h2>
        <p class="desc">
          Real hardware fact: the VGA text framebuffer is memory-mapped at
          physical address <code>0xB8000</code>, an 80&times;25 grid where
          each cell is exactly 2 bytes &mdash; a character byte and an
          attribute byte (<code>background&lt;&lt;4 | foreground</code>,
          4 bits each, 16 CGA colors). This grid is a real
          <code>Uint16Array(80*25)</code> laid out the same way. Click a
          cell, type, and pick colors &mdash; the byte readout below is
          computed straight from the buffer, not staged.
        </p>
        <div id="vga-app"></div>
      </div>
    `,
    mount: mountVga,
  },
];

let activeStage = 0;

function renderStepper(): void {
  const stepperEl = document.getElementById('stepper');
  if (!stepperEl) return; // boot view not mounted (a different tab is active)
  stepperEl.innerHTML = stages
    .map(
      (s, i) => `
      <button class="step-btn ${i === activeStage ? 'active' : ''}" data-idx="${i}">
        <span class="num">${String(i).padStart(2, '0')}</span>
        <span>${s.title}</span>
      </button>`
    )
    .join('');

  stepperEl.querySelectorAll<HTMLButtonElement>('.step-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      activeStage = Number(btn.dataset.idx);
      renderAll();
    });
  });
}

function renderStage(): void {
  const stageEl = document.getElementById('stage-content');
  if (!stageEl) return; // boot view not mounted (a different tab is active)
  const stage = stages[activeStage];
  stageEl.innerHTML = stage.render();
  stageEl.insertAdjacentHTML(
    'beforeend',
    `<div class="btn-row">
      <button class="btn secondary" id="prev-btn" ${activeStage === 0 ? 'disabled' : ''}>&larr; Previous</button>
      <button class="btn" id="next-btn" ${activeStage === stages.length - 1 ? 'disabled' : ''}>Next stage &rarr;</button>
    </div>`
  );
  document.getElementById('prev-btn')?.addEventListener('click', () => {
    if (activeStage > 0) { activeStage--; renderAll(); }
  });
  document.getElementById('next-btn')?.addEventListener('click', () => {
    if (activeStage < stages.length - 1) { activeStage++; renderAll(); }
  });
  stage.mount?.();
}

function renderAll(): void {
  renderStepper();
  renderStage();
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

// ---------------------------------------------------------------------------
// GDT explorer
// ---------------------------------------------------------------------------

function mountGdtExplorer(): void {
  const container = document.getElementById('gdt-explorer')!;

  const access: AccessBits = { ...KERNEL_CODE_ACCESS };
  const flags: FlagBits = { granularity4k: true, size32: true, longMode: false };
  let base = 0x00000000;
  let limit = 0x000fffff;

  const bitDefs: { key: keyof AccessBits; label: string; kind: 'bool' | 'dpl' }[] = [
    { key: 'present', label: 'P (bit7)', kind: 'bool' },
    { key: 'dpl', label: 'DPL (6-5)', kind: 'dpl' },
    { key: 'descType', label: 'S (bit4)', kind: 'bool' },
    { key: 'executable', label: 'E (bit3)', kind: 'bool' },
    { key: 'dc', label: 'DC (bit2)', kind: 'bool' },
    { key: 'rw', label: 'RW (bit1)', kind: 'bool' },
    { key: 'accessed', label: 'A (bit0)', kind: 'bool' },
  ];

  function draw() {
    const accessByte = encodeAccessByte(access);
    const flagsNibble = encodeFlagsNibble(flags);
    const bytes = encodeGdtEntry({ base, limit, access, flags });

    container.innerHTML = `
      <div class="bitfield">
        ${bitDefs
          .map((d) => {
            if (d.kind === 'dpl') {
              return `<div class="bit" data-key="dpl" title="Descriptor Privilege Level, ring 0-3">
                <span class="label">${d.label}</span>
                <span class="val">${access.dpl}</span>
              </div>`;
            }
            const on = access[d.key] as boolean;
            return `<div class="bit ${on ? 'on' : ''}" data-key="${d.key}">
              <span class="label">${d.label}</span>
              <span class="val">${on ? '1' : '0'}</span>
            </div>`;
          })
          .join('')}
      </div>
      <div class="bitfield">
        <div class="bit ${flags.granularity4k ? 'on' : ''}" data-flag="granularity4k">
          <span class="label">G (4K gran.)</span><span class="val">${flags.granularity4k ? '1' : '0'}</span>
        </div>
        <div class="bit ${flags.size32 ? 'on' : ''}" data-flag="size32">
          <span class="label">DB (32-bit)</span><span class="val">${flags.size32 ? '1' : '0'}</span>
        </div>
        <div class="bit ${flags.longMode ? 'on' : ''}" data-flag="longMode">
          <span class="label">L (64-bit)</span><span class="val">${flags.longMode ? '1' : '0'}</span>
        </div>
      </div>

      <table class="byte-table">
        <tr><th>Field</th><th>Value</th><th>Binary</th></tr>
        <tr><td>access byte</td><td>${hex(accessByte)}</td><td>${bin(accessByte)}</td></tr>
        <tr><td>flags nibble</td><td>${hex(flagsNibble, 1)}</td><td>${bin(flagsNibble, 4)}</td></tr>
        <tr><td>base</td><td>${hex(base, 8)}</td><td>-</td></tr>
        <tr><td>limit</td><td>${hex(limit, 5)}</td><td>-</td></tr>
      </table>

      <p class="desc" style="margin:10px 0 4px">
        Raw 8-byte struct, in memory order (exactly what <code>lgdt</code> reads):
      </p>
      <div class="terminal">${bytes
        .map((b, i) => `<span class="${i === 5 ? 'hi' : i === 6 ? 'addr' : 'dim'}">${b.toString(16).toUpperCase().padStart(2, '0')}</span>`)
        .join(' ')}
<span class="dim">limit_lo(2) base_lo(2) base_mid(1) </span><span class="hi">access(1)</span><span class="dim"> </span><span class="addr">flags_limit(1)</span><span class="dim"> base_hi(1)</span>
      </div>

      <div class="btn-row">
        <button class="btn secondary" id="gdt-preset-code">Load: kernel code (0x9A)</button>
        <button class="btn secondary" id="gdt-preset-data">Load: kernel data (0x92)</button>
      </div>
    `;

    container.querySelectorAll<HTMLDivElement>('.bit[data-key]').forEach((el) => {
      el.addEventListener('click', () => {
        const key = el.dataset.key as keyof AccessBits;
        if (key === 'dpl') {
          access.dpl = ((access.dpl + 1) % 4) as 0 | 1 | 2 | 3;
        } else {
          (access[key] as boolean) = !(access[key] as boolean);
        }
        draw();
      });
    });

    container.querySelectorAll<HTMLDivElement>('.bit[data-flag]').forEach((el) => {
      el.addEventListener('click', () => {
        const key = el.dataset.flag as keyof FlagBits;
        flags[key] = !flags[key];
        draw();
      });
    });

    document.getElementById('gdt-preset-code')?.addEventListener('click', () => {
      Object.assign(access, {
        present: true, dpl: 0, descType: true, executable: true, dc: false, rw: true, accessed: false,
      });
      draw();
    });
    document.getElementById('gdt-preset-data')?.addEventListener('click', () => {
      Object.assign(access, {
        present: true, dpl: 0, descType: true, executable: false, dc: false, rw: true, accessed: false,
      });
      draw();
    });
  }

  draw();
}

// ---------------------------------------------------------------------------
// IDT explorer (read-mostly, one interactive gate-type toggle)
// ---------------------------------------------------------------------------

function mountIdtExplorer(): void {
  const container = document.getElementById('idt-explorer')!;
  let vector = 33; // IRQ1 (keyboard) after PIC remap, a concrete real vector
  let gateType: 0x8e | 0x8f = 0x8e;
  const handlerAddr = 0x00108f30; // illustrative ISR stub address

  function draw() {
    const selector = 0x08; // GDT_SEL_CODE
    const offset_low = handlerAddr & 0xffff;
    const offset_high = (handlerAddr >>> 16) & 0xffff;

    container.innerHTML = `
      <p class="desc">
        Pick a vector and gate type. <code>idt_set_gate()</code> splits the
        32-bit handler address into two 16-bit halves around a selector and
        a type/attribute byte &mdash; the same packed-struct trick as the GDT.
      </p>
      <div class="bitfield">
        <label style="display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--text-dim)">
          Vector (0-255)
          <input id="idt-vector" type="number" min="0" max="255" value="${vector}"
            style="background:var(--bg-elevated);border:1px solid var(--border);color:var(--text);border-radius:6px;padding:6px 8px;font-family:var(--mono);width:90px" />
        </label>
        <div class="bit ${gateType === 0x8e ? 'on' : ''}" data-gate="0x8e">
          <span class="label">Interrupt Gate</span><span class="val">0x8E</span>
        </div>
        <div class="bit ${gateType === 0x8f ? 'on' : ''}" data-gate="0x8f">
          <span class="label">Trap Gate</span><span class="val">0x8F</span>
        </div>
      </div>
      <table class="byte-table">
        <tr><th>Field</th><th>Value</th></tr>
        <tr><td>offset_low</td><td>${hex(offset_low, 4)}</td></tr>
        <tr><td>selector</td><td>${hex(selector, 4)} <span style="color:var(--text-dim)">(GDT_SEL_CODE)</span></td></tr>
        <tr><td>zero</td><td>0x00</td></tr>
        <tr><td>type_attr</td><td>${hex(gateType)} <span style="color:var(--text-dim)">${gateType === 0x8e ? '(P=1, DPL=0, 32-bit interrupt gate -- clears IF, no nested IRQs)' : '(P=1, DPL=0, 32-bit trap gate -- IF unchanged)'}</span></td></tr>
        <tr><td>offset_high</td><td>${hex(offset_high, 4)}</td></tr>
      </table>
      <p class="desc">
        This fills <code>idt[${vector}]</code> &mdash;
        ${vector === 32 ? 'IRQ0, the PIT timer, remapped after PIC init.' :
          vector === 33 ? 'IRQ1, the keyboard controller, remapped after PIC init.' :
          vector < 32 ? 'a CPU exception vector (e.g. divide error, page fault range).' :
          'a general interrupt vector.'}
      </p>
    `;

    container.querySelector<HTMLInputElement>('#idt-vector')?.addEventListener('input', (e) => {
      vector = Math.max(0, Math.min(255, Number((e.target as HTMLInputElement).value) || 0));
      draw();
    });
    container.querySelectorAll<HTMLDivElement>('.bit[data-gate]').forEach((el) => {
      el.addEventListener('click', () => {
        gateType = el.dataset.gate === '0x8e' ? 0x8e : 0x8f;
        draw();
      });
    });
  }

  draw();
}

// ---------------------------------------------------------------------------
// VGA text-mode buffer emulator
// ---------------------------------------------------------------------------

let vgaKeyHandler: ((e: KeyboardEvent) => void) | null = null;

function mountVga(): void {
  if (vgaKeyHandler) {
    window.removeEventListener('keydown', vgaKeyHandler);
    vgaKeyHandler = null;
  }

  const container = document.getElementById('vga-app')!;
  const vga = new VgaBuffer();
  let fg = 0xf; // white
  let bg = 0x0; // black

  container.innerHTML = `
    <div class="vga-toolbar">
      <label>Foreground
        <div class="color-swatches" id="fg-swatches"></div>
      </label>
      <label>Background
        <div class="color-swatches" id="bg-swatches"></div>
      </label>
      <button class="btn secondary" id="vga-clear">vga_clear()</button>
    </div>
    <div class="vga-screen-wrap">
      <div id="vga-screen"></div>
    </div>
    <div class="vga-readout" id="vga-readout"></div>
  `;

  const screenEl = document.getElementById('vga-screen')!;
  const readoutEl = document.getElementById('vga-readout')!;
  const fgSwatches = document.getElementById('fg-swatches')!;
  const bgSwatches = document.getElementById('bg-swatches')!;

  function drawSwatches() {
    fgSwatches.innerHTML = VGA_PALETTE.map((c, i) =>
      `<div class="swatch ${i === fg ? 'selected' : ''}" data-fg="${i}" style="background:${c}" title="${VGA_COLOR_NAMES[i]}"></div>`
    ).join('');
    bgSwatches.innerHTML = VGA_PALETTE.map((c, i) =>
      `<div class="swatch ${i === bg ? 'selected' : ''}" data-bg="${i}" style="background:${c}" title="${VGA_COLOR_NAMES[i]}"></div>`
    ).join('');
    fgSwatches.querySelectorAll<HTMLDivElement>('[data-fg]').forEach((el) => {
      el.addEventListener('click', () => { fg = Number(el.dataset.fg); vga.color = vgaColorByte(bg, fg); drawSwatches(); });
    });
    bgSwatches.querySelectorAll<HTMLDivElement>('[data-bg]').forEach((el) => {
      el.addEventListener('click', () => { bg = Number(el.dataset.bg); vga.color = vgaColorByte(bg, fg); drawSwatches(); });
    });
  }

  function drawScreen() {
    let html = '';
    for (let row = 0; row < VGA_ROWS; row++) {
      for (let col = 0; col < VGA_COLS; col++) {
        const { char, attr } = vga.getCell(row, col);
        const fgIdx = attr & 0xf;
        const bgIdx = (attr >>> 4) & 0xf;
        const ch = char === 0x20 || char === 0 ? '&nbsp;' : escapeHtml(String.fromCharCode(char));
        const isCursor = row === vga.cursorRow && col === vga.cursorCol;
        html += `<span class="vga-cell${isCursor ? ' cursor' : ''}" data-row="${row}" data-col="${col}" style="color:${VGA_PALETTE[fgIdx]};background:${VGA_PALETTE[bgIdx]}">${ch}</span>`;
      }
    }
    screenEl.innerHTML = html;
    screenEl.querySelectorAll<HTMLSpanElement>('.vga-cell').forEach((el) => {
      el.addEventListener('click', () => {
        vga.cursorRow = Number(el.dataset.row);
        vga.cursorCol = Number(el.dataset.col);
        drawScreen();
        drawReadout();
      });
    });
  }

  function drawReadout() {
    const { char, attr } = vga.getCell(vga.cursorRow, vga.cursorCol);
    const word = (attr << 8) | char;
    const addr = vga.physicalAddress(vga.cursorRow, vga.cursorCol);
    readoutEl.innerHTML = `
      <p class="desc" style="margin-bottom:8px">Cursor at row ${vga.cursorRow}, col ${vga.cursorCol} &mdash; click any cell to move it, then type.</p>
      <div class="byte-chip"><span class="k">physical address</span><span class="v">${hex(addr, 5)}</span></div>
      <div class="byte-chip"><span class="k">char byte</span><span class="v">${hex(char)} '${char >= 32 && char < 127 ? String.fromCharCode(char) : '·'}'</span></div>
      <div class="byte-chip"><span class="k">attribute byte</span><span class="v">${hex(attr)}</span></div>
      <div class="byte-chip"><span class="k">16-bit word</span><span class="v">${hex(word, 4)}</span></div>
      <div class="byte-chip"><span class="k">bg (bits 7-4)</span><span class="v">${(attr >>> 4) & 0xf} ${VGA_COLOR_NAMES[(attr >>> 4) & 0xf]}</span></div>
      <div class="byte-chip"><span class="k">fg (bits 3-0)</span><span class="v">${attr & 0xf} ${VGA_COLOR_NAMES[attr & 0xf]}</span></div>
    `;
  }

  vgaKeyHandler = (e: KeyboardEvent) => {
    if (!document.getElementById('vga-screen')) return; // stage no longer mounted
    if (e.key === 'Backspace') {
      e.preventDefault();
      if (vga.cursorCol > 0) vga.cursorCol--;
      vga.writeCell(vga.cursorRow, vga.cursorCol, 0x20, vga.color);
    } else if (e.key === 'Enter') {
      vga.putChar('\n');
    } else if (e.key === 'Tab') {
      e.preventDefault();
      vga.putChar('\t');
    } else if (e.key.length === 1) {
      vga.putChar(e.key);
    } else {
      return;
    }
    drawScreen();
    drawReadout();
  };
  window.addEventListener('keydown', vgaKeyHandler);

  document.getElementById('vga-clear')?.addEventListener('click', () => {
    vga.clear();
    drawScreen();
    drawReadout();
  });

  drawSwatches();
  vga.color = vgaColorByte(bg, fg);
  vga.putString('BobOS kernel_main() reached.\nVGA driver online.\n');
  drawScreen();
  drawReadout();
}

function escapeHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// Deferred until every view's render function (renderAll, the Module
// Library, the Run guide) is defined above, calling this any earlier
// would hit `stages` and friends before their `const`/`function`
// initializers have run.
renderView();
