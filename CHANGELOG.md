# Changelog

All notable changes to **`rog-power-manager`** are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0] - 2026-10-28
### Added
- **Project Renamed to `rog-power-manager`:**
  - Standardized all binaries, services, desktop files, rules, and configuration directories to `rog-power-manager` (`rog-power-manager.py`, `rog-power-manager.service`, `~/.config/rog-power-manager/`).
  - Seamless migration of existing configuration and backwards compatibility symlinks (`rog-auto-power`, `auto-power-profile.py`).
- **ASUS Fan Control Engine (`Silent`, `Balanced`, `Turbo`, `Max`):**
  - Integrated fan control for ASUS ROG laptops via `asus-nb-wmi` (`throttle_thermal_policy` & `asus_custom_fan_curve`).
  - **Silent Mode:** Passive fanless cooling (`0 RPM`) up to ~55-60 °C, whisper quiet under light load.
  - **Balanced Mode:** Standard factory cooling curve.
  - **Turbo Mode:** Aggressive fan profile for peak sustained performance.
  - **Max Fan Mode:** Ramps CPU and GPU fans to 100% PWM (~5500 RPM) for rapid cooling and dust purge.
  - **Auto Sync:** Automatically selects optimal fan mode based on power state (`battery` -> silent, `usbc` -> balanced, `ac` -> turbo) with configurable defaults.
  - **Live RPM Telemetry:** Real-time RPM reading for CPU and GPU fans in tray menu, tooltip, About dialog, and CLI.
  - **CLI Switch:** Added `--fan auto|silent|balanced|turbo|max` for scripting and instant terminal switching.
- **Interactive GUI Settings & Automations (`SettingsDialog`):**
  - Graphical settings dialog accessible directly from the tray menu (`⚙️ Einstellungen & Automationen...` / `⚙️ Settings & Automations...`).
  - **Profiles & Hardware Tab:** Live configuration of power profile, fan mode, refresh rate (`60Hz` / `240Hz`), screen brightness (`10-100%`), and keyboard lighting (`0-3`) per power source (`battery`, `usbc`, `ac`).
  - **Inactivity & Display Tab:** Customizable timeouts for display dimming, DPMS screen-off, and auto-suspend per power source.
  - **Automations & Hooks Tab:** Custom shell commands triggered on power source transition, custom battery alert thresholds, and direct access to the hooks directory.
- **Config & Extensibility Engine:**
  - Configuration saved to `~/.config/rog-power-manager/config.json` with sensible defaults and persistent user overrides.
  - User hooks directory `~/.config/rog-power-manager/hooks.d/` executing arbitrary executable scripts for `on_state_change`, `battery_low`, and `battery_critical`.
- **Modern GUI About Window (`AboutDialog`):**
  - Replaced simple web popup with a dedicated dark-themed dialog displaying developer information, organization branding, links, and live system telemetry (power source, active profile, fan status, refresh rate, battery level/limit, RTX 3060 D3cold sleep state).
- **Brand Update:**
  - Updated author organization and website to **Mountainhouse Software Solutions** (`https://mountain-house.dev`).

## [0.2.0] - 2026-10-28
### Added
- **Full KDE PowerDevil Replacement:**
  - **Inactivity Idle Management (KIdleTime):** Native Wayland/X11 idle detection managing 3-state display dimming (to 25%), DPMS screen off (`kscreen-doctor --dpms off`), and auto-suspend (`loginctl suspend`) with automatic wake restoration on user resume.
  - **Freedesktop D-Bus Services:** Exports standard `org.freedesktop.PowerManagement` and `org.freedesktop.PowerManagement.Inhibit` so web browsers (Chrome, Firefox) and media players (VLC, MPV) can inhibit idle sleep and blanking.
  - **KDE Solid D-Bus Drop-in:** Exports `org.kde.Solid.PowerManagement.Actions.BrightnessControl`, `KeyboardBrightnessControl`, and `PolicyAgent` so laptop Fn keys and KDE widgets interact directly with `rog-auto-power`.
  - **Keep Awake / Presentation Mode:** Tray menu toggle (`☕ System wachhalten`) to prevent dimming, display blanking, and sleep.
  - **Low & Critical Battery Alerts:** Proactive warning notification at ≤ 10% battery, and critical alert with automatic emergency suspend at ≤ 5% to prevent data loss.
  - **Quick Display & Keyboard Controls:** Added submenus to the tray for 10%, 25%, 50%, 75%, 100% display brightness and 4-level keyboard backlighting.
  - **Session Actions:** Added Lock Screen, Sleep (Suspend), and Hibernate directly to the tray menu.
- **Unprivileged Direct Sysfs Access (No Sudo Required):** Shipped `99-rog-auto-power.rules` (udev) and `rog-auto-power.conf` (`systemd-tmpfiles`) granting direct user write permissions to display backlight, keyboard backlight, battery charge limit, ASUS WMI features (panel overdrive, GPU Mux), and CPU boost.
- **Clean Sudo-Free GitHub & AUR Installation:** Removed `/etc/sudoers.d` requirement. The daemon runs 100% as an unprivileged user service without polling `sudo`.
- **Automated Conflict Resolution:** Installer cleanly stops and masks `plasma-powerdevil.service` to prevent background interference.

## [0.1.2] - 2026-10-16
### Changed
- **License & Mandatory Credits (`GPL-3.0-or-later`):** Switched to GNU General Public License v3.0 or later (`GPL-3.0-or-later`) with explicit Section 7(b) author attribution notice (`NOTICE` & `AUTHORS`) and SPDX headers across all files.
- **Standard Linux / FHS Packaging:** Added standard `Makefile` (`PREFIX=/usr`, `DESTDIR=...`), Freedesktop XDG entry (`rog-auto-power.desktop`), `--version` CLI output, and in-app **About & Credits** tray menu item.

## [0.1.1] - 2026-10-16
### Added
- **Automatic Localization (`en` / `de`):** Automatic system locale detection (`LANG`, `LC_ALL`, `LC_MESSAGES`) with `--lang en|de` CLI override and `ROG_LANG` environment variable.
- **Battery C-Rate & Protection Modes:** Live C-Rate calculation (`W / Wh`), instant `0.0 C` AC Bypass mode (stop charging immediately at current percentage), and persistent `60%`, `80%`, and `100%` charge thresholds.
- **Deep Hardware Power Savings:** Automatic screen brightness switching (`50%` Battery / `75%` USB-C / `100%` AC), CPU Turbo Boost toggle, ASUS Panel Overdrive toggle, PCIe Runtime PM (`auto`), and Wi-Fi power-save (`12.4 W` idle on battery).

### Fixed
- **NVIDIA `D3cold` Sleep Fix:** Removed `nvidia-smi` from polling loop and switched to pure `sysfs` runtime PM inspection (`/sys/bus/pci/devices/0000:01:00.0/power/runtime_status`) so the RTX 3060 stays suspended at `0.0 W` on battery and hybrid mode.

## [0.1.0] - 2026-10-16
### Added
- Initial release with 3-state power switching (`battery`, `usbc`, `ac`), `60 Hz` / `240 Hz` refresh rate switching via `kscreen-doctor`, live 2-line KDE system-tray Wattage badge (`Total IN = Battery + System`), and GPU/MUX switching (`Integrated`, `Hybrid`, `AsusMuxDgpu`).
