# Changelog

## v1.0.0

First release. Direct controller input for the AYN Odin 3 and AYN Thor on Armada, without InputPlumber.

- Raw rsinput gamepad handed to SDL and Steam (udev rule, mouse class cleared)
- InputPlumber masked, leftover hide rules removed
- Odin 3 back paddles as A and B
- Bottom right button opens the Quick Access Menu (Guide then A, 80 ms steps)
- `handheld-directinput on | off | status | uninstall`
- Replaces the older odin3-direct automatically

Install on the device:

```bash
echo "=== download ==="
cd ~
curl -fsSL https://github.com/cerroverb/handheld-directinput/releases/latest/download/handheld-directinput-aarch64.tar.gz | tar xz
echo "=== install and turn on ==="
cd ~/handheld-directinput
./install.sh
```

The Quick Access button on the Thor runs the same code as on the Odin 3 but has not been tried on a Thor yet.
