# handheld-directinput

Built-in controller input without InputPlumber for the AYN Odin 3 and the AYN Thor on Armada. Games and Steam read the gamepad straight from the kernel driver, with nothing in between.

This is my own setup, packaged so anyone can copy it. It is not an official Armada feature.

## What it does

- Masks InputPlumber, so it no longer grabs and hides the gamepad.
- Adds a udev rule that marks the raw gamepad as a joystick, so SDL and Steam open it. The gamepad is "AYN Odin3 Gamepad" on the Odin 3 and "AYN Odin2 Gamepad" on the Thor. The rule also clears the mouse class udev gives it (otherwise the left stick moves the desktop cursor) and tells libinput to ignore it.
- Runs a small C service that writes the extra buttons into the gamepad node, so everything still sees one controller.
  - Odin 3 back paddles. Left is A, right is B.
  - Bottom right button, on both devices. It opens Steam's Quick Access Menu. The driver reports it as `BTN_BACK`, which SDL does not map, so on its own it does nothing. While it is held, the service sends Guide and then A 80 ms later, which is the same chord InputPlumber sends for it.

Everything lives in `/etc` and `/var`, so it works with Armada's read-only `/usr` and survives image updates.

## Why

On my Odin 3, InputPlumber added a median 0.99 ms (p10 0.20 ms, p90 1.58 ms) between the controller report and its virtual output, and sent about 4.6 output reports for every input report. Method and data are in [armada-os/armada#808](https://github.com/armada-os/armada/issues/808).

With this setup the sticks, triggers and normal buttons go kernel to game. Only the paddles and the bottom right button pass through the service. I measured the paddle path on the Odin 3 in a game at a median 0.046 ms and 0.061 ms over two runs of about 500 presses each.

## What you lose

- Gyro. On Armada it is read through InputPlumber.
- Armada Control's controller type setting (Steam Deck, Xbox, DualSense). Steam sees a generic controller.
- Remapping the paddles outside Steam Input. They are fixed to A and B.
- The Thor's AYN key is not mapped to anything.

`handheld-directinput off` gives all of it back.

## Tested

- Odin 3 Ultra on Armada. Desktop Mode and Game Mode, across reboots. Steam UI, Guide, the Quick Access button, paddles as A and B, and games with Steam Input on and off.
- Thor on Armada. Direct mode with the raw gamepad. The Quick Access button on the Thor runs the same code as on the Odin 3 but has not been tried on a Thor yet.

## Install

On the Odin 3 or Thor, in Desktop Mode, open a terminal and run this. It asks for your password once.

```bash
echo "=== download ==="
cd ~
curl -fsSL https://github.com/cerroverb/handheld-directinput/releases/latest/download/handheld-directinput-aarch64.tar.gz | tar xz
echo "=== install and turn on ==="
cd ~/handheld-directinput
./install.sh
```

The last lines show the status. InputPlumber should read `masked inactive` and the buttons service `enabled active`. If Steam does not pick up the controller right away, reboot once.

If you used my older `odin3-direct` on the Odin 3, `install.sh` removes it first.

## Use

```bash
echo "=== show the current state ==="
handheld-directinput status
echo "=== back to InputPlumber ==="
handheld-directinput off
echo "=== direct mode again ==="
handheld-directinput on
echo "=== remove everything and turn InputPlumber back on ==="
handheld-directinput uninstall
```

If the command is not found, use `~/.local/bin/handheld-directinput` instead.

## What gets installed

| Path | What |
|---|---|
| `/var/lib/handheld-directinput/buttons` | the button service (`src/buttons.c`) |
| `/var/lib/handheld-directinput/handheld-directinput` | the on/off/status/uninstall command, linked into `~/.local/bin` |
| `/etc/udev/rules.d/65-handheld-directinput.rules` | marks the gamepad as a joystick, hides the paddles as a second controller |
| `/etc/udev/rules.d/99-handheld-directinput-acl.rules` | gives your user access to the gamepad even without a logind seat |
| `/etc/systemd/system/handheld-directinput.service` | runs the button service at realtime priority |

`on` also masks `inputplumber.service`. `off` and `uninstall` unmask and start it again.

## Build from source

Armada has no compiler on the host, so build in a toolbox.

```bash
echo "=== get the source ==="
git clone https://github.com/cerroverb/handheld-directinput ~/handheld-directinput-src
echo "=== build in a Fedora toolbox ==="
toolbox list --containers | grep -q fedora-toolbox-43 || toolbox create -y --distro fedora --release 43
toolbox run -c fedora-toolbox-43 sh -c 'command -v gcc >/dev/null && command -v make >/dev/null || sudo dnf install -y gcc make'
toolbox run -c fedora-toolbox-43 make -C ~/handheld-directinput-src
echo "=== install ==="
cd ~/handheld-directinput-src
./install.sh
```

`make release` cross-builds the static aarch64 binary and the release tarball (needs `aarch64-linux-gnu-gcc`).

## Notes

- Gamepad names come from Armada's device trees. If an update renames the devices or changes how InputPlumber or udev hide them, this can break. `handheld-directinput off` always gets you back to the stock setup.
- The service can only send buttons the gamepad already has, so it duplicates A, B and Guide but cannot add new buttons.
- X and Y are correct without InputPlumber. The driver sends `BTN_NORTH` and `BTN_WEST`, which are the same codes as `BTN_X` and `BTN_Y`, and SDL maps those to X and Y.

Related upstream requests: [shuuri-labs/pocknix-os#166](https://github.com/shuuri-labs/pocknix-os/issues/166), [ShadowBlip/InputPlumber#799](https://github.com/ShadowBlip/InputPlumber/issues/799).

## License

GPL-3.0-or-later. The Quick Access chord and its timing follow InputPlumber's Xbox target (`src/input/target/xpad.rs`, GPL-3.0).
