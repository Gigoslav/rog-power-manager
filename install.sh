#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Project: https://github.com/Gigoslav/rog-auto-power

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Installing ROG Auto-Power & Live Watt Indicator (v0.3.0) ==="

mkdir -p ~/.local/bin ~/.config/systemd/user ~/.local/share/applications
ln -sf "$REPO_DIR/auto-power-profile.py" ~/.local/bin/auto-power-profile.py
cp "$REPO_DIR/auto-power-profile.service" ~/.config/systemd/user/auto-power-profile.service
cp "$REPO_DIR/rog-auto-power.desktop" ~/.local/share/applications/rog-auto-power.desktop

echo "=== Installing udev rules & systemd-tmpfiles (Unprivileged Sysfs Access) ==="
sudo install -Dm644 "$REPO_DIR/99-rog-auto-power.rules" /etc/udev/rules.d/99-rog-auto-power.rules
sudo install -Dm644 "$REPO_DIR/rog-auto-power.conf" /etc/tmpfiles.d/rog-auto-power.conf
sudo systemd-tmpfiles --create /etc/tmpfiles.d/rog-auto-power.conf
sudo udevadm control --reload-rules || true
sudo udevadm trigger || true

echo "=== Installing CLI helper (/usr/local/bin/rog-gpu-mux-helper) ==="
sudo install -Dm755 "$REPO_DIR/rog-gpu-mux-helper" /usr/local/bin/rog-gpu-mux-helper

# Clean up legacy sudoers file if it exists, as daemon now runs unprivileged without sudo
if [ -f "/etc/sudoers.d/$USER-rog-mux" ]; then
    echo "=== Removing legacy sudoers entry (/etc/sudoers.d/$USER-rog-mux) ==="
    sudo rm -f "/etc/sudoers.d/$USER-rog-mux"
fi

# Replace KDE PowerDevil to avoid all background conflicts
echo "=== Replacing KDE PowerDevil (stopping & masking plasma-powerdevil.service) ==="
systemctl --user stop plasma-powerdevil.service 2>/dev/null || true
systemctl --user mask plasma-powerdevil.service 2>/dev/null || true

systemctl --user daemon-reload
systemctl --user enable --now auto-power-profile.service
systemctl --user restart auto-power-profile.service

echo "=== Installation complete! ==="
~/.local/bin/auto-power-profile.py --version
~/.local/bin/auto-power-profile.py --status
