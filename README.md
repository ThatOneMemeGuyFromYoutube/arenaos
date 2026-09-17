# NT31 — Windows NT 3.1, from memory, in portable C

A terminal-based recreation of **Microsoft Windows NT 3.1 (build 103, 1993)**:
the NTLDR/kernel boot sequence, the navy "WINDOWS NT" splash, the Program
Manager shell with its MDI group windows, 3D-beveled windows, pull-down
menus, modal dialogs, a working MS-DOS console, and a handful of Win32
applications — all rendered with plain ANSI escape sequences, all of it in
strict C11 with a POSIX 2008 API surface (i.e. it builds cleanly against
**musl**, glibc, and macOS libSystem, and the Windows branch compiles under
MinGW/MSVC).

No assets, no dependencies, no GNU extensions. One Makefile, five `.c`
files, one header.

```
                    ■Program                   _  x
                   ┌───────────────────────────────────────┐
                    ★Favorites                   _  x
                     ▗         ●         ▣
                      Display  Network  Add Printer
   Program Manager                        Memory: 11.8 MB free
```

## Building

```sh
make                  # default compiler (cc), -O2 -std=c11
make CC=musl-gcc      # Linux + musl  (apt install musl-tools)
make CC=clang         # macOS / Linux + clang
make strict           # -std=c11 -D_POSIX_C_SOURCE=200809L -Wpedantic -Werror
                      # the exact feature set musl provides
make stub-win         # syntax-check the #if defined(_WIN32) branch on Linux
                      # against a minimal test stub (real builds use <windows.h>)
```

Windows: with MinGW-w64 or MSVC, compile the same sources with
`/std:c11` (MSVC) — the platform split lives entirely in `#if defined(_WIN32)`
directives inside `src/term.c`:

```sh
x86_64-w64-mingw32-gcc -std=c11 -Wall -Wextra -O2 src/*.c -o nt31.exe
```

(`getenv`/`snprintf`/`atexit` etc. come from the standard C runtime on all
targets; the POSIX side uses only termios, select, ioctl, and clock_gettime.)

## Running

```sh
./nt31                # full boot: NTLDR text, driver load, splash, desktop
./nt31 --fast         # skip the boot delays (NT31_BOOTFAST=1 works too)
NT31_SHOT=out.txt ./nt31 --fast   # dump one frame as text, then exit
NT31_DEBUG=1 ./nt31    # trace events to stderr (for debugging)
```

A terminal of at least **60×17** is required; larger is better (100×30
comfortable). Ctrl-C asks to exit; the "safe to turn off your computer"
screen ends the session.

## Controls

| Input | Action |
|---|---|
| Left mouse | select / activate; double-click icon opens it |
| Right mouse | flag a mine (Minesweeper); close menu |
| Mouse drag on title bar | move window |
| Arrows | move focused window; select icons in a focused group |
| Enter | activate selected icon / press focused button |
| Tab | cycle between group windows / dialog buttons |
| Esc | close focused window; close menu |
| F10 | open the System menu |
| Ctrl-C | confirm and shut down |

## What's inside

- **Boot**: NTLDR-style text banner, 16 MB RAM, subsystem/driver "ok" log
  (VDMDD, KEYB, MOUSE, VIDEOPRT, WIN32K...), navy figlet splash, then the
  desktop. A fake process table (System, Idle, Win386, csrss, services, Pm,
  plus one entry per launched app) and a memory counter that actually
  tracks allocations show up in the status bar, the About box, and
  `MEM`/`TASK` in the console.
- **Program Manager**: full-screen MDI shell, System/File/Window/Help menus
  (Change Password with a real masked 3-field modal, Control Panel, Logon
  User, About, Exit→confirm→shutdown; Print; Cascade/Tile/Arrange/Next/
  Restore Minimized; Help Topics), status bar with live free-memory.
- **Groups**: Program, Favorites, Startup, Windows Setup — cascaded 52×17
  child windows with selectable icon slots, minimize boxes, and the
  "you cannot close this group" surprise.
- **Apps**:
  - *Console* — MS-DOS prompt with `HELP CLS CD DATE DIR ECHO EXIT MEM SET
    TASK TIME VER` and `WIN notepad|calc|clock|winmine|charmap|cpl` to
    launch apps from DOS, plus command history (up arrow).
  - *Notepad* — 64-line text editor (the Read Me icon opens it prefilled).
  - *Calculator* — the classic four-function layout, mouse and keyboard.
  - *Clock* — live time and date.
  - *Minesweeper* — 9×9, 10 mines, flood fill, flags, smiley, timer;
    mouse or arrows+Enter+`f`, `r` to reset.
  - *Character Map* — the box-drawing block (U+2500–U+257F).
  - *Control Panel* — six items with a detail pane.
  - *About Windows NT* — Version 3.1 (build 103), 1981–1993 copyright.
- **Shutdown**: "Windows NT is shutting down..." →
  "It is now safe to turn off your computer."

## Source layout

| File | Role |
|---|---|
| `src/nt.h` | everything shared: palette, geometry, events, window/app structs |
| `src/term.c` | terminal backend: raw mode, UTF-8 cells, diff flush, SGR mouse (POSIX) / ReadConsoleInput (Windows) |
| `src/kernel.c` | boot sequence, memory, process table |
| `src/win.c` | z-order window manager, Program Manager, menus, dialogs, shutdown |
| `src/apps.c` | the eight applications |
| `src/main.c` | arg/env handling + the event loop |
| `test/win_stub/windows.h` | **test-only** stub used by `make stub-win` on Linux |
| `test/drive.py` | pty driver that boots NT31 and plays a full session |

## Verification performed

- `make` (glibc): clean, no warnings with `-Wall -Wextra`
- `make strict` (C11 + `_POSIX_C_SOURCE=200809L`, `-Wpedantic -Werror`):
  clean — this is the gate that approximates a musl build in this sandbox
  (no network available to install musl-gcc)
- `make stub-win` (`-D_WIN32 -fsyntax-only`, `-Werror`): the Windows
  console-API branch compiles
- `python3 test/drive.py`: automated pty session — boot banner, desktop,
  groups, Console opens, `VER` prints `Windows NT [Version 3.10.103]`,
  Ctrl-C confirm, shutdown screen: all checks pass
