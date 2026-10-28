# ⚡ ROG Power Manager & Live Wattage Tray Indicator (`rog-power-manager`)

🇬🇧 **English** | 🇩🇪 **[Deutsch](README.de.md)**

[![License: GPL v3+](https://img.shields.io/badge/License-GPLv3%2B-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/Version-0.3.0-brightgreen.svg)](https://github.com/Gigoslav/rog-power-manager/releases)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Arch%20%7C%20KDE%20Plasma%206-informational.svg)](https://github.com/Gigoslav/rog-power-manager)

Intelligent 3-state power management, **ASUS fan control (`Silent`, `Balanced`, `Turbo`, `Max`)**, automatic refresh rate switching (`60 Hz` ↔ `240 Hz`), **live KDE system tray Wattage monitor (`Total IN = Battery + System`)**, **battery protection mode with live C-rate calculation**, and **GPU/Hardware MUX switch** (`supergfxctl` + `asus-nb-wmi`) tailored for **ASUS ROG Strix (G17 G713RM)** laptops running Linux & KDE Plasma 6 (Wayland & X11).

Automatically adapts to the desktop system language (**German `de`** and **English `en`** built-in).

---

## ✨ Features

1. **Automatic 3-State Power, Display & Brightness Switching**
   Monitors `UPower` D-Bus signals and `sysfs` power supplies to detect whether the laptop is on **Battery**, connected via **USB-C Power Delivery (`ucsi-source-psy`)**, or running on the **240W Barrel Charger (`ADP0`)**:

   | Power State | Power Profile | Fan Mode | Display Rate | Screen Brightness | Keyboard LED | Hardware Tuning |
   | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
   | **⚡ Battery (`battery`)** | `power-saver` | `Silent` (0 RPM) | **60 Hz** | **50%** | Level 1 | Boost Disabled, Panel OD Disabled, PCIe PM `auto`, Wi-Fi PS `on` (**~12.4W Idle**) |
   | **🔌 USB-C PD (`usbc`)** | `balanced` | `Balanced` | **60 Hz** | **75%** | Level 1 | Prevents tripping 45W/65W chargers (**~0.45 C Gentle Charging**) |
   | **🚀 240W AC (`ac`)** | `performance` | `Turbo` | **240 Hz** | **100%** | Level 3 | Full CPU Boost & Panel Overdrive (**~0.95 C Fast Charging**) |

2. **💨 Integrated ASUS Fan Control (`asus-nb-wmi`)**
   * **🔄 Auto (Profile-based):** Automatically syncs fan curves with the active power state.
   * **🔇 Silent (Quiet):** Fanless 0 RPM passive operation up to ~55–60 °C, whisper quiet under light load.
   * **⚖️ Balanced (Standard):** Balanced factory thermal curves.
   * **🚀 Turbo (High Cooling):** Aggressive fan speeds for sustained high performance during gaming and compiling.
   * **🌪️ Max (100% Full Speed):** Ramps both CPU & GPU fans to 100% PWM (~5500 RPM) for rapid cooling and dust discharge.
   * **Live Fan RPM Telemetry:** Real-time RPM reading for CPU and GPU fans in tray menu, tooltip, and CLI.

3. **Live Wattage KDE Tray Badge (`Total IN = Battery + System`)**
   * **Two-line live icon badge** (updated every 2 seconds):
     * **Charging (Green):** Top line displays **`Total IN`** (`89W`), bottom line displays **`+Battery|System`** (`+64|25`).
     * **Discharging (Orange/Red):** Top line displays **`-SystemW`** (`-12W`), bottom line displays **`OUT`**.
     * **Direct AC Bypass / Limit Reached (Blue):** Top line displays **`SystemW`**, bottom line displays **`0|SystemW`**.
   * **Clean Tray Menu & Tooltip:**
     ```text
     ⚡ Energy Balance (Live)
     🔌 Total IN:    54.5 W  [USB-C PD (Max 60W)]
     ├── 🔋 Battery: +30.3 W  (0.38 C • Charging • 45%/80% • ~0.8h to 80%)
     └── 💻 System:   24.2 W  (Active Load)
            ↳ CPU/iGPU: 17.2W | RTX 3060: 0.0W | Display/Board: 7.0W
     ```
   * **Preserves NVIDIA `D3cold` Sleep:** Queries AMD Ryzen APU power (`PPT`) via `/sys/class/hwmon` and checks RTX 3060 sleep state directly via `sysfs` **without calling `nvidia-smi`**, allowing the dGPU to sleep uninterrupted at `0.0W`.

4. **Battery Health & C-Rate Management**
   * Calculates real-time battery charge/discharge C-rate (`Watts / 80 Wh capacity`).
   * **⏸️ Stop Charging Immediately (`0.0 C` AC Bypass):** Clamps the charge limit to current capacity for pure direct AC operation without battery stress.
   * **🛡️ One-Click Charge Limits:** **60% (Desk / Ultra-Protection)**, **80% (Daily Balanced)**, and **100% (Travel / Full)**.

5. **Integrated GPU & Hardware MUX Switcher**
   * **🔋 Integrated (`Integrated`):** AMD Radeon 680M only (`0.0W` RTX 3060).
   * **⚡ Hybrid (`Hybrid`):** AMD Radeon 680M default with on-demand NVIDIA (`prime-run`), sleeping at `0.0W` when idle.
   * **🔥 Dedicated (`AsusMuxDgpu`):** Hardware MUX (`gpu_mux_mode = 0`) wired directly to the NVIDIA RTX 3060.

6. **Complete KDE PowerDevil Drop-in Replacement (`KIdleTime`)**
   * Wayland/X11 idle dimming (to 25%), DPMS screen off, and auto-suspend with wake restoration.
   * Standard D-Bus services: `org.freedesktop.PowerManagement` and `org.kde.Solid.PowerManagement` (Fn keys).
   * Presentation Mode toggle (`☕ Keep System Awake`).

---

## 🚀 Installation

### Prerequisites (Arch / EndeavourOS)
```bash
sudo pacman -S power-profiles-daemon python-pyqt6 python-dbus nvidia-prime switcheroo-control iw
yay -S supergfxctl
sudo systemctl enable --now supergfxd switcheroo-control
```

### Option A: Arch Linux Package (`makepkg`) — Recommended
```bash
git clone https://github.com/Gigoslav/rog-power-manager.git
cd rog-power-manager
makepkg -si
```

### Option B: User & System Helper Install
```bash
git clone https://github.com/Gigoslav/rog-power-manager.git
cd rog-power-manager
./install.sh
```

---

## ⚙️ Settings & Automations

Open the settings dialog via the tray menu (**⚙️ Settings & Automations...**):

1. **⚡ Profiles & Hardware:** Customize power profiles (`power-saver`, `balanced`, `performance`), fan mode (`auto`, `silent`, `balanced`, `turbo`, `max`), refresh rate (`60 Hz` / `240 Hz`), screen brightness (`10-100%`), and keyboard backlight per power state (`battery`, `usbc`, `ac`).
2. **⏱️ Inactivity & Display:** Set custom timeouts for idle dimming, DPMS screen-off, and auto-suspend.
3. **🚀 Automations & Hooks:**
   - Execute custom shell commands upon power source transitions or low battery.
   - User script hooks placed in `~/.config/rog-power-manager/hooks.d/`:
     - `on_state_change <new_state> <old_state>`
     - `battery_low <capacity>`
     - `battery_critical <capacity>`
   - Persistent user configuration stored at `~/.config/rog-power-manager/config.json`.

---

## 🛠️ CLI Usage

```bash
# Show current power source, energy balance, C-rate, fan RPM & display status:
rog-power-manager --status

# Change fan mode directly from CLI:
rog-power-manager --fan silent
rog-power-manager --fan balanced
rog-power-manager --fan turbo
rog-power-manager --fan max
rog-power-manager --fan auto

# Force English or German output:
rog-power-manager --lang en --status
rog-power-manager --lang de --status

# Show version, author credits and license:
rog-power-manager --version

# Apply matching profile & refresh rate once and exit:
rog-power-manager --once
```

*(Note: The legacy command `auto-power-profile.py` remains fully functional as a compatibility symlink.)*

---

## 👤 Author, Credits & License

* **Author & Maintainer:** **Marcel Berghaus ([Mountainhouse Software Solutions](https://mountain-house.dev) / [Gigoslav](https://github.com/Gigoslav))** — `<berghaus.marcel@gmail.com>`
* **Website:** **[https://mountain-house.dev](https://mountain-house.dev)**
* **Repository:** **[https://github.com/Gigoslav/rog-power-manager](https://github.com/Gigoslav/rog-power-manager)**
* **License:** Released under the **GNU General Public License v3.0 or later (`GPL-3.0-or-later`)** — see [LICENSE](LICENSE) and [NOTICE](NOTICE).
* **Attribution Notice (GPLv3 § 7b):** All copies, packages, forks, or derivative works **must** retain the original copyright notice, author attribution (`Marcel Berghaus / Mountainhouse Software Solutions`), website link (`https://mountain-house.dev`), and repository link.
