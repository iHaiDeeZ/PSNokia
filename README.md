# VitoKia

A native port of **phoneME** (Sun/Oracle's CLDC/MIDP Java ME runtime) to the
**PS Vita**, cross-compiled with [vitasdk](https://vitasdk.org) and tested
primarily under the [Vita3K](https://vita3k.org) emulator, with real-hardware
testing in progress.

This lets original J2ME/MIDP `.jar` MIDlets — the games and apps that used to
run on 2000s-era feature phones — run natively on the Vita.

Built on top of [j2me-preservation/phoneME-GP2X-SDL](https://github.com/j2me-preservation/phoneME-GP2X-SDL),
itself a GP2X-SDL fork of Sun's original phoneME (`phoneme_feature`
`mr2-rel-b23`). All of Sun/Oracle's and Intel's original copyright notices
are preserved throughout the source tree.

## What's implemented

- Native SDL2-based input (D-pad, analog stick, face/shoulder buttons,
  touch), following pspkvm's digit/navigation mapping convention.
- Full 2D LCDUI rendering pipeline (Chameleon skin, softkeys, Canvas/GameCanvas).
- A `com.nokia.mid.ui` (Nokia UI API) stub subsystem — `DirectGraphics`,
  `DirectUtils`, `FullCanvas` — for the many S40-originated commercial MIDlets
  that depend on it.
- A from-scratch JSR184 (M3G / Mobile 3D Graphics) engine: a binary `.m3g`
  scene loader plus a software triangle rasterizer (edge-function barycentric
  fill, Z-buffer, nearest-neighbor texture sampling) rendering directly into
  the shared 2D framebuffer. Work in progress — see Known issues.
- A real VM timer tick, wired up so `Thread.yield()`-paced MIDlet loops keep
  polling native input (this port's original OS layer never called it).
- CLDC's native `Math.sin/cos/tan` replaced with a pure-Java implementation
  system-wide — the original native trig routines returned garbage for
  ordinary angles.

## Known issues

- JSR184 (M3G) rendering pipeline is confirmed writing real pixels, but at
  least one tested game's 3D content doesn't fully appear on screen yet —
  under active investigation.
- A build that runs cleanly under the Vita3K emulator has been reported not
  to run on real Vita hardware; root cause not yet identified (Vita3K's HLE
  is known to be more forgiving than real firmware about things like
  unresolved imports and enforced memory budgets).
- No JIT — the interpreter-only `-int` flag is currently always passed;
  removing it produced no measurable speedup in testing so far, but the
  question of whether the JIT is actually being invoked at all was never
  fully answered.

## Building

Requires:
- [vitasdk](https://vitasdk.org) (`arm-vita-eabi-*` toolchain)
- A JDK 6-compatible `javac` for CLDC's old stub classes (e.g. Zulu 6)
- JDK 8 for MIDP's own sources
- MSYS2/MinGW for the build (phoneME's Makefiles expect a POSIX toolchain)

High level:
```bash
# CLDC
cd phoneme_feature/cldc/build/vita_arm
make PCSL_OUTPUT_DIR=<path>

# MIDP (after CLDC)
cd phoneme_feature/midp/build/vita_arm
make USE_DEBUG=true
```

Package the resulting `runMidlet_g` into a bootable VPK with vitasdk's
`vita-elf-create`, `vita-make-fself`, and `vita-pack-vpk`.

## License

GPL v2 (inherited from phoneME — see the license header at the top of any
source file in this tree). Portions Copyright 2000-2007 Sun Microsystems,
Inc. / Oracle, and Intel Corporation.
