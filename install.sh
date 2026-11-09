#!/usr/bin/env bash
# Installs handheld-directinput on an AYN Odin 3 or AYN Thor and turns it on.
# Usage: ./install.sh [--user NAME]
#   --user NAME   user who gets access to the gamepad (default: you)
# Everything goes to /etc and /var, so a read-only /usr (Armada) is fine.
set -eu
cd "$(dirname "$0")"

D=/var/lib/handheld-directinput
COMPATIBLE=${COMPATIBLE:-/sys/firmware/devicetree/base/compatible}
user=${SUDO_USER:-$(id -un)}
case "${1:-}" in
"") ;;
--user) user=${2:?--user needs a name} ;;
*) echo "usage: $0 [--user NAME]" >&2; exit 1 ;;
esac

# 1. Only the AYN Odin 3 and AYN Thor.
if ! tr '\0' '\n' <"$COMPATIBLE" 2>/dev/null | grep -qx -e 'ayn,odin3' -e 'ayn,thor'; then
    echo "This is not an AYN Odin 3 or AYN Thor, nothing installed." >&2
    exit 1
fi

# 2. The buttons binary. Release downloads ship bin/buttons.
if [ -x bin/buttons ]; then
    bin=bin/buttons
elif [ -x build/buttons ]; then
    bin=build/buttons
elif command -v cc >/dev/null && command -v make >/dev/null; then
    make
    bin=build/buttons
else
    echo "No bin/buttons and no compiler here." >&2
    echo "Use the release download, or build in a toolbox first (see README)." >&2
    exit 1
fi

[ "$(id -u)" -eq 0 ] || exec sudo "$0" "$@"

home=$(getent passwd "$user" | cut -d: -f6)
[ -n "$home" ] || { echo "unknown user $user" >&2; exit 1; }

# 3. Remove the older odin3-direct, so two services don't both press buttons.
if [ -e /var/lib/odin3-direct ] || [ -e /etc/systemd/system/odin3-paddles.service ]; then
    echo "Removing the older odin3-direct setup"
    systemctl disable --now odin3-paddles.service 2>/dev/null || true
    rm -f /etc/systemd/system/odin3-paddles.service
    rm -f /etc/udev/rules.d/99-zz-odin3-direct.rules \
        /etc/udev/rules.d/65-odin3-direct.rules \
        /etc/udev/rules.d/99-odin3-direct-acl.rules
    if [ -r /var/lib/odin3-direct/linkpath ]; then rm -f "$(cat /var/lib/odin3-direct/linkpath)"; fi
    rm -f "$home/.local/bin/odin3-direct"
    rm -rf /var/lib/odin3-direct
fi

# 4. Point out other rules for the same devices, e.g. from a manual setup.
for f in /etc/udev/rules.d/*.rules; do
    case "${f##*/}" in 65-handheld-directinput.rules | 99-handheld-directinput-acl.rules) continue ;; esac
    if grep -q -e 'AYN Odin3 Gamepad' -e 'AYN Odin2 Gamepad' -e 'gpio-keys-paddles' -e 'rsinput-gamepad' "$f" 2>/dev/null; then
        echo "Note: $f also matches the gamepad. Remove it if it is left over from a manual setup."
    fi
done

# 5. Install.
install -d "$D"
install -m 755 "$bin" "$D/buttons"
install -m 755 handheld-directinput "$D/handheld-directinput"
install -m 644 udev/65-handheld-directinput.rules "$D/65-handheld-directinput.rules"
sed "s/@USER@/$user/" udev/99-handheld-directinput-acl.rules.in >"$D/99-handheld-directinput-acl.rules"
install -m 644 systemd/handheld-directinput.service /etc/systemd/system/handheld-directinput.service
systemctl daemon-reload

link=$home/.local/bin/handheld-directinput
runuser -u "$user" -- mkdir -p "$home/.local/bin"
ln -sf "$D/handheld-directinput" "$link"
echo "$link" >"$D/linkpath"

# 6. Turn it on.
"$D/handheld-directinput" on
"$D/handheld-directinput" status
echo "Done. Turn it off with: handheld-directinput off"
