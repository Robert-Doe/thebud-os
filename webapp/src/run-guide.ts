/**
 * "Run It Yourself" — a from-scratch explanation of the real toolchain
 * (QEMU, NASM, a freestanding cross-compiler) needed to actually build and
 * boot any module in this repo, written for someone who has never set any
 * of this up before. Static content + a Windows/macOS/Linux platform
 * switcher + copy buttons on every command, no server involved.
 */

type Platform = 'windows' | 'mac' | 'linux';
let platform: Platform = 'windows';

interface Cmd { label?: string; cmd: string }

function cmdBlock(c: Cmd): string {
  return `
    <div class="run-cmd">
      ${c.label ? `<div class="run-cmd-label">${c.label}</div>` : ''}
      <div class="run-cmd-row">
        <code>${escHtml(c.cmd)}</code>
        <button class="run-copy-btn" data-cmd="${escAttr(c.cmd)}">Copy</button>
      </div>
    </div>`;
}

function escHtml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
function escAttr(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/"/g, '&quot;');
}

const WINDOWS_STEPS: { title: string; body: string; cmds?: Cmd[] }[] = [
  {
    title: '1. Install QEMU (the machine emulator)',
    body: `Get the official installer from qemu.org, or install from the command line. Either way, make sure its install folder (typically <code>C:\\Program Files\\qemu</code>) ends up on your <code>PATH</code> so <code>qemu-system-i386</code> works from any terminal.`,
    cmds: [{ cmd: 'winget install --id SoftwareFreedomConservancy.QEMU -e' }],
  },
  {
    title: '2. Install NASM (the assembler)',
    body: `NASM turns the <code>.asm</code> files (the bootloader, the low-level ISR stubs) into raw machine bytes. Get it from nasm.us, or:`,
    cmds: [{ cmd: 'winget install --id NASM.NASM -e' }],
  },
  {
    title: '3. Install WSL + Ubuntu (the C compiler)',
    body: `Here's the part that trips people up: Windows doesn't ship a compiler that produces <em>freestanding</em>, 32-bit, flat binary code the way this kernel needs (no C runtime, no OS underneath it to call into). Rather than fight MinGW's defaults, this course compiles every <code>.c</code> file through Ubuntu's real <code>gcc</code>/<code>ld</code> running inside WSL (Windows Subsystem for Linux) &mdash; a genuine Linux kernel running alongside Windows, not a VM you have to babysit.`,
    cmds: [
      { cmd: 'wsl --install -d Ubuntu', label: 'One-time install (reboot if prompted, then set a Linux username/password on first launch)' },
      { cmd: 'sudo apt update && sudo apt install -y build-essential', label: 'Inside the new Ubuntu terminal — installs gcc, ld, make' },
    ],
  },
  {
    title: '4. Install GNU Make for Windows',
    body: `The Makefiles are written to run on <em>native</em> Windows <code>make</code> (they fall back to Unix commands only if the Windows one isn\u2019t found) and shell out to WSL only for the compile/link steps. A stock Windows install usually has no <code>make</code> at all &mdash; add one:`,
    cmds: [
      { cmd: 'winget install --id GnuWin32.Make -e', label: 'Or: choco install make' },
    ],
  },
  {
    title: '5. Verify everything is on PATH',
    body: `Open a fresh terminal (so PATH changes take effect) and confirm all four tools answer:`,
    cmds: [
      { cmd: 'qemu-system-i386 --version' },
      { cmd: 'nasm -v' },
      { cmd: 'make --version' },
      { cmd: 'wsl -d Ubuntu -- gcc --version' },
    ],
  },
];

const MAC_STEPS: { title: string; body: string; cmds?: Cmd[] }[] = [
  {
    title: '1. Install QEMU and NASM',
    body: `Both are one Homebrew line:`,
    cmds: [{ cmd: 'brew install qemu nasm' }],
  },
  {
    title: '2. Install a freestanding cross-compiler',
    body: `macOS's own <code>clang</code> defaults to producing Mach-O binaries for macOS itself, not flat 32-bit freestanding code. The standard fix in OS-dev circles is a dedicated <code>i686-elf</code> cross-compiler:`,
    cmds: [{ cmd: 'brew install i686-elf-gcc i686-elf-binutils' }],
  },
  {
    title: '3. Point the Makefile at your local compiler',
    body: `These Makefiles were authored assuming Windows+WSL, so each one prefixes the compiler with a <code>wsl -d Ubuntu --</code> call. On macOS/Linux, open a module's <code>Makefile</code> and change three lines near the top to skip that prefix, using your cross-compiler in place of plain <code>gcc</code>/<code>ld</code> if the freestanding flags need it:`,
    cmds: [
      { cmd: 'WSL_GCC = i686-elf-gcc' },
      { cmd: 'WSL_LD  = i686-elf-ld' },
      { cmd: 'WSL_DIR := $(CURDIR)' },
    ],
  },
  {
    title: '4. Verify',
    body: `Confirm each tool answers:`,
    cmds: [
      { cmd: 'qemu-system-i386 --version' },
      { cmd: 'nasm -v' },
      { cmd: 'i686-elf-gcc --version' },
      { cmd: 'make --version' },
    ],
  },
];

const LINUX_STEPS: { title: string; body: string; cmds?: Cmd[] }[] = [
  {
    title: '1. Install everything from your package manager',
    body: `Debian/Ubuntu:`,
    cmds: [{ cmd: 'sudo apt update && sudo apt install -y qemu-system-x86 nasm build-essential' }],
  },
  {
    title: '1b. Fedora',
    body: `If you're on Fedora/RHEL instead:`,
    cmds: [{ cmd: 'sudo dnf install -y qemu-system-x86 nasm gcc binutils make' }],
  },
  {
    title: '2. Point the Makefile at your local compiler',
    body: `Same as macOS &mdash; these Makefiles assume Windows+WSL by default. Open a module's <code>Makefile</code> and change three lines near the top so it calls your system tools directly instead of shelling out through WSL:`,
    cmds: [
      { cmd: 'WSL_GCC = gcc' },
      { cmd: 'WSL_LD  = ld' },
      { cmd: 'WSL_DIR := $(CURDIR)' },
    ],
  },
  {
    title: '3. Verify',
    body: `Confirm each tool answers:`,
    cmds: [
      { cmd: 'qemu-system-i386 --version' },
      { cmd: 'nasm -v' },
      { cmd: 'gcc --version' },
      { cmd: 'make --version' },
    ],
  },
];

function stepsFor(p: Platform) {
  return p === 'windows' ? WINDOWS_STEPS : p === 'mac' ? MAC_STEPS : LINUX_STEPS;
}

export function renderRunItYourself(container: HTMLElement): void {
  container.innerHTML = `
    <div class="run-guide">
      <div class="card">
        <h2>What you're actually installing</h2>
        <p class="desc">
          Three tools, each doing one job. <strong>NASM</strong> assembles the
          hand-written <code>.asm</code> files (the bootloader, the raw ISR
          stubs) straight into machine bytes &mdash; no linker needed for the
          bootloader, since it's a flat 512-byte binary. A <strong>freestanding
          C compiler + linker</strong> (<code>gcc</code>/<code>ld</code>, run
          with <code>-ffreestanding -m32</code> and a custom linker script)
          turns the kernel's <code>.c</code> files into one flat kernel binary
          with no C runtime, no libc, no OS underneath it &mdash; because this
          <em>is</em> the OS. <strong>QEMU</strong> is a machine emulator: it
          pretends to be a real x86 PC, with a real BIOS, a real floppy/disk
          controller, and a real screen &mdash; so handing it <code>os.img</code>
          is exactly like inserting a floppy disk into a physical computer and
          powering it on. Nothing here is a simulation of the mechanics; QEMU
          runs the actual bytes this course produces.
        </p>
      </div>

      <div class="card">
        <h2>Why WSL / a cross-compiler at all?</h2>
        <p class="desc">
          Your everyday <code>gcc</code> or <code>clang</code> is configured
          to build programs that run <em>under</em> your OS (Windows/macOS),
          linked against its C runtime and expecting its loader to set up a
          stack, environment variables, and more before <code>main()</code>
          ever runs. A kernel gets none of that &mdash; it <em>is</em> the
          first code running on bare metal. <code>-ffreestanding</code> tells
          the compiler not to assume a hosted environment exists, and a custom
          <code>linker.ld</code> places every function at exact addresses this
          course's bootloader jumps to. Windows' native toolchains target a
          different binary format (PE, not ELF) by default, so this course
          reaches for a real Linux <code>gcc</code>/<code>ld</code> (via WSL)
          to get genuine ELF/flat-binary freestanding output without fighting
          MinGW's assumptions.
        </p>
      </div>

      <div class="run-platform-tabs" id="run-platform-tabs">
        <button class="run-platform-tab ${platform === 'windows' ? 'active' : ''}" data-p="windows">Windows</button>
        <button class="run-platform-tab ${platform === 'mac' ? 'active' : ''}" data-p="mac">macOS</button>
        <button class="run-platform-tab ${platform === 'linux' ? 'active' : ''}" data-p="linux">Linux</button>
      </div>
      <div id="run-steps"></div>

      <div class="card">
        <h2>Building and booting any module</h2>
        <p class="desc">Every module directory is a complete, independent kernel. Pick one, <code>cd</code> into it, and:</p>
        ${cmdBlock({ cmd: 'cd 13_shell' })}
        ${cmdBlock({ cmd: 'make' })}
        ${cmdBlock({ cmd: 'make run' })}
        <p class="desc" style="margin-top:14px">
          <code>make</code> assembles/compiles/links everything into
          <code>os.img</code> (and, from Module 12 onward, a second
          <code>disk.img</code> for the filesystem). <code>make run</code>
          boots it in QEMU with the right flags for that module &mdash;
          <code>-fda os.img</code> alone for the early modules, plus
          <code>-hda disk.img</code> once a module has a filesystem. Check
          that module's own <code>tutorial.html</code> (in the Module Library
          tab above) if you want the exact reasoning for that module's flags.
        </p>
        <div class="callout">
          <p class="desc" style="margin:0">
            The Makefiles originally hardcoded one specific machine's WSL path
            and would fail with a "no such file or directory" for anyone else
            who clones this repo. That's now fixed &mdash; every Makefile
            computes its own WSL path automatically from wherever you actually
            checked the repo out (<code>wslpath -a "$(CURDIR)")</code>), so
            <code>make</code> just works regardless of where the folder lives.
          </p>
        </div>
      </div>

      <div class="card">
        <h2>Common gotchas</h2>
        <dl class="qa">
          <dt>QEMU opens a black window and does nothing</dt>
          <dd>Click inside the QEMU window first &mdash; some builds start with keyboard focus elsewhere. If it stays black past a few seconds, the boot signature (bytes 510&ndash;511 of the image) probably isn't <code>0x55 0xAA</code>; re-run <code>make clean && make</code>.</dd>
          <dt>"No such file or directory" from WSL during a build</dt>
          <dd>Make sure Ubuntu has actually started at least once (<code>wsl -d Ubuntu</code>) and that <code>build-essential</code> is installed inside it &mdash; a fresh WSL install has no compiler until you apt-install one.</dd>
          <dt>A later module (12+) boots but can't find its filesystem</dt>
          <dd>It needs <em>both</em> images: <code>qemu-system-i386 -fda os.img -hda disk.img</code>. <code>make run</code> already does this for you, but a manual QEMU invocation is a common place to forget the second flag.</dd>
          <dt>Do I need to redo this setup for every module?</dt>
          <dd>No &mdash; install the four tools once. Every module folder reuses the same NASM/QEMU/WSL toolchain; only the source files inside each folder change.</dd>
        </dl>
      </div>
    </div>
  `;

  wireCopyButtons(container);
  renderSteps(container);

  container.querySelector('#run-platform-tabs')!.addEventListener('click', (e) => {
    const btn = (e.target as HTMLElement).closest<HTMLButtonElement>('.run-platform-tab');
    if (!btn) return;
    platform = btn.dataset.p as Platform;
    container.querySelectorAll('.run-platform-tab').forEach((b) => b.classList.toggle('active', (b as HTMLElement).dataset.p === platform));
    renderSteps(container);
  });
}

function renderSteps(container: HTMLElement): void {
  const el = container.querySelector<HTMLElement>('#run-steps')!;
  el.innerHTML = stepsFor(platform)
    .map(
      (s) => `
      <div class="card">
        <h3 style="margin-top:0">${s.title}</h3>
        <p class="desc">${s.body}</p>
        ${(s.cmds ?? []).map(cmdBlock).join('')}
      </div>`
    )
    .join('');
  wireCopyButtons(el);
}

function wireCopyButtons(root: ParentNode): void {
  root.querySelectorAll<HTMLButtonElement>('.run-copy-btn').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const text = btn.dataset.cmd ?? '';
      try {
        await navigator.clipboard.writeText(text);
        btn.textContent = 'Copied!';
      } catch {
        btn.textContent = 'Select + Ctrl/Cmd+C';
      }
      setTimeout(() => { btn.textContent = 'Copy'; }, 1500);
    });
  });
}
