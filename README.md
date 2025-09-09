# cfetch - A Lightweight System Information Fetcher with ASCII Art

## Features
- Auto-detection and display of:
  - Host (motherboard)
  - Shell + version
  - CPU
  - GPU
  - RAM (used/total)
  - OS (ID from /etc/*-release)
  - WM/DE (via XDG_CURRENT_DESKTOP, DESKTOP_SESSION, GDMSESSION)
- Distribution-specific ASCII selection with fallback to Tux
- 24-bit color palette blocks
- Export ASCII art from a text file to a C array

## Requirements
- Linux with /proc and /sys available
- pciutils (lspci) and grep for GPU detection
- A truecolor-capable terminal (24-bit)
- Read access to /sys/class/dmi/id/* for Host information (may be restricted on some systems)

## Build
- With Makefile:
  make
  sudo make install    (# optional, installs to /usr/local/bin by default)
- Without Makefile:
  gcc main.c -o cfetch
  gcc -O2 -Wall -Wextra -std=c11 main.c -o cfetch
- Ensure ascii.c is in the same directory as main.c (it is included via `#include "ascii.c"`).

## Usage
- Auto-detect distro and print appropriate ASCII:
  ./cfetch

- Force a specific ASCII:
  ./cfetch --arch
  ./cfetch --arch-classic
  ./cfetch --arch-alt
  ./cfetch --fedora
  ./cfetch --gentoo
  ./cfetch --debian
  ./cfetch --mint
  ./cfetch --slackware
  ./cfetch --redhat
  ./cfetch --tux
  ./cfetch --apple
  ./cfetch --apple-mini
  ./cfetch --custom
  ./cfetch --dota
  ./cfetch --nixos

- Export ASCII from a text file to a C array:
  ./cfetch --ExportAscii path/to/art.txt > ascii_export.c
  (ascii_export.c will contain `const char *exported_ascii_art[]`)

## Configuration
- Path: ~/.config/cfetch/config
- Create the config if it does not exist:
  mkdir -p ~/.config/cfetch
  touch ~/.config/cfetch/config
- The program reads configuration from this file at startup.

## Contributing
- Pull requests and issues are welcome.
- Please keep the Linux kernel code style (tabs = 8 spaces).

## Authors
- Zer0Flux86 — https://github.com/Zer0Flux86
- BoLIIIoi — https://github.com/BoLIIIoi

