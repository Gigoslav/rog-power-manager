#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Project: https://github.com/Gigoslav/rog-power-manager

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Building & Installing ROG Power Manager (Native C++ / Qt6) ==="

cd "$REPO_DIR"
make build
sudo make install
install -Dm755 build/rog-power-manager ~/.local/bin/rog-power-manager

# Clean up conflicting legacy battery-charge-threshold.conf if present
if [ -f /etc/tmpfiles.d/battery-charge-threshold.conf ]; then
    echo "=== Removing conflicting legacy /etc/tmpfiles.d/battery-charge-threshold.conf ==="
    sudo rm -f /etc/tmpfiles.d/battery-charge-threshold.conf 2>/dev/null || true
fi

# Clean up legacy auto-power-profile user service if present
if systemctl --user is-active --quiet auto-power-profile.service 2>/dev/null; then
    echo "=== Migrating from auto-power-profile.service ==="
    systemctl --user stop auto-power-profile.service 2>/dev/null || true
    systemctl --user disable auto-power-profile.service 2>/dev/null || true
    rm -f ~/.config/systemd/user/auto-power-profile.service
fi

sudo systemd-tmpfiles --create /etc/tmpfiles.d/rog-power-manager.conf 2>/dev/null || true
sudo udevadm control --reload-rules 2>/dev/null || true
sudo udevadm trigger 2>/dev/null || true

# Replace KDE PowerDevil to avoid all background conflicts
echo "=== Replacing KDE PowerDevil (stopping & masking plasma-powerdevil.service) ==="
systemctl --user stop plasma-powerdevil.service 2>/dev/null || true
systemctl --user mask plasma-powerdevil.service 2>/dev/null || true

chmod 644 ~/.config/systemd/user/rog-power-manager.service 2>/dev/null || true
systemctl --user daemon-reload
systemctl --user enable --now rog-power-manager.service
systemctl --user restart rog-power-manager.service

echo "=== Installation complete! ==="
rog-power-manager --version
rog-power-manager --status
