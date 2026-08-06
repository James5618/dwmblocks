

## Modifying blocks

Blocks are added and removed in [config.h](config.h); each block runs a
script from
[`~/.local/bin/statusbar`](https://github.com/james5618/dotfiles/tree/master/.local/bin/statusbar)
in my dotfiles, so install those if you want the bar working out of the box.

## Signaling changes

Each module has a signal number, so it can be updated on demand instead of
polling: `pkill -RTMIN+<num> dwmblocks`, or the faster
`kill -<SIGRTMIN+num> $(pgrep -x dwmblocks)`. The volume module never polls at
all — dwm's volume keybindings signal it directly. All modules must have
distinct signal numbers.

Note that `SIGRTMIN` is 34 on Linux but **65 on FreeBSD**, so a hardcoded
`kill -35` from elsewhere means a different module here; the dotfiles use
`pkill -RTMIN+<num>`, which is correct on both.

## Clickable modules

Scripts can react to clicks via the `$BLOCK_BUTTON` environment variable.
Requires the [statuscmd](https://dwm.suckless.org/patches/statuscmd/) patch
in dwm (patch credit: Daniel Bylinka).

## Installation

```sh
git clone https://github.com/james5618/dwmblocks.git
cd dwmblocks
sudo make install
```

## FreeBSD

This is the `freebsd` branch, for FreeBSD 15. The status bar waited for
signals with Linux's `signalfd(2)` plus `poll(2)`, neither of which FreeBSD
has, so the wait loop uses `sigwaitinfo(2)` there instead. That was chosen
over a kqueue `EVFILT_SIGNAL` because kqueue does not carry the queued
`sigval` and the clickable modules need it to know which mouse button dwm
saw. The Linux path is kept behind `#ifdef __linux__`, so this branch still
builds and behaves as before on Linux.

The build also uses `cc` and the `/usr/local` X11 paths from ports.

Build dependencies: `pkg install libX11 pkgconf`.
