# Changelog

All notable changes to **`rog-power-manager`** are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.3] - 2026-10-31
### Fixed
- **Live Dynamic System Power & Wattage Fluctuations in System Tray:**
  - Implemented `getApuWatts()` in `Hardware.cpp` reading live AMD APU package power (PPT) directly from sysfs (`/sys/class/hwmon/hwmon*/power1_input`).
  - Replaced static/hardcoded 25W AC power baseline with real-time dynamic total: APU package power + discrete GPU state + dynamic display backlight ratio and refresh-rate overhead.
  - Fixed static tray icon badge and tooltip in KDE Plasma StatusNotifierItem: live wattage now immediately fluctuates with CPU/GPU load (updating every 500ms).

## [0.4.2] - 2026-10-31
### Added
- **Configurable & High-Speed Live Telemetry Refresh Rate:**
  - Increased default telemetry refresh rate from 2000 ms to **1000 ms (1 Sekunde)** for rapid live updates.
  - Added configurable refresh rates: 250 ms (realtime), 500 ms (ultra-fast), 1000 ms (standard), 2000 ms (power-saving).
  - Added "Aktualisierungsrate (Live-Daten)" setting to GUI Settings dialog (`SettingsDialog`).
  - Added CLI flag `--refresh-rate <ms>` (250–5000 ms) to set and persist telemetry intervals.
  - Added interactive CLI live monitoring mode: `rog-power-manager --watch [seconds]`.
- **Performance & Polling Optimization:**
  - Optimized `PowerManagerController::applyState` to return immediately when power source hasn't changed, eliminating redundant subprocess overhead (`kscreen-doctor` and `powerprofilesctl`).

## [0.4.1] - 2026-10-31
### Added
- **Persistent Battery Charge Limit across Reboots & State Changes:**
  - Saves the active battery charge limit (`charge_limit`: 60%, 80%, 100%, or custom/bypass) persistently in `~/.config/rog-power-manager/config.json`.
  - Automatically restores the user's configured charge limit on daemon/app startup and upon power source switches (`applyState`), protecting against hardware EC resets.
  - Added CLI flag `--charge-limit <pct>` to view/set and persist the threshold directly from the terminal.
  - Added "Akku-Ladelimit (Hardware-Schutz)" combobox in the GUI Settings dialog (`SettingsDialog`).
  - Added checkmark states in the Tray Icon context menu for immediate visual feedback on the active limit.
  - Added cleanup of legacy `/etc/tmpfiles.d/battery-charge-threshold.conf` in `install.sh`.
- **Turn off Display & Keyboard Backlight (`turn_off_all` / `lock_and_turn_off_all`):**
  - Dedicated lid close actions that power off the display via DPMS and set keyboard backlight to 0.
- **Maximum Power-Save While Closed (`enable_max_powersave_while_closed`):**
  - Temporarily forces 60 Hz display refresh rate, CPU governor `power-saver`, and fan mode `silent` (0 RPM passiv) while closed.
  - Automatically restores previous display refresh rate, brightness, keyboard backlight level, and fan profile upon reopening the lid.
- **Comprehensive Laptop Lid Close Management & Settings:**
  - Added dedicated `LidManager` inhibiting systemd-logind's default `handle-lid-switch` via D-Bus (`Inhibit("handle-lid-switch")`) to give users 100% control over lid switch behaviors.
  - Reactive lid state changes via UPower D-Bus (`LidIsClosed`) and immediate `/proc/acpi/button/lid/LID0/state` procfs fallback.
  - Configurable actions per power source (`battery`, `usbc`, `ac`): `suspend`, `turn_off_all`, `turn_off_screen`, `max_powersave`, `lock_max_powersave`, `lock`, `lock_and_turn_off`, `lock_and_turn_off_all`, `hibernate`, `nothing`.
  - **Clamshell Mode:** Automatically detects external monitors on non-eDP ports (`/sys/class/drm/*/status`) and keeps laptop running when lid is closed.
  - **Lid Automations & Hooks:** Dedicated GUI settings tab `💻 Zuklappen (Lid)`, custom shell commands, and execution of `~/.config/rog-power-manager/hooks.d/on_lid_close` and `on_lid_open`.

### Changed
- **Tray Context Menu Clean-up:** Removed duplicate screen/keyboard action from tray context menu to keep the menu sleek; lid switch and power saving behaviors are managed centrally in the settings dialog.

## [0.4.0] - 2026-10-28
### Added
- **Complete Native C++ / Qt6 Port:**
  - Replaced Python/PyQt6 runtime with a high-performance, native C++20 / Qt6 binary (`rog-power-manager`).
  - Native CMake build system and FHS-compliant Makefile.
  - Architecture:
    - `src/core/Hardware`: Direct POSIX/Sysfs I/O for ASUS fan control, Wattage telemetry, display refresh rate, brightness, keyboard backlighting, and boost.
    - `src/core/Config`: Persistent JSON configuration (`~/.config/rog-power-manager/config.json`) and executable hook runner (`~/.config/rog-power-manager/hooks.d/`).
    - `src/core/IdleManager`: Dynamic loading of `libKF6IdleTime.so.6` for Wayland and X11 display dimming, DPMS-off, and auto-suspend.
    - `src/dbus/DBusServices`: Complete Qt D-Bus implementation of `org.freedesktop.PowerManagement`, `Inhibit`, `org.kde.Solid.PowerManagement`, and listener for KDE Fn brightness/kbd/profile shortcuts.
    - `src/gui/TrayIcon`: Dynamic 64x64 Wattage badge with sub-millisecond drawing, rich context menus, and live hardware indicators.
    - `src/gui/SettingsDialog` & `AboutDialog`: Modern dark-theme configuration and system status dialogs.
  - Single binary under 1 MB, startup in under 15ms, and reduced memory footprint.
  - Python version preserved in `legacy/python/`.
- **Active Profile Visual Indicators:**
  - Dynamic 64x64 tray badge includes a real-time status indicator dot (🔴 Coral/Red for Performance, 🔵 Sky-Blue for Balanced, 🟢 Mint/Green for Power-Saver).
  - Prominent menu header displaying active profile and refresh rate.
  - Exclusive checkable menu actions (`QActionGroup`) reflecting active profile state.
- **Hardware Turbo/Max Fan Curves:**
  - Dynamic discovery of `asus_custom_fan_curve` sysfs hwmon device (`pwm1_enable` / `pwm2_enable`).
  - Turbo mode activates custom PWM curve (`180-255 PWM`) with immediate acoustic feedback (>3500 RPM).
  - Max mode drives all 8 curve points to 255 PWM (~5500 RPM full blast).
  - Silent/Balanced restores auto firmware curve (`pwm1_enable = 2`).
  - `detectFanMode()` queries hardware registers directly for accurate CLI and status feedback.
- **Battery ETA Calculation:**
  - Discharging: Calculates remaining runtime (`~X.Xh remaining`) based on available capacity and real-time power draw.
  - Charging: Calculates estimated time to reach configured battery health limit (`~X.Xh to 80%`).
  - Displayed across tray menu, rich tooltip, and CLI (`--status`).

### Fixed
- **Notification Translations & Desktop Attribution:**
  - Resolved `battery_title`/`battery_msg` placeholder bug by adding canonical translation keys and Breeze-compatible icon fallbacks.
  - Added `-h string:desktop-entry:rog-power-manager` hint for clean notification daemon association.

## [0.3.1] - 2026-10-28
### Added
- **Native KDE Plasma Fn Hotkeys & OSD Integration:**
  - Full listener for `kglobalaccel` shortcuts on `org.kde.kglobalaccel.Component` (`org_kde_powerdevil`).
  - Hardware control and native KDE Plasma OSD popups for:
    - Screen brightness up/down (`Fn+F8` / `Fn+F7`) in 5% and 1% steps.
    - Keyboard backlight up/down/toggle (`Fn+F3` / `Fn+F2`) between levels 0-3.
    - Power profile cycling (`Fn+F5` / `powerProfile`) between `power-saver`, `balanced`, and `performance` with synchronized refresh rate and fan curves.
    - Screen turn-off, sleep, and hibernate shortcuts.
  - CLI flags: `--profile`, `--brightness`, and `--kbd` for direct command-line or external shortcut control.

### Fixed
- **AMD Ryzen CPU Boost & Battery Performance Profile:**
  - Resolved `amd_pstate` CPU driver error 13 when switching to `performance` on battery power by ensuring CPU boost (`/sys/devices/system/cpu/cpufreq/boost`) is enabled prior to profile activation.
  - Fixed daemon state loop overriding manual performance profile selections on battery power; profiles now persist cleanly until an actual power source transition occurs.

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
