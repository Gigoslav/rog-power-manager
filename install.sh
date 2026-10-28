#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Project: https://github.com/Gigoslav/rog-power-manager

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Installing ROG Power Manager & Fan Control (v0.3.0) ==="

mkdir -p ~/.local/bin ~/.config/systemd/user ~/.local/share/applications
ln -sf "$REPO_DIR/rog-power-manager.py" ~/.local/bin/rog-power-manager
ln -sf "$REPO_DIR/rog-power-manager.py" ~/.local/bin/rog-power-manager.py
ln -sf "$REPO_DIR/rog-power-manager.py" ~/.local/bin/rog-auto-power
ln -sf "$REPO_DIR/rog-power-manager.py" ~/.local/bin/auto-power-profile.py

cp "$REPO_DIR/rog-power-manager.service" ~/.config/systemd/user/rog-power-manager.service
cp "$REPO_DIR/rog-power-manager.desktop" ~/.local/share/applications/rog-power-manager.desktop

# Clean up legacy auto-power-profile user service if present
if systemctl --user is-active --quiet auto-power-profile.service 2>/dev/null; then
    echo "=== Migrating from auto-power-profile.service ==="
    systemctl --user stop auto-power-profile.service 2>/dev/null || true
    systemctl --user disable auto-power-profile.service 2>/dev/null || true
    rm -f ~/.config/systemd/user/auto-power-profile.service
fi

echo "=== Installing udev rules & systemd-tmpfiles (Unprivileged Fan & Power Sysfs Access) ==="
sudo install -Dm644 "$REPO_DIR/99-rog-power-manager.rules" /etc/udev/rules.d/99-rog-power-manager.rules
sudo install -Dm644 "$REPO_DIR/rog-power-manager.conf" /etc/tmpfiles.d/rog-power-manager.conf
sudo rm -f /etc/udev/rules.d/99-rog-auto-power.rules /etc/tmpfiles.d/rog-auto-power.conf
sudo systemd-tmpfiles --create /etc/tmpfiles.d/rog-power-manager.conf
sudo udevadm control --reload-rules || true
sudo udevadm trigger || true

echo "=== Installing CLI helper (/usr/local/bin/rog-power-helper) ==="
sudo install -Dm755 "$REPO_DIR/rog-power-helper" /usr/local/bin/rog-power-helper
sudo ln -sf /usr/local/bin/rog-power-helper /usr/local/bin/rog-gpu-mux-helper

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
systemctl --user enable --now rog-power-manager.service
systemctl --user restart rog-power-manager.service

echo "=== Installation complete! ==="
~/.local/bin/rog-power-manager --version
~/.local/bin/rog-power-manager --status
