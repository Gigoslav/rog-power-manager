# ⚡ ROG Power Manager & Echtzeit-Watt-Anzeige (`rog-power-manager`)

🇬🇧 **[English](README.md)** | 🇩🇪 **Deutsch**

[![Lizenz: GPL v3+](https://img.shields.io/badge/Lizenz-GPLv3%2B-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/Version-0.4.0-brightgreen.svg)](https://github.com/Gigoslav/rog-power-manager/releases)
[![Plattform](https://img.shields.io/badge/Plattform-Linux%20%7C%20Arch%20%7C%20KDE%20Plasma%206-informational.svg)](https://github.com/Gigoslav/rog-power-manager)

Intelligente 3-Stufen-Energieverwaltung, **ASUS-Lüftersteuerung (`Silent`, `Balanced`, `Turbo`, `Max`)**, automatische Bildwiederholraten-Umschaltung (`60 Hz` ↔ `240 Hz`), **Echtzeit-Watt-Anzeige in der KDE-Systemleiste (`Eingang Gesamt = Akku + System`)**, **Akku-Schutzmodus mit C-Raten-Anzeige** sowie **GPU-/Hardware-MUX-Umschalter** (`supergfxctl` + `asus-nb-wmi`) für **ASUS ROG Strix (G17 G713RM)** Laptops unter Linux & KDE Plasma 6 (Wayland & X11).

Erkennt automatisch die eingestellte Systemsprache (**Deutsch `de`** und **Englisch `en`** integriert).

---

## ✨ Funktionen

1. **Automatische 3-Stufen-Energie-, Helligkeits- & Bildwiederholraten-Umschaltung**
   Erkennt über `UPower` D-Bus-Signale & `sysfs`, ob der Laptop im **Akkubetrieb**, über **USB-C Power Delivery (`ucsi-source-psy`)** oder am **240W-Netzteil (`ADP0`)** läuft:

   | Stromquelle | Energieprofil | Lüftermodus | Bildwiederholrate | Bildschirmhelligkeit | Tastatur-LED | Hardware-Optimierung |
   | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
   | **⚡ Akkubetrieb (`battery`)** | `Stromsparmodus` | `Silent` (0 RPM) | **60 Hz** | **50%** | Stufe 1 | Turbo Aus, Panel-Overdrive Aus, PCIe PM `auto`, WLAN-Sparmodus `ein` (**~12,4 W Leerlauf**) |
   | **🔌 USB-C PD (`usbc`)** | `Ausgeglichen` | `Balanced` | **60 Hz** | **75%** | Stufe 1 | Schützt 45W/65W USB-C-Ladegeräte vor Überlastung (**~0,45 C schonendes Laden**) |
   | **🚀 240W-Netzteil (`ac`)** | `Höchstleistung` | `Turbo` | **240 Hz** | **100%** | Stufe 3 | Voller CPU-Turbo & Panel-Overdrive (**~0,95 C Schnellladen**) |

2. **💨 Integrierte ASUS-Lüftersteuerung (`asus-nb-wmi`)**
   * **🔄 Auto (Profilabhängig):** Passt die Lüfterkurve automatisch an die aktive Stromquelle an.
   * **🔇 Silent (Flüsterleise):** Bis ~55–60 °C lüfterloser Betrieb (`0 RPM`), danach sehr leise geregelt.
   * **⚖️ Balanced (Ausgeglichen):** Werksstandard für ausgewogene Kühlung und Lautstärke.
   * **🚀 Turbo (Hohe Kühlung):** Aggressive Lüfterdrehzahlen für niedrige Systemtemperaturen beim Arbeiten und Spielen.
   * **🌪️ Max (100% Volllast):** Beide Lüfter (CPU & GPU) drehen mit 100 % PWM (~5500 RPM) zur schnellen Abkühlung oder Staubentfernung.
   * **Echtzeit-Drehzahlanzeige (RPM):** Live-Messung der CPU- und GPU-Lüfter im Menü, Tooltip und CLI.

3. **Echtzeit-Watt-Anzeige im KDE-Systemabschnitt (`Eingang Gesamt = Akku + System`)**
   * **Zweizeiliges dynamisches Tray-Symbol** (Aktualisierung alle 2 Sekunden):
     * **Lädt (Grün):** Obere Zeile zeigt **`Eingang Gesamt`** (`89W`), untere Zeile zeigt **`+Akku|System`** (`+64|25`).
     * **Entlädt (Orange/Rot):** Obere Zeile zeigt **`-SystemW`** (`-12W`), untere Zeile zeigt **`OUT`**.
     * **Direktbetrieb / Lade-Limit erreicht (Blau):** Obere Zeile zeigt **`SystemW`**, untere Zeile zeigt **`0|SystemW`**.
   * **Übersichtliches Kontextmenü & Kurzinfo:**
     ```text
     ⚡ Energie-Bilanz (Echtzeit)
     🔌 Eingang Gesamt: 54.5 W  [USB-C PD (Ladebetrieb) (Max 60W)]
     ├── 🔋 Akku:      +30.3 W  (0.38 C • Lädt • 45%/80% • ~0.8h bis 80%)
     └── 💻 System:     24.2 W  (Laufender Betrieb)
            ↳ CPU/iGPU: 17.2W | RTX 3060: 0.0W | Bildschirm/Mainboard: 7.0W
     ```
   * **Kein Aufwecken der NVIDIA-Grafikkarte (`D3cold`):** Liest die AMD-Ryzen-APU-Leistungsaufnahme (`PPT`) über `/sys/class/hwmon` (`amdgpu`) und prüft den Schlafzustand der NVIDIA RTX 3060 rein über `/sys/bus/pci/devices/0000:01:00.0/power/runtime_status` **ohne `nvidia-smi` aufzurufen** (wodurch die NVIDIA-GPU im `0,0 W` Tiefschlaf bleibt).

4. **Akku-Schutzmodus & Echtzeit-C-Raten-Kontrolle**
   * Zeigt die aktuelle Lade-/Entladerate als **C-Rate** (`Watt / 80 Wh`).
   * **⏸️ Laden sofort stoppen (`0.0 C` Direktbetrieb):** Setzt das Lade-Limit sofort auf den aktuellen Akkustand, sodass der Laptop zu 100 % direkt über das Netzteil läuft (`0,0 W` Akku-Belastung).
   * **🛡️ Lade-Limits:** Ein-Klick-Umschaltung zwischen **60 % (Ultra-Schutz / Schreibtisch)**, **80 % (Standard-Schutz / Alltag)** und **100 % (Reise-Modus / Vollladung)**.

5. **Integrierter GPU- & Hardware-MUX-Umschalter**
   * **🔋 Integriert (`Integrated`):** Nur AMD Radeon 680M (`0,0 W` NVIDIA).
   * **⚡ Hybrid (`Hybrid`):** AMD Radeon 680M für den Desktop + NVIDIA RTX 3060 bei Bedarf (`prime-run`), im Leerlauf im `0,0 W` Schlafmodus.
   * **🔥 Dediziert (`AsusMuxDgpu`):** Schaltet den Hardware-MUX (`gpu_mux_mode = 0`) direkt auf die NVIDIA RTX 3060.

6. **Vollwertiger PowerDevil-Ersatz & Inaktivitätsverwaltung (`KIdleTime`)**
   * Nativ integriertes Display-Dimmen, DPMS-Bildschirmabschaltung und automatischer Standby mit Wiederaufwecken.
   * D-Bus-Dienste `org.freedesktop.PowerManagement` und `org.kde.Solid.PowerManagement` (Fn-Tasten-Integration).
   * Präsentationsmodus (`☕ System wachhalten`).

---

## 🚀 Installation

### Voraussetzungen (Arch / EndeavourOS)
```bash
sudo pacman -S power-profiles-daemon python-pyqt6 python-dbus nvidia-prime switcheroo-control iw
yay -S supergfxctl
sudo systemctl enable --now supergfxd switcheroo-control
```

### Option A: Arch-Linux-Paket (`makepkg`) — Empfohlen
```bash
git clone https://github.com/Gigoslav/rog-power-manager.git
cd rog-power-manager
makepkg -si
```

### Option B: Benutzer- & System-Helper-Installation
```bash
git clone https://github.com/Gigoslav/rog-power-manager.git
cd rog-power-manager
./install.sh
```

---

## ⚙️ Einstellungen & Automationen

Über das Tray-Menü (**⚙️ Einstellungen & Automationen...**) öffnet sich ein modernes Konfigurationsfenster:

1. **⚡ Profile & Hardware:** Individuelle Festlegung von Energieprofil (`power-saver`, `balanced`, `performance`), Lüftermodus (`auto`, `silent`, `balanced`, `turbo`, `max`), Bildwiederholrate (`60 Hz` / `240 Hz`), Display-Helligkeit (`10-100%`) und Tastaturbeleuchtung pro Stromquelle (`battery`, `usbc`, `ac`).
2. **⏱️ Inaktivität & Display:** Einstellbare Timeouts für Inaktivitäts-Abdunkelung (Dimming), Bildschirm-Ausschaltung (DPMS) und automatischen Standby.
3. **🚀 Automationen & Hooks:**
   - Eigene Shell-Befehle bei Statuswechsel (`battery`, `usbc`, `ac`) und bei schwachem Akku (`on_low_command`).
   - Shell-Hooks im Verzeichnis `~/.config/rog-power-manager/hooks.d/`:
     - `on_state_change <neuer_status> <alter_status>`
     - `battery_low <kapazität>`
     - `battery_critical <kapazität>`
   - Konfigurationsdatei liegt menschenlesbar unter `~/.config/rog-power-manager/config.json`.

---

## 🛠️ Befehlszeile (CLI)

```bash
# Aktuelle Stromquelle, Energie-Bilanz, C-Rate, Lüfter-RPM & Bildschirmstatus:
rog-power-manager --status

# Lüftermodus direkt per CLI setzen:
rog-power-manager --fan silent
rog-power-manager --fan balanced
rog-power-manager --fan turbo
rog-power-manager --fan max
rog-power-manager --fan auto

# Sprache manuell erzwingen (Deutsch oder Englisch):
rog-power-manager --lang de --status
rog-power-manager --lang en --status

# Version, Autoren-Credits & Lizenz anzeigen:
rog-power-manager --version

# Passendes Profil & Bildwiederholrate einmalig anwenden und beenden:
rog-power-manager --once
```

*(Hinweis: Der bisherige Befehlsname `auto-power-profile.py` bleibt als Symlink weiterhin uneingeschränkt funktionsfähig.)*

---

## 👤 Autor, Credits & Lizenz

* **Autor & Maintainer:** **Marcel Berghaus ([Mountainhouse Software Solutions](https://mountain-house.dev) / [Gigoslav](https://github.com/Gigoslav))** — `<berghaus.marcel@gmail.com>`
* **Webseite:** **[https://mountain-house.dev](https://mountain-house.dev)**
* **Repository:** **[https://github.com/Gigoslav/rog-power-manager](https://github.com/Gigoslav/rog-power-manager)**
* **Lizenz:** Veröffentlicht unter der **GNU General Public License v3.0 oder höher (`GPL-3.0-or-later`)** — siehe [LICENSE](LICENSE) und [NOTICE](NOTICE).
* **Pflicht zur Namensnennung (GPLv3 § 7b):** Alle Kopien, Pakete, Forks oder abgeleiteten Werke **müssen** den ursprünglichen Copyright-Vermerk, die Autoren-Nennung (`Marcel Berghaus / Mountainhouse Software Solutions`), die Webseite (`https://mountain-house.dev`) sowie den Repository-Link beibehalten.
