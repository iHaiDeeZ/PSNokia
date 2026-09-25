# PSNokia

PSNokia runs J2ME (MIDP) phone games — the Java games of 2000s Nokia and
other feature phones — natively on a jailbroken **PS4**. It is a port of
**phoneME** (Sun's open-source CLDC/MIDP runtime) built with the
[OpenOrbis PS4 toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain).
Each game is packaged as its own PS4 app.

It grew out of [VitoKia](https://github.com/iHaiDeeZ/VitoKia), the PS Vita
port of the same code, which is built on
[j2me-preservation/phoneME-GP2X-SDL](https://github.com/j2me-preservation/phoneME-GP2X-SDL)
(Sun's `phoneme_feature` `mr2-rel-b23` plus a GP2X/SDL port). The Vita
README is kept as [README-VITA.md](README-VITA.md).

## Status

Tested on a real PS4 with GoldHEN:

| Game | State |
|---|---|
| Sonic Advance (Gameloft) | Plays through, with music |
| Tower Bloxx, City Bloxx (Digital Chocolate / Nokia) | Play, including the 3D (M3G) buildings, with music |
| Bounce (Nokia) | Plays, at its original 128x128 screen scaled up |
| Rayman 3 (Gameloft) | Plays, with its Nokia ringtone music and sounds |

## What the port does

- **A 64-bit VM.** phoneME's CLDC-HI VM only supports 32-bit machines and
  the PS4 runs only 64-bit programs. The VM keeps its 32-bit heap layout;
  object references are stored as 4-byte "narrow" pointers
  (`narrow<T>` in `GlobalDefinitions.hpp`) and all VM memory is kept below
  2GB by a dedicated allocator (`ps4/common/lowheap.c`, on dlmalloc).
- **A PS4 platform layer** for the VM (`cldc/src/vm/os/ps4`), PCSL and MIDP:
  SDL2 video scaled to the TV, DS4 input, file access (with a fix for the
  OpenOrbis headers' `struct stat` layout), timers and threads.
- **Sound:** WAV (including IMA ADPCM) and tones, plus a software
  synthesizer for MIDI music
  (`midp/src/media_vita/reference/native/midi_synth.c`), which most games
  use. Nokia ringtones (`com.nokia.mid.sound`) are converted to MIDI.
- **Vibration** on the DS4's rumble motors.
- **Nokia UI API** (`com.nokia.mid.ui`) and **JSR 184 (M3G)** 3D, which
  many commercial games need.
- **Per-game settings:** save data in `/data/psnokia/<TITLE_ID>`, and the
  phone screen size the game was made for.

## Controls

| DS4 | Phone key |
|---|---|
| Cross | Select / fire (centre key) |
| Circle | Right soft key: Back / Exit |
| Square, Options | Left soft key: OK / Options / Menu |
| D-pad | 2 / 4 / 6 / 8 (up, left, right, down) |
| Left stick | Arrow keys |
| Triangle | 0 |
| L2 / R2 / L3 / R3 | 1 / 3 / 7 / 9 |
| Touchpad | * |
| Hold L1 or R1 | Cross = 5, Square = *, Circle = #, Triangle = Clear |
| L1 + Options | Display shape: fit, 4:3, full (16:9), pixel |
| L1 + Touchpad | Display filter: sharp or smooth (Scale2x) |

As on Nokia phones, the soft key labels are drawn in the bottom corners, and
Circle usually leaves a screen — on a game's main menu it quits.

The display settings are saved for each game. **fit** keeps the phone's
proportions, **full** stretches to the whole screen, and **pixel** uses the
largest whole-number scale, so every phone pixel is the same size.

## Building

Everything builds on Windows from an MSYS2 shell.

Requirements:

- [OpenOrbis PS4 toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
  (tested with v0.5.4) and LLVM/clang 18
- MSYS2 with the 32-bit MinGW gcc (`mingw-w64-i686-gcc`), used for the
  VM's host-side ROM generator
- JDK 6 (e.g. Zulu 6) for CLDC and JDK 8 for MIDP
- The .NET runtime, for the toolchain's `PkgTool.Core`
- Optional: Python with Pillow, to make each game's icon from its own

Set the tool locations if they differ from the defaults in
[ps4/env.sh](ps4/env.sh) (`OO_PS4_TOOLCHAIN`, `PS4_LLVM`, `JDK6_DIR`,
`JDK8_DIR`), then:

```bash
ps4/build_all.sh
```

This builds PCSL, the VM and MIDP into `ps4/out`. Then package a game:

```bash
ps4/package/make_game.sh path/to/game.jar "Game Title" PSNK00001
```

The title ID (4 letters and 5 digits) must be different for each game. A
fourth argument sets the phone screen size, for games made for something
other than a 240x320 portrait screen, for example `128x128` for early Nokia
games. The package is written to `ps4/out/games/<title-id>/`; install it
with GoldHEN's package installer.

No games are included. Use games you own.

## Logs and debugging

The app writes `/data/psnokia/renderlog.txt` (the VM and MIDP output,
exceptions with Java stack traces, crashes, FPS) and a few screenshots as
`/data/psnokia/frameN.bmp`; fetch them with GoldHEN's FTP
server. The log is also sent over UDP (port 18194) to the address in
`PSN_LOG_PC_IP` (see `ps4/common/psn_log.h`) and as a LAN broadcast;
`ps4/tools/logrecv.py` receives it.

Per-game overrides, as files in `/data/psnokia/<TITLE_ID>/` or
`/data/psnokia/`:

- `screen.txt` — the phone screen size, e.g. `176x208`
- `landscape` or `portrait` (empty files) — force the orientation
- `display.txt` — the display shape and filter, e.g. `full smooth` (written
  by the controller shortcuts)

## Known limitations

- MIDP is built in debug mode, which is slower than a release build would
  be; there is no JIT.
- The MIDI synthesizer approximates General MIDI with simple waveforms.
- The Vita build shares this source tree but has not been rebuilt since the
  64-bit changes.

## License

phoneME is licensed under the GNU General Public License version 2, as
stated at the top of its source files, and so are the changes in this
repository.
dlmalloc (`ps4/common/dlmalloc`) is by Doug Lea under an MIT-style license.
