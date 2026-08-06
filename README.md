

## Modifying blocks

Blocks are added and removed in [config.h](config.h); each block runs a
script from
[`~/.local/bin/statusbar`](https://github.com/james5618/dotfiles/tree/master/.local/bin/statusbar)
in my dotfiles, so install those if you want the bar working out of the box.

## Signaling changes

Each module has a signal number, so it can be updated on demand instead of
polling: `pkill -RTMIN+<num> dwmblocks`, or the faster
`kill -<34+num> $(pidof dwmblocks)`. The volume module never polls at all —
dwm's volume keybindings signal it directly. All modules must have distinct
signal numbers.

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

## Licence

GPLv2, see [LICENSE](LICENSE). The copyright notice lives here rather than in
that file because the GPL text is meant to be passed on verbatim.

© 2026 james5618 <james@josullivan.co.uk>
