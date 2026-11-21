# Building

Built with devkitPro and the Switch 2 devkit (libounce, OunceVK), on Windows (the devkitPro MSYS2 shell) or on Linux.

## Requirements

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `switch-dev` group
- the [ounce-devkit](https://github.com/ouncebrew/ounce-devkit), installed into the same devkitPro (run its `install.sh`)
- `switch-sdl2` installed with `pacman -S`
- `python3` and `make`
- `DEVKITPRO` set (the default is `/opt/devkitpro`)

## Steps

```sh
bash tools/build.sh             # builds, checks and writes sdbench.nro
python3 tools/package.py        # the SD-card zip in dist/
```

`tools/build.sh` runs two checks on the result:

- `tools/verify_elf.sh` refuses a program that was not linked against the Switch 2 runtime or that carries the Switch 1 graphics libraries
- `tools/verify_nro.py` checks the NRO header and, with the link map, that no Switch 1 library went into the link

A program built against the Switch 1 libnx can make the console warn that it may crash the system, so do not skip these checks.

## Layout

| Path | What |
|---|---|
| `source/main.c` | the whole program: the tests, the screen and the controls |
| `source/stb_truetype.h` | font rendering |
| `data/font.bin` | the Noto Sans font, linked into the program |
| `icon.jpg` | the icon of the program |
| `docs/screenshot.jpg` | the screenshot at the top of the README |
| `tools/` | build, check and package scripts |
