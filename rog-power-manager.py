#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Website: https://lexchaos.de
# Project: https://github.com/Gigoslav/rog-auto-power
"""
ROG Auto-Power & Live Wattage Tray Indicator (rog-auto-power)
- AC (Barrel Plug):    Performance profile + 240 Hz
- USB-C PD Charger:    Balanced profile    + 60 Hz
- Battery (Unplugged): Power-saver profile + 60 Hz
Includes a live KDE System Tray icon showing Total IN, Battery IN/OUT (with C-rate),
System Watts, Battery Protection modes, and GPU/MUX switching.
Supports automatic German (de) and English (en) localization based on system locale.
"""

import os
import sys
import glob
import locale
import subprocess
import re
import argparse
import ctypes
import json
import copy

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from PyQt6 import sip
from PyQt6.QtCore import Qt, QObject, QTimer, QRect
from PyQt6.QtGui import QIcon, QPixmap, QPainter, QColor, QFont, QAction, QActionGroup
from PyQt6.QtWidgets import (
    QApplication, QSystemTrayIcon, QMenu, QDialog, QTabWidget, QWidget,
    QVBoxLayout, QHBoxLayout, QFormLayout, QGroupBox, QLabel, QLineEdit,
    QSpinBox, QComboBox, QPushButton
)

APP_NAME = "rog-power-manager"
APP_VERSION = "0.3.0"
APP_AUTHOR = "Marcel Berghaus (Gigoslav)"
APP_ORG = "Mountainhouse Software Solutions"
APP_EMAIL = "berghaus.marcel@gmail.com"
APP_WEBSITE = "https://mountain-house.dev"
APP_URL = "https://github.com/Gigoslav/rog-power-manager"
APP_LICENSE = "GPL-3.0-or-later"


def detect_language() -> str:
    """Detect whether system language is German ('de') or default to English ('en')."""
    for env_var in ("LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE"):
        val = os.environ.get(env_var, "")
        if val:
            return "de" if val.lower().startswith("de") else "en"
    try:
        loc = locale.getdefaultlocale()[0] or ""
        if loc.lower().startswith("de"):
            return "de"
    except Exception:
        pass
    return "en"


LANG = detect_language()

I18N = {
    "de": {
        "bat_label": "Akkubetrieb",
        "bat_title": "⚡ Akkubetrieb aktiviert",
        "bat_msg": "Stromsparmodus | 60 Hz | 50% Helligkeit | Eco-Modus",
        "usbc_label": "USB-C PD (Ladebetrieb)",
        "usbc_title": "🔌 USB-C Laden aktiviert",
        "usbc_msg": "Ausgeglichen | 60 Hz | 75% Helligkeit",
        "ac_label": "Netzteil 240W (Netzbetrieb)",
        "ac_title": "🚀 Netzbetrieb (240W) aktiviert",
        "ac_msg": "Höchstleistung | 240 Hz | 100% Helligkeit",
        "menu_header": "⚡ Aktuelle Energie-Bilanz",
        "auto_mode": "🔄 Automatik-Modus",
        "manual_title": "⚙️ Manuelles Profil",
        "menu_ps": "🌱 Stromsparmodus + 60 Hz",
        "menu_bal": "⚖️ Ausgeglichen + 60 Hz",
        "menu_perf": "🚀 Höchstleistung + 240 Hz",
        "gpu_active": "⚡ Aktiv",
        "gpu_sleep": "💤 0W Schlafmodus",
        "gpu_mux_nv_title": "🎮 GPU-Umschaltung (MUX -> NVIDIA)",
        "gpu_mux_nv_msg": "Hardware-MUX für nächsten Neustart auf dedizierte NVIDIA gestellt. Bitte neu starten!",
        "gpu_mux_prep_title": "🎮 GPU-Umschaltung (MUX -> {mode})",
        "gpu_mux_prep_msg": "Hardware-MUX auf {mode} vorbereitet. Bitte Laptop einmal neu starten!",
        "gpu_super_title": "🎮 GPU-Umschaltung (supergfxctl)",
        "gpu_super_msg": "GPU-Modus '{mode}' gesetzt. Bitte einmal ab- und wieder anmelden!",
        "gpu_err_title": "⚠️ GPU-Umschaltung Fehler",
        "gpu_int_menu": "🔋 GPU: Integriert (Nur AMD, 0W NVIDIA)",
        "gpu_hyb_menu": "⚡ GPU: Hybrid (AMD + NVIDIA für Spiele)",
        "gpu_ded_menu": "🔥 GPU: Dediziert (MUX Direkt NVIDIA)",
        "bat_prot_title": "🛡️ Akku-Schutzmodus",
        "bat_prot_msg": "{label} aktiviert (Lade-Limit: {limit}%)",
        "bat_prot_err": "⚠️ Akku-Schutz Fehler",
        "stop_now_msg": "Laden sofort gestoppt (0.0 C Direktbetrieb bei {cap}%)",
        "menu_lim_stop": "⏸️ Laden sofort stoppen (0.0 C Direktbetrieb)",
        "menu_lim_60": "🛡️ Ultra-Schutz: Max 60% (Schreibtisch)",
        "menu_lim_80": "⚖️ Standard-Schutz: Max 80% (Alltag)",
        "menu_lim_100": "🎒 Reise-Modus: Max 100% (Vollladung)",
        "lbl_lim_60": "Ultra-Schutz (60%)",
        "lbl_lim_80": "Standard-Schutz (80%)",
        "lbl_lim_100": "Reise-Modus (100%)",
        "eta_to_limit": "~{h:.1f}h bis {limit}%",
        "eta_almost": "Fast {limit}%",
        "eta_rem": "~{h:.1f}h Restlaufzeit",
        "no_charger": "Kein Netzteil",
        "line_in": "🔌 Eingang Gesamt:{total_in:5.1f} W  [{src_label}{usbc_extra}]",
        "line_in_none": "🔌 Eingang Gesamt:  0.0 W  [Kein Netzteil]",
        "line_bat_chg": "├── 🔋 Akku:     +{bat_w:5.1f} W  ({c_rate:.2f} C • Lädt • {cap}%/{limit}% • {eta})",
        "line_bat_dis": "├── 🔋 Akku:     -{bat_w:5.1f} W  ({c_rate:.2f} C • Entlädt • {cap}% • {eta})",
        "line_bat_byp": "├── 🔋 Akku:        0.0 W  (0.00 C • {cap}% • Direktbetrieb bei {limit}% Limit)",
        "line_sys_run": "└── 💻 System:    {sys_w:5.1f} W  (Laufender Betrieb)",
        "line_sys_bat": "└── 💻 System:    {sys_w:5.1f} W  (Aus dem Akku)",
        "line_sys_ac": "└── 💻 System:    {sys_w:5.1f} W  (Direkt vom Netzteil)",
        "line_detail": "       ↳ CPU/iGPU: {apu_w:.1f}W | RTX 3060: {dgpu_w:.1f}W | Bildschirm/Mainboard: {rest_w:.1f}W",
        "menu_profile": "⚙️ Profil: {prof} | {panel} @ {rate}",
        "menu_gpu": "🎮 GPU-Modus: {gpu}",
        "menu_bat_prot": "🛡️ Akku-Schutz: Limit {limit}% ({c_rate:.2f} C)",
        "tt_header": "⚡ Energie-Bilanz (Echtzeit)",
        "tt_limit": "🛡️ Akku-Limit: {limit}% | Laderate: {c_rate:.2f} C",
        "tt_profile": "⚙️ Profil: {prof} | Bildschirm: {rate}",
        "tt_gpu": "🎮 GPU: {gpu}",
        "tt_fan": "💨 Lüfter: {status} ({mode})",
        "cli_header": "=== ROG Power Manager & Energie-Bilanz ===",
        "cli_source": "Stromquelle:     {label}",
        "cli_total_in": "Eingang Gesamt:  {w:.2f} W",
        "cli_bat": "├── Davon Akku:  {sign}{w:.2f} W ({c:.2f} C) [{st}] (Stand: {cap}% / Limit: {lim}%)",
        "cli_sys": "└── Davon System:{w:6.2f} W (CPU/iGPU: {apu:.1f}W | RTX 3060: {dgpu:.1f}W [{dst}] | Bildschirm/Mainboard: {rest:.1f}W)",
        "cli_prof": "Aktives Profil:  {prof}",
        "cli_panel": "Bildschirm:      {panel} ({res}) @ {rate} Hz",
        "cli_fan": "Lüfter:          {status}",
        "menu_about": "ℹ️ About",
        "menu_settings": "⚙️ Einstellungen & Automationen...",
        "settings_title": "⚙️ ROG Power Manager — Einstellungen & Automationen",
        "tab_profiles": "⚡ Profile & Hardware",
        "tab_idle": "⏱️ Inaktivität & Display",
        "tab_automations": "🚀 Automationen & Hooks",
        "btn_save": "💾 Speichern & Anwenden",
        "btn_cancel": "Abbrechen",
        "btn_open_hooks": "📂 Hooks-Ordner öffnen (~/.config/rog-power-manager/hooks.d)",
        "about_win_title": "Über ROG Power Manager",
        "about_dev_group": "Entwickler & Organisation",
        "about_sys_group": "Live System-Status",
        "about_author": "Entwickler",
        "about_org": "Organisation",
        "about_web": "Webseite",
        "about_repo": "GitHub Repository",
        "about_license": "Lizenz",
        "about_fans": "Lüfter (CPU / GPU)",
        "about_close": "Schließen",
        "about_title": "⚡ ROG Power Manager v{ver}",
        "about_msg": "Entwickelt von {author} ({org}) • Web: {web} • Lizenz: {lic} • {url}",
        "ver_free": "Dies ist freie Software: Sie können sie unter den Bedingungen der GPLv3+ ändern und weitergeben.",
        "ver_attr": "Autoren-Credits (Marcel Berghaus / Mountainhouse Software Solutions • https://mountain-house.dev) müssen in allen Kopien erhalten bleiben.",
        "prof_power-saver": "Stromsparmodus",
        "prof_balanced": "Ausgeglichen",
        "prof_performance": "Höchstleistung",
        "st_Charging": "Lädt",
        "st_Discharging": "Entlädt",
        "st_Not charging": "Direktbetrieb / Lädt nicht",
        "st_Full": "Voll",
        "dst_suspended": "Schlafmodus",
        "dst_active": "Aktiv",
        "menu_inhibit": "☕ System wachhalten (Präsentationsmodus)",
        "menu_fan": "💨 Lüfter: {status}",
        "fan_mode_auto": "🔄 Auto (Profilabhängig)",
        "fan_mode_silent": "🔇 Silent (Flüsterleise)",
        "fan_mode_balanced": "⚖️ Balanced (Ausgeglichen)",
        "fan_mode_turbo": "🚀 Turbo (Hohe Kühlung)",
        "fan_mode_max": "🌪️ Max (100% Volllast)",
        "fan_status_off": "Aus (0 RPM)",
        "fan_status_active": "CPU {cpu} | GPU {gpu} RPM",
        "fan_set_title": "💨 Lüftermodus geändert",
        "fan_set_msg": "Lüftermodus auf '{mode}' gesetzt.",
        "menu_brightness": "🔆 Display-Helligkeit",
        "menu_kbd": "⌨️ Tastaturbeleuchtung",
        "menu_lock": "🔒 Bildschirm sperren",
        "menu_suspend": "💤 Energiesparmodus (Standby)",
        "menu_hibernate": "🌙 Ruhezustand (Hibernate)",
        "inhibit_on_title": "☕ System wird wachgehalten",
        "inhibit_on_msg": "Display-Abschaltung und Standby vorübergehend deaktiviert.",
        "inhibit_off_title": "☕ Normaler Energiemodus",
        "inhibit_off_msg": "Automatisches Abdunkeln und Standby wieder aktiv.",
        "bat_low_title": "Akku schwach",
        "bat_low_msg": "Akkustand beträgt nur noch {cap}%. Bitte Ladegerät anschließen.",
        "bat_crit_title": "Akku kritisch leer!",
        "bat_crit_msg": "Akkustand beträgt nur {cap}%. System wird in Kürze in den Standby versetzt!",
    },
    "en": {
        "bat_label": "Battery (Unplugged)",
        "bat_title": "⚡ Battery Mode Activated",
        "bat_msg": "Power-Saver | 60 Hz | 50% Brightness | Eco-Mode",
        "usbc_label": "USB-C PD (Charging)",
        "usbc_title": "🔌 USB-C Charging Activated",
        "usbc_msg": "Balanced | 60 Hz | 75% Brightness",
        "ac_label": "AC 240W (Plugged In)",
        "ac_title": "🚀 AC Mode (240W) Activated",
        "ac_msg": "Performance | 240 Hz | 100% Brightness",
        "menu_header": "⚡ Live Energy Balance",
        "auto_mode": "🔄 Automatic Mode",
        "manual_title": "⚙️ Manual Profile",
        "menu_ps": "🌱 Power-Saver + 60 Hz",
        "menu_bal": "⚖️ Balanced + 60 Hz",
        "menu_perf": "🚀 Performance + 240 Hz",
        "gpu_active": "⚡ Active",
        "gpu_sleep": "💤 0W Suspended",
        "gpu_mux_nv_title": "🎮 GPU Switch (MUX -> NVIDIA)",
        "gpu_mux_nv_msg": "Hardware MUX set to Dedicated NVIDIA for next boot. Please reboot!",
        "gpu_mux_prep_title": "🎮 GPU Switch (MUX -> {mode})",
        "gpu_mux_prep_msg": "Hardware MUX prepared for {mode}. Please reboot your laptop!",
        "gpu_super_title": "🎮 GPU Switch (supergfxctl)",
        "gpu_super_msg": "GPU mode '{mode}' set. Please log out and log back in!",
        "gpu_err_title": "⚠️ GPU Switch Error",
        "gpu_int_menu": "🔋 GPU: Integrated (AMD Only, 0W NVIDIA)",
        "gpu_hyb_menu": "⚡ GPU: Hybrid (AMD + NVIDIA for Games)",
        "gpu_ded_menu": "🔥 GPU: Dedicated (Direct NVIDIA MUX)",
        "bat_prot_title": "🛡️ Battery Protection Mode",
        "bat_prot_msg": "{label} enabled (Charge Limit: {limit}%)",
        "bat_prot_err": "⚠️ Battery Protection Error",
        "stop_now_msg": "Charging stopped immediately (0.0 C Bypass at {cap}%)",
        "menu_lim_stop": "⏸️ Stop Charging Now (0.0 C Bypass)",
        "menu_lim_60": "🛡️ Ultra Protection: Max 60% (Desk Mode)",
        "menu_lim_80": "⚖️ Standard Protection: Max 80% (Daily)",
        "menu_lim_100": "🎒 Travel Mode: Max 100% (Full Charge)",
        "lbl_lim_60": "Ultra Protection (60%)",
        "lbl_lim_80": "Standard Protection (80%)",
        "lbl_lim_100": "Travel Mode (100%)",
        "eta_to_limit": "~{h:.1f}h to {limit}%",
        "eta_almost": "Almost {limit}%",
        "eta_rem": "~{h:.1f}h remaining",
        "no_charger": "Unplugged",
        "line_in": "🔌 Total IN:    {total_in:5.1f} W  [{src_label}{usbc_extra}]",
        "line_in_none": "🔌 Total IN:      0.0 W  [Unplugged]",
        "line_bat_chg": "├── 🔋 Battery: +{bat_w:5.1f} W  ({c_rate:.2f} C • Charging • {cap}%/{limit}% • {eta})",
        "line_bat_dis": "├── 🔋 Battery: -{bat_w:5.1f} W  ({c_rate:.2f} C • Discharging • {cap}% • {eta})",
        "line_bat_byp": "├── 🔋 Battery:    0.0 W  (0.00 C • {cap}% • Bypass at {limit}% Limit)",
        "line_sys_run": "└── 💻 System:  {sys_w:5.1f} W  (Active Load)",
        "line_sys_bat": "└── 💻 System:  {sys_w:5.1f} W  (From Battery)",
        "line_sys_ac": "└── 💻 System:  {sys_w:5.1f} W  (Direct AC Bypass)",
        "line_detail": "       ↳ CPU/iGPU: {apu_w:.1f}W | RTX 3060: {dgpu_w:.1f}W | Display/Board: {rest_w:.1f}W",
        "menu_profile": "⚙️ Profile: {prof} | {panel} @ {rate}",
        "menu_gpu": "🎮 GPU Mode: {gpu}",
        "menu_bat_prot": "🛡️ Battery Limit: {limit}% ({c_rate:.2f} C)",
        "tt_header": "⚡ Energy Balance (Live)",
        "tt_limit": "🛡️ Charge Limit: {limit}% | Rate: {c_rate:.2f} C",
        "tt_profile": "⚙️ Profile: {prof} | Display: {rate}",
        "tt_gpu": "🎮 GPU: {gpu}",
        "tt_fan": "💨 Fans: {status} ({mode})",
        "cli_header": "=== ROG Power Manager & Energy Balance ===",
        "cli_source": "Power Source:    {label}",
        "cli_total_in": "Total IN:        {w:.2f} W",
        "cli_bat": "├── Battery:     {sign}{w:.2f} W ({c:.2f} C) [{st}] (Level: {cap}% / Limit: {lim}%)",
        "cli_sys": "└── System Load: {w:6.2f} W (CPU/iGPU: {apu:.1f}W | RTX 3060: {dgpu:.1f}W [{dst}] | Display/Board: {rest:.1f}W)",
        "cli_prof": "Active Profile:  {prof}",
        "cli_panel": "Display:         {panel} ({res}) @ {rate} Hz",
        "cli_fan": "Fans:            {status}",
        "menu_about": "ℹ️ About",
        "menu_settings": "⚙️ Settings & Automations...",
        "settings_title": "⚙️ ROG Power Manager — Settings & Automations",
        "tab_profiles": "⚡ Profiles & Hardware",
        "tab_idle": "⏱️ Inactivity & Display",
        "tab_automations": "🚀 Automations & Hooks",
        "btn_save": "💾 Save & Apply",
        "btn_cancel": "Cancel",
        "btn_open_hooks": "📂 Open Hooks Folder (~/.config/rog-power-manager/hooks.d)",
        "about_win_title": "About ROG Power Manager",
        "about_dev_group": "Developer & Organization",
        "about_sys_group": "Live System Status",
        "about_author": "Developer",
        "about_org": "Organization",
        "about_web": "Website",
        "about_repo": "GitHub Repository",
        "about_license": "License",
        "about_fans": "Fans (CPU / GPU)",
        "about_close": "Close",
        "about_title": "⚡ ROG Power Manager v{ver}",
        "about_msg": "Created by {author} ({org}) • Web: {web} • License: {lic} • {url}",
        "ver_free": "This is free software: you are free to change and redistribute it under the terms of the GPLv3+.",
        "ver_attr": "Author attribution (Marcel Berghaus / Mountainhouse Software Solutions • https://mountain-house.dev) must be preserved in all copies.",
        "prof_power-saver": "Power-Saver",
        "prof_balanced": "Balanced",
        "prof_performance": "Performance",
        "st_Charging": "Charging",
        "st_Discharging": "Discharging",
        "st_Not charging": "Not charging (AC Bypass)",
        "st_Full": "Full",
        "dst_suspended": "suspended",
        "dst_active": "active",
        "menu_inhibit": "☕ Keep System Awake (Presentation Mode)",
        "menu_fan": "💨 Fans: {status}",
        "fan_mode_auto": "🔄 Auto (Profile-based)",
        "fan_mode_silent": "🔇 Silent (Quiet)",
        "fan_mode_balanced": "⚖️ Balanced (Standard)",
        "fan_mode_turbo": "🚀 Turbo (High Cooling)",
        "fan_mode_max": "🌪️ Max (100% Full Speed)",
        "fan_status_off": "Off (0 RPM)",
        "fan_status_active": "CPU {cpu} | GPU {gpu} RPM",
        "fan_set_title": "💨 Fan Mode Changed",
        "fan_set_msg": "Fan mode set to '{mode}'.",
        "menu_brightness": "🔆 Display Brightness",
        "menu_kbd": "⌨️ Keyboard Backlight",
        "menu_lock": "🔒 Lock Screen",
        "menu_suspend": "💤 Sleep (Suspend)",
        "menu_hibernate": "🌙 Hibernate",
        "inhibit_on_title": "☕ Keeping System Awake",
        "inhibit_on_msg": "Display blanking and auto-suspend temporarily disabled.",
        "inhibit_off_title": "☕ Normal Power Mode",
        "inhibit_off_msg": "Automatic dimming and sleep re-enabled.",
        "bat_low_title": "Battery Low",
        "bat_low_msg": "Battery level is down to {cap}%. Please connect a charger.",
        "bat_crit_title": "Battery Critically Low!",
        "bat_crit_msg": "Battery level is at {cap}%. System will suspend shortly to preserve data!",
    },
}


def tr(key: str, **kwargs) -> str:
    """Translate key using current language dictionary."""
    text = I18N.get(LANG, I18N["en"]).get(key, I18N["en"].get(key, key))
    return text.format(**kwargs) if kwargs else text


def get_power_source() -> str:
    """
    Determine current power source:
    - 'battery': running on battery
    - 'usbc':    charging via USB-C Power Delivery
    - 'ac':      charging via full AC barrel adapter
    """
    power_supply_dir = "/sys/class/power_supply"
    usbc_online = False
    ac_online = False

    if os.path.isdir(power_supply_dir):
        for name in os.listdir(power_supply_dir):
            online_file = os.path.join(power_supply_dir, name, "online")
            if not os.path.exists(online_file):
                continue
            try:
                with open(online_file, "r") as f:
                    val = f.read().strip()
            except Exception:
                continue

            if name.startswith("ucsi-source-psy") and val == "1":
                usbc_online = True
            elif name.startswith(("AC", "ADP", "acad")) and val == "1":
                ac_online = True

    charge_mode_path = "/sys/devices/platform/asus-nb-wmi/charge_mode"
    if ac_online and os.path.exists(charge_mode_path):
        try:
            with open(charge_mode_path, "r") as f:
                cm = f.read().strip()
                if cm == "2":
                    usbc_online = True
        except Exception:
            pass

    if usbc_online:
        return "usbc"
    if ac_online:
        return "ac"

    try:
        bus = dbus.SystemBus()
        up_obj = bus.get_object("org.freedesktop.UPower", "/org/freedesktop/UPower")
        props = dbus.Interface(up_obj, "org.freedesktop.DBus.Properties")
        on_batt = bool(props.Get("org.freedesktop.UPower", "OnBattery"))
        return "battery" if on_batt else "ac"
    except Exception:
        return "battery"


def get_apu_ppt_watts() -> float:
    """Read AMD Ryzen APU (CPU + Radeon 680M iGPU) package power in Watts from hwmon."""
    for h in glob.glob("/sys/class/hwmon/hwmon*"):
        try:
            name_file = os.path.join(h, "name")
            if os.path.exists(name_file):
                with open(name_file, "r") as f:
                    if f.read().strip() == "amdgpu":
                        p_file = os.path.join(h, "power1_input")
                        if os.path.exists(p_file):
                            with open(p_file, "r") as pf:
                                return int(pf.read().strip()) / 1_000_000.0
        except Exception:
            pass
    return 0.0


def get_dgpu_status_and_watts() -> tuple[str, float]:
    """
    Check NVIDIA dGPU runtime PM state via sysfs ONLY.
    IMPORTANT: Never call nvidia-smi in a polling loop, as nvidia-smi resets the
    kernel runtime PM autosuspend timer and prevents the GPU from entering D3cold (0W)!
    """
    pm_path = "/sys/bus/pci/devices/0000:01:00.0/power/runtime_status"
    pm_state = "unknown"
    if os.path.exists(pm_path):
        try:
            with open(pm_path, "r") as f:
                pm_state = f.read().strip()
        except Exception:
            pass

    if pm_state == "suspended":
        return "suspended", 0.0

    return pm_state, 10.0 if pm_state == "active" else 0.0


def get_raw_brightness() -> int:
    for bl_dir in glob.glob("/sys/class/backlight/*"):
        try:
            b_file = os.path.join(bl_dir, "brightness")
            if os.path.exists(b_file):
                with open(b_file, "r") as f:
                    return int(f.read().strip())
        except Exception:
            pass
    return 32768


def get_max_brightness() -> int:
    for bl_dir in glob.glob("/sys/class/backlight/*"):
        try:
            m_file = os.path.join(bl_dir, "max_brightness")
            if os.path.exists(m_file):
                with open(m_file, "r") as f:
                    return int(f.read().strip())
        except Exception:
            pass
    return 65535


def set_raw_brightness(val: int):
    val = max(100, min(get_max_brightness(), int(val)))
    for bl_dir in glob.glob("/sys/class/backlight/*"):
        try:
            b_file = os.path.join(bl_dir, "brightness")
            with open(b_file, "w") as f:
                f.write(f"{val}\n")
        except Exception:
            pass


def set_brightness_pct(screen_pct: int):
    mx = get_max_brightness()
    target_val = int(round((screen_pct / 100.0) * mx))
    set_raw_brightness(target_val)


def get_kbd_brightness() -> int:
    for k_dir in glob.glob("/sys/class/leds/*kbd_backlight*"):
        try:
            b_file = os.path.join(k_dir, "brightness")
            if os.path.exists(b_file):
                with open(b_file, "r") as f:
                    return int(f.read().strip())
        except Exception:
            pass
    return 1


def set_kbd_brightness(level: int):
    level = max(0, min(3, int(level)))
    for k_dir in glob.glob("/sys/class/leds/*kbd_backlight*"):
        try:
            b_file = os.path.join(k_dir, "brightness")
            with open(b_file, "w") as f:
                f.write(f"{level}\n")
        except Exception:
            pass


def get_backlight_ratio() -> float:
    """Get screen brightness ratio (0.0 to 1.0) directly from sysfs."""
    cur = get_raw_brightness()
    mx = get_max_brightness()
    if mx > 0:
        return max(0.0, min(1.0, float(cur) / float(mx)))
    return 0.5


def set_screen_and_kbd_brightness(screen_pct: int, kbd_level: int):
    """Set screen brightness (0-100%) and keyboard backlight (0-3) directly via sysfs."""
    set_brightness_pct(screen_pct)
    set_kbd_brightness(kbd_level)


def apply_hardware_tunings(state: str):
    """Set CPU boost, Panel Overdrive, and PCIe Runtime PM directly via sysfs without sudo."""
    # 1. Panel Overdrive (0 on battery/usbc, 1 on ac)
    panel_od_file = "/sys/devices/platform/asus-nb-wmi/panel_od"
    if os.path.exists(panel_od_file):
        try:
            with open(panel_od_file, "w") as f:
                f.write("1\n" if state == "ac" else "0\n")
        except Exception:
            pass

    # 2. CPU Boost (0 on battery, 1 on usbc/ac)
    boost_file = "/sys/devices/system/cpu/cpufreq/boost"
    if os.path.exists(boost_file):
        try:
            with open(boost_file, "w") as f:
                f.write("0\n" if state == "battery" else "1\n")
        except Exception:
            pass

    # 3. PCIe Runtime PM (auto)
    for dev in ("0000:02:00.0", "0000:03:00.0", "0000:04:00.0", "0000:05:00.0", "0000:06:00.0", "0000:06:00.2"):
        pci_file = f"/sys/bus/pci/devices/{dev}/power/control"
        if os.path.exists(pci_file):
            try:
                with open(pci_file, "w") as f:
                    f.write("auto\n")
            except Exception:
                pass

    # 4. Optional background tunables via helper if available (Wi-Fi power-save & vm writeback)
    if os.path.exists("/usr/local/bin/rog-gpu-mux-helper"):
        try:
            subprocess.Popen(
                ["sudo", "-n", "/usr/local/bin/rog-gpu-mux-helper", f"power-{state}"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
        except Exception:
            pass


# ==============================================================================
# PowerDevil Replacement Services (D-Bus & Idle Management)
# ==============================================================================

class FreedesktopPowerManagementService(dbus.service.Object):
    """Provides org.freedesktop.PowerManagement and Inhibit interfaces."""
    def __init__(self, bus_name):
        super().__init__(bus_name, "/org/freedesktop/PowerManagement")
        self.inhibitions = {}
        self._cookie_counter = 1000
        self.manual_inhibit = False

    @dbus.service.method("org.freedesktop.PowerManagement", out_signature="b")
    def CanSuspend(self):
        return True

    @dbus.service.method("org.freedesktop.PowerManagement", out_signature="b")
    def CanHibernate(self):
        return True

    @dbus.service.method("org.freedesktop.PowerManagement", out_signature="b")
    def CanHybridSuspend(self):
        return False

    @dbus.service.method("org.freedesktop.PowerManagement", out_signature="b")
    def CanSuspendThenHibernate(self):
        return False

    @dbus.service.method("org.freedesktop.PowerManagement")
    def Suspend(self):
        subprocess.Popen(["loginctl", "suspend"])

    @dbus.service.method("org.freedesktop.PowerManagement")
    def Hibernate(self):
        subprocess.Popen(["loginctl", "hibernate"])

    @dbus.service.method("org.freedesktop.PowerManagement", out_signature="b")
    def GetPowerSaveStatus(self):
        return get_power_source() == "battery"

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", in_signature="ss", out_signature="u")
    def Inhibit(self, application, reason):
        cookie = self._cookie_counter
        self._cookie_counter += 1
        self.inhibitions[cookie] = (str(application), str(reason))
        print(f"[PowerManagement] Registered Inhibit from '{application}': {reason} (Cookie {cookie})")
        return cookie

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", in_signature="u")
    def UnInhibit(self, cookie):
        if cookie in self.inhibitions:
            app, reason = self.inhibitions.pop(cookie)
            print(f"[PowerManagement] Released Inhibit for '{app}' (Cookie {cookie})")

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", out_signature="b")
    def HasInhibit(self):
        return len(self.inhibitions) > 0 or self.manual_inhibit


class FreedesktopInhibitService(dbus.service.Object):
    """Provides org.freedesktop.PowerManagement.Inhibit at /org/freedesktop/PowerManagement/Inhibit."""
    def __init__(self, bus_name, pm_service):
        super().__init__(bus_name, "/org/freedesktop/PowerManagement/Inhibit")
        self.pm_service = pm_service

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", in_signature="ss", out_signature="u")
    def Inhibit(self, application, reason):
        return self.pm_service.Inhibit(application, reason)

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", in_signature="u")
    def UnInhibit(self, cookie):
        self.pm_service.UnInhibit(cookie)

    @dbus.service.method("org.freedesktop.PowerManagement.Inhibit", out_signature="b")
    def HasInhibit(self):
        return self.pm_service.HasInhibit()


class KDEBrightnessControlService(dbus.service.Object):
    """Provides org.kde.Solid.PowerManagement.Actions.BrightnessControl interface."""
    def __init__(self, bus_name):
        super().__init__(bus_name, "/org/kde/Solid/PowerManagement/Actions/BrightnessControl")

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", out_signature="i")
    def brightness(self):
        return get_raw_brightness()

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", out_signature="i")
    def brightnessMax(self):
        return get_max_brightness()

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", out_signature="i")
    def brightnessMin(self):
        return 0

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", out_signature="i")
    def brightnessSteps(self):
        return 20

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", in_signature="i")
    def setBrightness(self, val):
        set_raw_brightness(val)

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.BrightnessControl", in_signature="i")
    def setBrightnessSilent(self, val):
        set_raw_brightness(val)


class KDEKeyboardBrightnessControlService(dbus.service.Object):
    """Provides org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl interface."""
    def __init__(self, bus_name):
        super().__init__(bus_name, "/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl")

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl", out_signature="i")
    def keyboardBrightness(self):
        return get_kbd_brightness()

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl", out_signature="i")
    def keyboardBrightnessMax(self):
        return 3

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl", out_signature="i")
    def keyboardBrightnessSteps(self):
        return 3

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl", in_signature="i")
    def setKeyboardBrightness(self, val):
        set_kbd_brightness(val)

    @dbus.service.method("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl", in_signature="i")
    def setKeyboardBrightnessSilent(self, val):
        set_kbd_brightness(val)


class KDEPolicyAgentService(dbus.service.Object):
    """Provides org.kde.Solid.PowerManagement.PolicyAgent interface."""
    def __init__(self, bus_name, pm_service):
        super().__init__(bus_name, "/org/kde/Solid/PowerManagement/PolicyAgent")
        self.pm_service = pm_service

    @dbus.service.method("org.kde.Solid.PowerManagement.PolicyAgent", in_signature="u", out_signature="b")
    def HasInhibition(self, types):
        return self.pm_service.HasInhibit()

    @dbus.service.method("org.kde.Solid.PowerManagement.PolicyAgent", in_signature="uss", out_signature="u")
    def AddInhibition(self, types, app_name, reason):
        return self.pm_service.Inhibit(app_name, reason)

    @dbus.service.method("org.kde.Solid.PowerManagement.PolicyAgent", in_signature="u")
    def ReleaseInhibition(self, cookie):
        self.pm_service.UnInhibit(cookie)


class IdleManager:
    """Manages display dimming, DPMS-off, and auto-suspend based on user idle time via KIdleTime."""
    def __init__(self, has_inhibit_callback):
        self.has_inhibit = has_inhibit_callback
        self.is_dimmed = False
        self.is_dpms_off = False
        self.saved_brightness = None
        self.current_state = "ac"

        # Timeouts in seconds for (dim, dpms_off, suspend)
        self.timeouts = {
            "battery": {"dim": 180, "off": 300, "suspend": 900},   # 3m, 5m, 15m
            "usbc":    {"dim": 300, "off": 600, "suspend": 1800},  # 5m, 10m, 30m
            "ac":      {"dim": 600, "off": 900, "suspend": 0},     # 10m, 15m, never
        }
        self.timeout_actions = {}
        self.available = False
        self._init_kidletime()

    def _init_kidletime(self):
        try:
            self.lib = ctypes.CDLL("libKF6IdleTime.so.6")
            fn = getattr(self.lib, "_ZN9KIdleTime8instanceEv")
            fn.restype = ctypes.c_void_p
            self.inst = fn()
            self.qobj = sip.wrapinstance(self.inst, QObject)

            self._add_to = getattr(self.lib, "_ZN9KIdleTime14addIdleTimeoutEi")
            self._add_to.argtypes = [ctypes.c_void_p, ctypes.c_int]
            self._add_to.restype = ctypes.c_int

            self._remove_all = getattr(self.lib, "_ZN9KIdleTime21removeAllIdleTimeoutsEv")
            self._remove_all.argtypes = [ctypes.c_void_p]

            self._catch_resume = getattr(self.lib, "_ZN9KIdleTime20catchNextResumeEventEv")
            self._catch_resume.argtypes = [ctypes.c_void_p]

            self.qobj.timeoutReached.connect(self._on_timeout)
            self.qobj.resumingFromIdle.connect(self._on_resume)
            self.available = True
            print("[IdleManager] KIdleTime Wayland/X11 idle listener initialized.")
        except Exception as e:
            print(f"[IdleManager] KIdleTime not available: {e}")

    def update_state(self, state: str):
        self.current_state = state
        self.rearm()

    def rearm(self):
        if not self.available:
            return
        try:
            self._remove_all(self.inst)
            self.timeout_actions.clear()

            idle_cfg = CONFIG_MGR.data.get("idle", {})
            dim_min = idle_cfg.get(f"dim_{self.current_state}_min", 3 if self.current_state == "battery" else (5 if self.current_state == "usbc" else 10))
            off_min = idle_cfg.get(f"off_{self.current_state}_min", 5 if self.current_state == "battery" else (10 if self.current_state == "usbc" else 15))
            sus_min = idle_cfg.get(f"suspend_{self.current_state}_min", 15 if self.current_state == "battery" else (30 if self.current_state == "usbc" else 0))

            if dim_min > 0:
                tid = self._add_to(self.inst, int(dim_min * 60 * 1000))
                self.timeout_actions[tid] = "dim"
            if off_min > 0:
                tid = self._add_to(self.inst, int(off_min * 60 * 1000))
                self.timeout_actions[tid] = "off"
            if sus_min > 0:
                tid = self._add_to(self.inst, int(sus_min * 60 * 1000))
                self.timeout_actions[tid] = "suspend"
        except Exception as e:
            print(f"[IdleManager] Error setting timeouts: {e}")

    def _on_timeout(self, tid: int, ms: int):
        action = self.timeout_actions.get(tid)
        if not action:
            return

        # Check inhibition
        if self.has_inhibit():
            return

        if action == "dim" and not self.is_dimmed and not self.is_dpms_off:
            self.saved_brightness = get_raw_brightness()
            dim_val = max(100, int(self.saved_brightness * 0.25))
            set_raw_brightness(dim_val)
            self.is_dimmed = True
            self._catch_resume(self.inst)
            print("[IdleManager] Inactivity: Display dimmed to 25%")

        elif action == "off" and not self.is_dpms_off:
            subprocess.Popen(["kscreen-doctor", "--dpms", "off"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.is_dpms_off = True
            self._catch_resume(self.inst)
            print("[IdleManager] Inactivity: Display DPMS turned off")

        elif action == "suspend":
            print("[IdleManager] Inactivity: Triggering auto-suspend")
            subprocess.Popen(["loginctl", "suspend"])

    def _on_resume(self):
        if self.is_dpms_off:
            subprocess.Popen(["kscreen-doctor", "--dpms", "on"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.is_dpms_off = False
            print("[IdleManager] User resumed: Display turned on")

        if self.is_dimmed:
            if self.saved_brightness is not None:
                set_raw_brightness(self.saved_brightness)
                self.saved_brightness = None
            self.is_dimmed = False
            print("[IdleManager] User resumed: Brightness restored")


def get_power_telemetry(current_hz: float = 60.0) -> dict:
    """
    Compute complete power telemetry:
    - total_in_watts: Total power coming from charger (0.0 on battery)
    - bat_watts:      Power going into (+) or out of (-) the battery
    - sys_watts:      Power consumed by the laptop system itself
    - apu_watts:      AMD CPU + iGPU package power
    - dgpu_watts:     NVIDIA RTX 3060 power (0.0W when suspended)
    - rest_watts:     Display + Mainboard + NVMe + Wi-Fi
    """
    bat_dir = "/sys/class/power_supply/BAT0"
    status = "Unknown"
    bat_watts = 0.0
    capacity = 0
    energy_now_wh = 0.0
    energy_full_wh = 0.0
    usbc_amps = 0.0
    charge_limit = 80

    try:
        with open(os.path.join(bat_dir, "status"), "r") as f:
            status = f.read().strip()
        with open(os.path.join(bat_dir, "power_now"), "r") as f:
            bat_watts = int(f.read().strip()) / 1_000_000.0
        with open(os.path.join(bat_dir, "capacity"), "r") as f:
            capacity = int(f.read().strip())
        with open(os.path.join(bat_dir, "energy_now"), "r") as f:
            energy_now_wh = int(f.read().strip()) / 1_000_000.0
        with open(os.path.join(bat_dir, "energy_full"), "r") as f:
            energy_full_wh = int(f.read().strip()) / 1_000_000.0
        limit_file = os.path.join(bat_dir, "charge_control_end_threshold")
        if os.path.exists(limit_file):
            with open(limit_file, "r") as f:
                charge_limit = int(f.read().strip())
    except Exception:
        pass

    usbc_curr = "/sys/class/power_supply/ucsi-source-psy-USBC000:001/current_now"
    if os.path.exists(usbc_curr):
        try:
            with open(usbc_curr, "r") as f:
                usbc_amps = int(f.read().strip()) / 1_000_000.0
        except Exception:
            pass

    apu_watts = get_apu_ppt_watts()
    dgpu_state, dgpu_watts = get_dgpu_status_and_watts()

    bl_ratio = get_backlight_ratio()
    hz_extra = 2.2 if (current_hz and current_hz > 100) else 0.0
    estimated_rest = 3.2 + (3.8 * bl_ratio) + hz_extra

    if status == "Discharging":
        sys_watts = bat_watts
        rest_watts = max(0.5, sys_watts - apu_watts - dgpu_watts)
        total_in_watts = 0.0
    elif status == "Charging":
        rest_watts = estimated_rest
        sys_watts = apu_watts + dgpu_watts + rest_watts
        total_in_watts = bat_watts + sys_watts
    else:
        rest_watts = estimated_rest
        sys_watts = apu_watts + dgpu_watts + rest_watts
        bat_watts = 0.0
        total_in_watts = sys_watts

    c_rate = (bat_watts / energy_full_wh) if energy_full_wh > 0 else 0.0

    return {
        "status": status,
        "total_in_watts": total_in_watts,
        "bat_watts": bat_watts,
        "c_rate": c_rate,
        "charge_limit": charge_limit,
        "sys_watts": sys_watts,
        "apu_watts": apu_watts,
        "dgpu_state": dgpu_state,
        "dgpu_watts": dgpu_watts,
        "rest_watts": rest_watts,
        "capacity": capacity,
        "energy_now_wh": energy_now_wh,
        "energy_full_wh": energy_full_wh,
        "usbc_amps": usbc_amps,
        "usbc_max_watts": usbc_amps * 20.0,
    }


CURRENT_FAN_MODE = "auto"


def get_fan_telemetry() -> dict:
    """Reads live CPU and GPU fan RPM from ASUS WMI hwmon sysfs."""
    cpu_rpm = 0
    gpu_rpm = 0
    for hw in glob.glob("/sys/devices/platform/asus-nb-wmi/hwmon/hwmon*"):
        f1 = os.path.join(hw, "fan1_input")
        f2 = os.path.join(hw, "fan2_input")
        if os.path.isfile(f1):
            try:
                with open(f1, "r") as f:
                    cpu_rpm = int(f.read().strip())
            except Exception:
                pass
        if os.path.isfile(f2):
            try:
                with open(f2, "r") as f:
                    gpu_rpm = int(f.read().strip())
            except Exception:
                pass
    return {"cpu_rpm": cpu_rpm, "gpu_rpm": gpu_rpm}


def get_fan_mode() -> str:
    global CURRENT_FAN_MODE
    return CURRENT_FAN_MODE


def set_fan_mode(mode: str) -> bool:
    """
    Sets ASUS fan mode: 'auto', 'silent', 'balanced', 'turbo', 'max'.
    Controls ASUS WMI throttle_thermal_policy, ACPI platform_profile, and hwmon custom fan curves.
    """
    global CURRENT_FAN_MODE
    CURRENT_FAN_MODE = mode
    thermal_policy_file = "/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy"
    platform_profile_file = "/sys/firmware/acpi/platform_profile"
    hwmon_paths = glob.glob("/sys/devices/platform/asus-nb-wmi/hwmon/hwmon*")

    custom_hwmon = None
    for hw in hwmon_paths:
        name_file = os.path.join(hw, "name")
        if os.path.isfile(name_file):
            try:
                with open(name_file, "r") as f:
                    if f.read().strip() == "asus_custom_fan_curve":
                        custom_hwmon = hw
                        break
            except Exception:
                pass

    written = False
    try:
        if mode == "max":
            if os.path.isfile(thermal_policy_file):
                with open(thermal_policy_file, "w") as f:
                    f.write("1\n")
            if custom_hwmon:
                for i in range(1, 9):
                    p1 = os.path.join(custom_hwmon, f"pwm1_auto_point{i}_pwm")
                    p2 = os.path.join(custom_hwmon, f"pwm2_auto_point{i}_pwm")
                    if os.path.isfile(p1):
                        with open(p1, "w") as f: f.write("255\n")
                    if os.path.isfile(p2):
                        with open(p2, "w") as f: f.write("255\n")
                with open(os.path.join(custom_hwmon, "pwm1_enable"), "w") as f: f.write("1\n")
                with open(os.path.join(custom_hwmon, "pwm2_enable"), "w") as f: f.write("1\n")
            written = True
        elif mode == "silent":
            if custom_hwmon:
                with open(os.path.join(custom_hwmon, "pwm1_enable"), "w") as f: f.write("2\n")
                with open(os.path.join(custom_hwmon, "pwm2_enable"), "w") as f: f.write("2\n")
            if os.path.isfile(thermal_policy_file):
                with open(thermal_policy_file, "w") as f: f.write("2\n")
            if os.path.isfile(platform_profile_file):
                with open(platform_profile_file, "w") as f: f.write("quiet\n")
            written = True
        elif mode in ("balanced", "normal"):
            if custom_hwmon:
                with open(os.path.join(custom_hwmon, "pwm1_enable"), "w") as f: f.write("2\n")
                with open(os.path.join(custom_hwmon, "pwm2_enable"), "w") as f: f.write("2\n")
            if os.path.isfile(thermal_policy_file):
                with open(thermal_policy_file, "w") as f: f.write("0\n")
            if os.path.isfile(platform_profile_file):
                with open(platform_profile_file, "w") as f: f.write("balanced\n")
            written = True
        elif mode in ("turbo", "performance"):
            if custom_hwmon:
                with open(os.path.join(custom_hwmon, "pwm1_enable"), "w") as f: f.write("2\n")
                with open(os.path.join(custom_hwmon, "pwm2_enable"), "w") as f: f.write("2\n")
            if os.path.isfile(thermal_policy_file):
                with open(thermal_policy_file, "w") as f: f.write("1\n")
            if os.path.isfile(platform_profile_file):
                with open(platform_profile_file, "w") as f: f.write("performance\n")
            written = True
        elif mode == "auto":
            if custom_hwmon:
                with open(os.path.join(custom_hwmon, "pwm1_enable"), "w") as f: f.write("2\n")
                with open(os.path.join(custom_hwmon, "pwm2_enable"), "w") as f: f.write("2\n")
            written = True
    except (PermissionError, IOError):
        written = False
    except Exception as e:
        print(f"[FanControl] Sysfs write error: {e}")
        written = False

    if not written:
        for helper in ("/usr/local/bin/rog-power-helper", "/usr/local/bin/rog-gpu-mux-helper"):
            if os.path.isfile(helper):
                try:
                    subprocess.check_call(["sudo", "-n", helper, "fan-mode", mode])
                    written = True
                    break
                except Exception:
                    pass

    print(f"[FanControl] Fan mode applied: {mode} (Success: {written})")
    return written


def get_current_power_profile() -> str:
    """Get the currently active power profile (powerprofilesctl or ACPI platform_profile)."""
    try:
        res = subprocess.check_output(["powerprofilesctl", "get"], stderr=subprocess.DEVNULL)
        out = res.decode().strip()
        if out:
            return out
    except Exception:
        pass

    if os.path.isfile("/sys/firmware/acpi/platform_profile"):
        try:
            with open("/sys/firmware/acpi/platform_profile", "r") as f:
                prof = f.read().strip()
                if prof == "quiet": return "power-saver"
                if prof == "balanced": return "balanced"
                if prof == "performance": return "performance"
        except Exception:
            pass
    return "balanced"


def set_power_profile(profile: str) -> bool:
    """Set the power profile via powerprofilesctl, ACPI platform_profile and throttle_thermal_policy."""
    current = get_current_power_profile()
    if current == profile:
        return False

    success = False
    try:
        subprocess.check_call(["powerprofilesctl", "set", profile], stderr=subprocess.DEVNULL)
        success = True
    except Exception:
        pass

    plat_map = {"power-saver": "quiet", "balanced": "balanced", "performance": "performance"}
    therm_map = {"power-saver": "2", "balanced": "0", "performance": "1"}

    plat_val = plat_map.get(profile)
    therm_val = therm_map.get(profile)

    if plat_val and os.path.isfile("/sys/firmware/acpi/platform_profile"):
        try:
            with open("/sys/firmware/acpi/platform_profile", "w") as f:
                f.write(f"{plat_val}\n")
            success = True
        except Exception:
            pass

    if therm_val and os.path.isfile("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy"):
        try:
            with open("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", "w") as f:
                f.write(f"{therm_val}\n")
            success = True
        except Exception:
            pass

    if not success:
        for helper in ("/usr/local/bin/rog-power-helper", "/usr/local/bin/rog-gpu-mux-helper"):
            if os.path.isfile(helper):
                try:
                    subprocess.check_call(["sudo", "-n", helper, "set-profile", profile])
                    success = True
                    break
                except Exception:
                    pass

    if success:
        print(f"[PowerProfile] Profile switched: {current} -> {profile}")
    return success


def get_panel_info():
    """
    Get the internal panel name, current resolution, current rate, and available rates.
    Uses kscreen-doctor first, falls back to xrandr.
    """
    try:
        out = subprocess.check_output(["kscreen-doctor", "-o"], stderr=subprocess.DEVNULL).decode()
        for block in out.split("Output: "):
            if not block.strip():
                continue
            lines = block.splitlines()
            header = lines[0].split()
            output_name = header[1] if len(header) > 1 else header[0]
            is_panel = any("Panel" in l for l in lines)
            is_connected = "connected" in block and "enabled" in block

            if (is_panel or output_name.startswith(("eDP", "DP-2"))) and is_connected:
                current_rate = None
                available_rates = []
                current_res = "2560x1440"
                for l in lines:
                    if "Modes:" in l:
                        modes_matches = re.findall(r'(\d+x\d+)@(\d+(?:\.\d+)?)([\*\!]*)', l)
                        for res, rate_str, flags in modes_matches:
                            rate = float(rate_str)
                            available_rates.append((res, rate))
                            if "*" in flags:
                                current_rate = rate
                                current_res = res
                return output_name, current_res, current_rate, available_rates
    except Exception:
        pass

    try:
        out = subprocess.check_output(["xrandr", "--current"], stderr=subprocess.DEVNULL).decode()
        current_disp = None
        current_res = None
        current_rate = None
        available_rates = []
        is_active_panel = False

        for line in out.splitlines():
            m_disp = re.match(r"^(\S+)\s+connected.*", line)
            if m_disp:
                name = m_disp.group(1)
                if current_disp and is_active_panel:
                    break
                current_disp = name
                is_active_panel = name.startswith(("eDP", "DP-2", "DP-0")) or "primary" in line
                continue
            if current_disp and is_active_panel:
                m_mode = re.match(r"^\s+(\d+x\d+)\s+(.*)", line)
                if m_mode:
                    res = m_mode.group(1)
                    raw_rates = m_mode.group(2).split()
                    for r in raw_rates:
                        clean_r = r.replace("+", "").replace("*", "")
                        try:
                            rate_val = float(clean_r)
                            available_rates.append((res, rate_val))
                            if "*" in r:
                                current_rate = rate_val
                                current_res = res
                        except ValueError:
                            pass
        if current_disp:
            return current_disp, current_res or "2560x1440", current_rate, available_rates
    except Exception:
        pass

    return "eDP-1", "2560x1440", None, []


def set_display_refresh_rate(target_rate: int) -> bool:
    """Set the internal display refresh rate using kscreen-doctor or xrandr."""
    panel_name, current_res, current_rate, available_rates = get_panel_info()
    if not panel_name:
        return False

    if current_rate is not None and abs(current_rate - target_rate) < 1.0:
        return False

    res = current_res or "2560x1440"
    print(f"[Display] Changing {panel_name} ({res}) refresh rate: {current_rate}Hz -> {target_rate}Hz")

    try:
        mode_str = f"{res}@{target_rate}"
        cmd = ["kscreen-doctor", f"output.{panel_name}.mode.{mode_str}"]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception:
        pass

    try:
        cmd = ["xrandr", "--output", panel_name, "--mode", res, "--rate", f"{target_rate:.2f}"]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception as e:
        print(f"[Display] Error switching display rate with xrandr: {e}")
        return False


def send_notification(title: str, message: str, icon: str = "preferences-system-power-management"):
    """Send a desktop notification to the user."""
    try:
        subprocess.Popen(
            ["notify-send", "-u", "low", "-a", "AutoPower", "-i", icon, title, message],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
    except Exception:
        pass


class ConfigManager:
    """Manages user-customizable settings, timeouts and automations (~/.config/rog-power-manager/config.json)."""
    DEFAULT_CONFIG = {
        "fan_mode": "auto",
        "profiles": {
            "battery": {"profile": "power-saver", "hz": 60, "brightness": 50, "kbd": 1, "fan_mode": "silent", "command": ""},
            "usbc": {"profile": "balanced", "hz": 60, "brightness": 75, "kbd": 1, "fan_mode": "balanced", "command": ""},
            "ac": {"profile": "performance", "hz": 240, "brightness": 100, "kbd": 3, "fan_mode": "turbo", "command": ""},
        },
        "idle": {
            "dim_battery_min": 3,
            "dim_usbc_min": 5,
            "dim_ac_min": 10,
            "off_battery_min": 5,
            "off_usbc_min": 10,
            "off_ac_min": 15,
            "suspend_battery_min": 15,
            "suspend_usbc_min": 30,
            "suspend_ac_min": 0,
        },
        "alerts": {
            "low_battery_percent": 10,
            "critical_battery_percent": 5,
            "on_low_command": "",
            "on_critical_command": "",
        },
    }

    def __init__(self):
        self.dir = os.path.expanduser("~/.config/rog-power-manager")
        self.legacy_dir = os.path.expanduser("~/.config/rog-auto-power")
        self.path = os.path.join(self.dir, "config.json")
        self.hooks_dir = os.path.join(self.dir, "hooks.d")
        self.data = copy.deepcopy(self.DEFAULT_CONFIG)
        self.migrate_legacy()
        self.load()

    def migrate_legacy(self):
        if not os.path.exists(self.dir) and os.path.exists(self.legacy_dir):
            try:
                import shutil
                shutil.copytree(self.legacy_dir, self.dir)
                print(f"[Config] Migrated configuration from {self.legacy_dir} to {self.dir}")
            except Exception as e:
                print(f"[Config] Migration error: {e}")

    def load(self):
        os.makedirs(self.hooks_dir, exist_ok=True)
        if os.path.exists(self.path):
            try:
                with open(self.path, "r", encoding="utf-8") as f:
                    user_data = json.load(f)
                for section, vals in user_data.items():
                    if section in self.data and isinstance(vals, dict):
                        self.data[section].update(vals)
                    else:
                        self.data[section] = vals
            except Exception as e:
                print(f"[Config] Error loading {self.path}: {e}")
        else:
            self.save()

    def save(self):
        os.makedirs(self.dir, exist_ok=True)
        try:
            with open(self.path, "w", encoding="utf-8") as f:
                json.dump(self.data, f, indent=2, ensure_ascii=False)
            print(f"[Config] Saved to {self.path}")
        except Exception as e:
            print(f"[Config] Error saving {self.path}: {e}")

    def run_hook(self, hook_name: str, *args):
        hook_path = os.path.join(self.hooks_dir, hook_name)
        if os.path.isfile(hook_path) and os.access(hook_path, os.X_OK):
            try:
                subprocess.Popen([hook_path, *args], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                print(f"[Automation] Hook executed: {hook_name} {' '.join(args)}")
            except Exception as e:
                print(f"[Automation] Failed to run hook {hook_name}: {e}")

    def run_command(self, cmd: str, env_vars: dict = None):
        if cmd and cmd.strip():
            try:
                env = os.environ.copy()
                if env_vars:
                    env.update(env_vars)
                subprocess.Popen(cmd, shell=True, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                print(f"[Automation] Shell command executed: {cmd}")
            except Exception as e:
                print(f"[Automation] Failed to run command '{cmd}': {e}")


CONFIG_MGR = ConfigManager()


def get_state_config() -> dict:
    p = CONFIG_MGR.data["profiles"]
    return {
        "battery": {
            "profile": p.get("battery", {}).get("profile", "power-saver"),
            "hz": int(p.get("battery", {}).get("hz", 60)),
            "brightness": int(p.get("battery", {}).get("brightness", 50)),
            "kbd": int(p.get("battery", {}).get("kbd", 1)),
            "fan_mode": p.get("battery", {}).get("fan_mode", "silent"),
            "label": tr("bat_label"),
            "title": tr("bat_title"),
            "msg": tr("bat_msg"),
            "icon": "battery-good",
        },
        "usbc": {
            "profile": p.get("usbc", {}).get("profile", "balanced"),
            "hz": int(p.get("usbc", {}).get("hz", 60)),
            "brightness": int(p.get("usbc", {}).get("brightness", 75)),
            "kbd": int(p.get("usbc", {}).get("kbd", 1)),
            "fan_mode": p.get("usbc", {}).get("fan_mode", "balanced"),
            "label": tr("usbc_label"),
            "title": tr("usbc_title"),
            "msg": tr("usbc_msg"),
            "icon": "ac-adapter",
        },
        "ac": {
            "profile": p.get("ac", {}).get("profile", "performance"),
            "hz": int(p.get("ac", {}).get("hz", 240)),
            "brightness": int(p.get("ac", {}).get("brightness", 100)),
            "kbd": int(p.get("ac", {}).get("kbd", 3)),
            "fan_mode": p.get("ac", {}).get("fan_mode", "turbo"),
            "label": tr("ac_label"),
            "title": tr("ac_title"),
            "msg": tr("ac_msg"),
            "icon": "ac-adapter",
        },
    }


class PowerManagerController:
    def __init__(self, verbose: bool = True):
        self.verbose = verbose
        self.last_state = None
        self.auto_mode = True
        self.cached_panel = ("eDP-1", "2560x1440", 60.0, [])
        self.cached_profile = "balanced"
        self.idle_mgr = None

    def apply_state(self, force: bool = False):
        global CURRENT_FAN_MODE
        if not self.auto_mode and not force:
            return

        new_state = get_power_source()
        cfg = get_state_config()[new_state]
        prof = cfg["profile"]
        hz = cfg["hz"]

        state_changed = (self.last_state is not None and self.last_state != new_state)
        prof_changed = set_power_profile(prof)
        rate_changed = set_display_refresh_rate(hz)

        if state_changed or force:
            set_screen_and_kbd_brightness(cfg["brightness"], cfg["kbd"])
            apply_hardware_tunings(new_state)
            if CURRENT_FAN_MODE == "auto":
                target_fan = cfg.get("fan_mode", "auto")
                set_fan_mode(target_fan)
                CURRENT_FAN_MODE = "auto"
            if self.idle_mgr:
                self.idle_mgr.update_state(new_state)

            # Run user automations & hooks on power state change
            if state_changed:
                cmd = CONFIG_MGR.data.get("profiles", {}).get(new_state, {}).get("command", "")
                CONFIG_MGR.run_command(cmd, {"ROG_POWER_STATE": new_state, "ROG_PREV_STATE": str(self.last_state)})
                CONFIG_MGR.run_hook("on_state_change", new_state, str(self.last_state))

        self.cached_panel = get_panel_info()
        self.cached_profile = get_current_power_profile()
        _, _, current_rate, _ = self.cached_panel
        display_ok = (current_rate is not None and abs(current_rate - hz) < 1.0)

        if display_ok:
            self.last_state = new_state

        if self.verbose and (state_changed or prof_changed or rate_changed or force):
            print(f"[AutoPower] State: {new_state.upper()} | Profile: {prof} | Refresh Rate: {hz}Hz (Display OK: {display_ok})")

        if state_changed or force:
            send_notification(cfg["title"], cfg["msg"], cfg["icon"])


class AboutDialog(QDialog):
    """Modern GUI About window showcasing author, brand, links, license and system status."""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle(tr("about_win_title"))
        self.setMinimumWidth(520)
        self.setStyleSheet("""
            QDialog {
                background-color: #111827;
                color: #f3f4f6;
                font-family: 'Segoe UI', 'Ubuntu', 'Sans Serif';
            }
            QLabel {
                color: #e5e7eb;
            }
            QGroupBox {
                border: 1px solid #374151;
                border-radius: 8px;
                margin-top: 1em;
                padding-top: 10px;
                font-weight: bold;
                color: #93c5fd;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 12px;
                padding: 0 4px;
            }
            QPushButton {
                background-color: #2563eb;
                color: white;
                border-radius: 6px;
                padding: 6px 20px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #1d4ed8;
            }
        """)

        layout = QVBoxLayout(self)

        # Header Title
        title_lbl = QLabel(f"<h2 style='margin: 0; color: #60a5fa;'>⚡ {APP_NAME} <span style='font-size: 14px; color: #9ca3af;'>v{APP_VERSION}</span></h2>")
        desc_lbl = QLabel(f"<p style='color: #d1d5db; margin-top: 4px;'>"
                          f"Hardware-, Energie-, C-Raten- & MUX-Management für ASUS ROG Strix (G713RM)</p>")
        layout.addWidget(title_lbl)
        layout.addWidget(desc_lbl)

        # Developer & Project Info
        dev_group = QGroupBox(tr("about_dev_group"))
        dev_layout = QFormLayout(dev_group)
        dev_layout.addRow(f"<b>{tr('about_author')}:</b>", QLabel(APP_AUTHOR))
        dev_layout.addRow(f"<b>{tr('about_org')}:</b>", QLabel(APP_ORG))

        web_lbl = QLabel(f"<a href='{APP_WEBSITE}' style='color: #60a5fa;'>{APP_WEBSITE}</a>")
        web_lbl.setOpenExternalLinks(True)
        dev_layout.addRow(f"<b>{tr('about_web')}:</b>", web_lbl)

        repo_lbl = QLabel(f"<a href='{APP_URL}' style='color: #60a5fa;'>{APP_URL}</a>")
        repo_lbl.setOpenExternalLinks(True)
        dev_layout.addRow(f"<b>{tr('about_repo')}:</b>", repo_lbl)

        dev_layout.addRow(f"<b>{tr('about_license')}:</b>", QLabel(f"GNU GPLv3+ ({APP_LICENSE})"))
        layout.addWidget(dev_group)

        # Live System Status
        sys_group = QGroupBox(tr("about_sys_group"))
        sys_layout = QFormLayout(sys_group)
        src = get_power_source()
        src_label = get_state_config()[src]["label"]
        prof = get_current_power_profile()
        panel_name, _, rate, _ = get_panel_info()
        rate_str = f"{int(rate)} Hz" if rate else "60 Hz"

        sys_layout.addRow("<b>Stromquelle:</b>", QLabel(src_label))
        sys_layout.addRow("<b>Profil & Display:</b>", QLabel(f"{tr(f'prof_{prof}')} | {panel_name} @ {rate_str}"))

        try:
            with open("/sys/class/power_supply/BAT0/capacity", "r") as f:
                cap = f.read().strip()
            with open("/sys/class/power_supply/BAT0/charge_control_end_threshold", "r") as f:
                lim = f.read().strip()
            sys_layout.addRow("<b>Akku-Stand & Limit:</b>", QLabel(f"{cap}% (Ladelimit: {lim}%)"))
        except Exception:
            pass

        pm_path = "/sys/bus/pci/devices/0000:01:00.0/power/runtime_status"
        if os.path.exists(pm_path):
            try:
                with open(pm_path, "r") as f:
                    pm = f.read().strip()
                status_txt = tr("gpu_sleep") if pm == "suspended" else tr("gpu_active")
                sys_layout.addRow("<b>NVIDIA RTX 3060:</b>", QLabel(status_txt))
            except Exception:
                pass

        fans = get_fan_telemetry()
        fan_mode_txt = tr(f"fan_mode_{CURRENT_FAN_MODE}")
        if fans["cpu_rpm"] == 0 and fans["gpu_rpm"] == 0:
            fans_txt = f"{tr('fan_status_off')} ({fan_mode_txt})"
        else:
            fans_txt = f"CPU {fans['cpu_rpm']} RPM | GPU {fans['gpu_rpm']} RPM ({fan_mode_txt})"
        sys_layout.addRow(f"<b>{tr('about_fans')}:</b>", QLabel(fans_txt))

        layout.addWidget(sys_group)

        # Close button
        btn_box = QHBoxLayout()
        btn_box.addStretch()
        close_btn = QPushButton(tr("about_close"))
        close_btn.clicked.connect(self.accept)
        btn_box.addWidget(close_btn)
        layout.addLayout(btn_box)


class SettingsDialog(QDialog):
    """Interactive GUI dialog to configure profiles, idle timeouts and shell automations."""
    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self.controller = controller
        self.setWindowTitle(tr("settings_title"))
        self.setMinimumSize(640, 520)
        self.setStyleSheet("""
            QDialog {
                background-color: #111827;
                color: #f3f4f6;
                font-family: 'Segoe UI', 'Ubuntu', 'Sans Serif';
            }
            QTabWidget::pane {
                border: 1px solid #374151;
                border-radius: 8px;
                padding: 10px;
                background-color: #1f2937;
            }
            QTabBar::tab {
                background: #111827;
                color: #9ca3af;
                padding: 8px 16px;
                border-top-left-radius: 6px;
                border-top-right-radius: 6px;
                margin-right: 4px;
            }
            QTabBar::tab:selected {
                background: #1f2937;
                color: #60a5fa;
                font-weight: bold;
            }
            QGroupBox {
                border: 1px solid #374151;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 12px;
                color: #93c5fd;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 4px;
            }
            QLineEdit, QComboBox, QSpinBox {
                background-color: #374151;
                color: #f9fafb;
                border: 1px solid #4b5563;
                border-radius: 4px;
                padding: 4px 8px;
            }
            QPushButton {
                background-color: #2563eb;
                color: white;
                border-radius: 6px;
                padding: 6px 16px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #1d4ed8;
            }
        """)

        layout = QVBoxLayout(self)
        self.tabs = QTabWidget()

        # Tab 1: Profiles & Hardware
        tab_prof = QWidget()
        lay_prof = QVBoxLayout(tab_prof)
        self.widgets_prof = {}

        for state, lbl in (("battery", "⚡ Akkubetrieb (battery)"),
                           ("usbc", "🔌 USB-C PD (usbc)"),
                           ("ac", "🚀 240W Netzteil (ac)")):
            grp = QGroupBox(lbl)
            f_lay = QFormLayout(grp)
            p_data = CONFIG_MGR.data["profiles"].get(state, {})

            cb_prof = QComboBox()
            cb_prof.addItems(["power-saver", "balanced", "performance"])
            cb_prof.setCurrentText(p_data.get("profile", "balanced"))

            cb_hz = QComboBox()
            cb_hz.addItems(["60", "240"])
            cb_hz.setCurrentText(str(p_data.get("hz", 60)))

            sb_bright = QSpinBox()
            sb_bright.setRange(10, 100)
            sb_bright.setSuffix("%")
            sb_bright.setValue(int(p_data.get("brightness", 75)))

            sb_kbd = QSpinBox()
            sb_kbd.setRange(0, 3)
            sb_kbd.setValue(int(p_data.get("kbd", 1)))

            cb_fan = QComboBox()
            cb_fan.addItems(["auto", "silent", "balanced", "turbo", "max"])
            cb_fan.setCurrentText(p_data.get("fan_mode", "auto"))

            f_lay.addRow("Energieprofil:", cb_prof)
            f_lay.addRow("Bildwiederholrate:", cb_hz)
            f_lay.addRow("Display-Helligkeit:", sb_bright)
            f_lay.addRow("Tastatur-LED (0-3):", sb_kbd)
            f_lay.addRow("Lüftermodus:" if LANG == "de" else "Fan Mode:", cb_fan)

            self.widgets_prof[state] = {
                "profile": cb_prof,
                "hz": cb_hz,
                "brightness": sb_bright,
                "kbd": sb_kbd,
                "fan_mode": cb_fan,
            }
            lay_prof.addWidget(grp)

        self.tabs.addTab(tab_prof, tr("tab_profiles"))

        # Tab 2: Inactivity & Display
        tab_idle = QWidget()
        lay_idle = QVBoxLayout(tab_idle)
        i_data = CONFIG_MGR.data.get("idle", {})
        self.widgets_idle = {}

        grp_dim = QGroupBox("Display abdunkeln (Minuten, 0 = Aus)")
        dim_lay = QFormLayout(grp_dim)
        sb_dim_bat = QSpinBox(); sb_dim_bat.setRange(0, 60); sb_dim_bat.setValue(i_data.get("dim_battery_min", 3))
        sb_dim_usbc = QSpinBox(); sb_dim_usbc.setRange(0, 60); sb_dim_usbc.setValue(i_data.get("dim_usbc_min", 5))
        sb_dim_ac = QSpinBox(); sb_dim_ac.setRange(0, 120); sb_dim_ac.setValue(i_data.get("dim_ac_min", 10))
        dim_lay.addRow("Akkubetrieb:", sb_dim_bat)
        dim_lay.addRow("USB-C PD:", sb_dim_usbc)
        dim_lay.addRow("240W Netzteil:", sb_dim_ac)
        lay_idle.addWidget(grp_dim)

        grp_off = QGroupBox("Display ausschalten / DPMS (Minuten, 0 = Aus)")
        off_lay = QFormLayout(grp_off)
        sb_off_bat = QSpinBox(); sb_off_bat.setRange(0, 60); sb_off_bat.setValue(i_data.get("off_battery_min", 5))
        sb_off_usbc = QSpinBox(); sb_off_usbc.setRange(0, 60); sb_off_usbc.setValue(i_data.get("off_usbc_min", 10))
        sb_off_ac = QSpinBox(); sb_off_ac.setRange(0, 120); sb_off_ac.setValue(i_data.get("off_ac_min", 15))
        off_lay.addRow("Akkubetrieb:", sb_off_bat)
        off_lay.addRow("USB-C PD:", sb_off_usbc)
        off_lay.addRow("240W Netzteil:", sb_off_ac)
        lay_idle.addWidget(grp_off)

        grp_sus = QGroupBox("Automatischer Standby (Minuten, 0 = Aus)")
        sus_lay = QFormLayout(grp_sus)
        sb_sus_bat = QSpinBox(); sb_sus_bat.setRange(0, 120); sb_sus_bat.setValue(i_data.get("suspend_battery_min", 15))
        sb_sus_usbc = QSpinBox(); sb_sus_usbc.setRange(0, 120); sb_sus_usbc.setValue(i_data.get("suspend_usbc_min", 30))
        sb_sus_ac = QSpinBox(); sb_sus_ac.setRange(0, 240); sb_sus_ac.setValue(i_data.get("suspend_ac_min", 0))
        sus_lay.addRow("Akkubetrieb:", sb_sus_bat)
        sus_lay.addRow("USB-C PD:", sb_sus_usbc)
        sus_lay.addRow("240W Netzteil:", sb_sus_ac)
        lay_idle.addWidget(grp_sus)

        self.widgets_idle = {
            "dim_battery_min": sb_dim_bat, "dim_usbc_min": sb_dim_usbc, "dim_ac_min": sb_dim_ac,
            "off_battery_min": sb_off_bat, "off_usbc_min": sb_off_usbc, "off_ac_min": sb_off_ac,
            "suspend_battery_min": sb_sus_bat, "suspend_usbc_min": sb_sus_usbc, "suspend_ac_min": sb_sus_ac,
        }
        self.tabs.addTab(tab_idle, tr("tab_idle"))

        # Tab 3: Automations & Hooks
        tab_auto = QWidget()
        lay_auto = QVBoxLayout(tab_auto)
        info_auto = QLabel("Führe eigene Shell-Befehle oder Hooks aus, wenn bestimmte Ereignisse eintreten:")
        info_auto.setWordWrap(True)
        lay_auto.addWidget(info_auto)

        grp_cmds = QGroupBox("Shell-Befehle bei Statuswechsel")
        cmd_lay = QFormLayout(grp_cmds)
        self.le_cmd_bat = QLineEdit(CONFIG_MGR.data["profiles"].get("battery", {}).get("command", ""))
        self.le_cmd_usbc = QLineEdit(CONFIG_MGR.data["profiles"].get("usbc", {}).get("command", ""))
        self.le_cmd_ac = QLineEdit(CONFIG_MGR.data["profiles"].get("ac", {}).get("command", ""))
        cmd_lay.addRow("Befehl bei Akku:", self.le_cmd_bat)
        cmd_lay.addRow("Befehl bei USB-C:", self.le_cmd_usbc)
        cmd_lay.addRow("Befehl bei Netzteil (AC):", self.le_cmd_ac)
        lay_auto.addWidget(grp_cmds)

        grp_alt = QGroupBox("Akkuwarnungen & Befehle")
        alt_lay = QFormLayout(grp_alt)
        a_data = CONFIG_MGR.data.get("alerts", {})
        self.sb_low_pct = QSpinBox(); self.sb_low_pct.setRange(5, 30); self.sb_low_pct.setSuffix("%"); self.sb_low_pct.setValue(a_data.get("low_battery_percent", 10))
        self.le_cmd_low = QLineEdit(a_data.get("on_low_command", ""))
        alt_lay.addRow("Schwacher Akku ab:", self.sb_low_pct)
        alt_lay.addRow("Befehl bei schwachem Akku:", self.le_cmd_low)
        lay_auto.addWidget(grp_alt)

        btn_hooks = QPushButton(tr("btn_open_hooks"))
        btn_hooks.clicked.connect(lambda: subprocess.Popen(["xdg-open", CONFIG_MGR.hooks_dir]))
        lay_auto.addWidget(btn_hooks)
        lay_auto.addStretch()

        self.tabs.addTab(tab_auto, tr("tab_automations"))
        layout.addWidget(self.tabs)

        # Dialog Buttons
        btn_box = QHBoxLayout()
        btn_box.addStretch()
        cancel_btn = QPushButton(tr("btn_cancel"))
        cancel_btn.setStyleSheet("background-color: #4b5563;")
        cancel_btn.clicked.connect(self.reject)
        btn_box.addWidget(cancel_btn)

        save_btn = QPushButton(tr("btn_save"))
        save_btn.clicked.connect(self.save_and_apply)
        btn_box.addWidget(save_btn)
        layout.addLayout(btn_box)

    def save_and_apply(self):
        # 1. Update profiles
        for state, w in self.widgets_prof.items():
            CONFIG_MGR.data["profiles"][state]["profile"] = w["profile"].currentText()
            CONFIG_MGR.data["profiles"][state]["hz"] = int(w["hz"].currentText())
            CONFIG_MGR.data["profiles"][state]["brightness"] = w["brightness"].value()
            CONFIG_MGR.data["profiles"][state]["kbd"] = w["kbd"].value()
            CONFIG_MGR.data["profiles"][state]["fan_mode"] = w["fan_mode"].currentText()

        CONFIG_MGR.data["profiles"]["battery"]["command"] = self.le_cmd_bat.text().strip()
        CONFIG_MGR.data["profiles"]["usbc"]["command"] = self.le_cmd_usbc.text().strip()
        CONFIG_MGR.data["profiles"]["ac"]["command"] = self.le_cmd_ac.text().strip()

        # 2. Update idle timeouts
        for key, sb in self.widgets_idle.items():
            CONFIG_MGR.data["idle"][key] = sb.value()

        # 3. Update alerts
        CONFIG_MGR.data["alerts"]["low_battery_percent"] = self.sb_low_pct.value()
        CONFIG_MGR.data["alerts"]["on_low_command"] = self.le_cmd_low.text().strip()

        CONFIG_MGR.save()

        # Re-arm idle manager and apply state immediately
        if self.controller and self.controller.idle_mgr:
            self.controller.idle_mgr.rearm()
        if self.controller:
            self.controller.apply_state(force=True)

        self.accept()


def run_gui_daemon(controller: PowerManagerController):
    """Run the PyQt6 System Tray live Wattage monitor + DBus event loop."""
    DBusGMainLoop(set_as_default=True)
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)

    # Initialize PowerDevil replacement D-Bus services
    pm_service = None
    try:
        session_bus = dbus.SessionBus()
        try:
            fd_bus_name = dbus.service.BusName("org.freedesktop.PowerManagement", session_bus, do_not_queue=True)
            pm_service = FreedesktopPowerManagementService(fd_bus_name)
            FreedesktopInhibitService(fd_bus_name, pm_service)
            print("[D-Bus] Registered org.freedesktop.PowerManagement & Inhibit")
        except Exception as e:
            print(f"[D-Bus] org.freedesktop.PowerManagement: {e}")

        try:
            kde_bus_name = dbus.service.BusName("org.kde.Solid.PowerManagement", session_bus, do_not_queue=True)
            KDEBrightnessControlService(kde_bus_name)
            KDEKeyboardBrightnessControlService(kde_bus_name)
            if pm_service:
                KDEPolicyAgentService(kde_bus_name, pm_service)
            print("[D-Bus] Registered org.kde.Solid.PowerManagement Actions & PolicyAgent")
        except Exception as e:
            print(f"[D-Bus] org.kde.Solid.PowerManagement: {e}")
    except Exception as e:
        print(f"[D-Bus] Session bus error: {e}")

    def check_inhibit() -> bool:
        return pm_service.HasInhibit() if pm_service else False

    idle_mgr = IdleManager(check_inhibit)
    controller.idle_mgr = idle_mgr
    idle_mgr.update_state(get_power_source())

    tray = QSystemTrayIcon()

    def create_watt_icon(tel: dict) -> QIcon:
        size = 64
        pix = QPixmap(size, size)
        pix.fill(Qt.GlobalColor.transparent)

        painter = QPainter(pix)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setRenderHint(QPainter.RenderHint.TextAntialiasing)

        status = tel["status"]
        total_in = int(round(tel["total_in_watts"]))
        bat_w = int(round(tel["bat_watts"]))
        sys_w = int(round(tel["sys_watts"]))

        if status == "Charging":
            bg_color = QColor(16, 124, 65, 235)      # Emerald green
            border_color = QColor(52, 211, 153)      # Bright mint
            top_text = f"{total_in}W"
            bot_text = f"+{bat_w}|{sys_w}"
        elif status == "Discharging":
            if sys_w >= 25:
                bg_color = QColor(185, 28, 28, 235)  # Red
                border_color = QColor(248, 113, 113)
            else:
                bg_color = QColor(180, 83, 9, 235)   # Amber/Orange
                border_color = QColor(251, 191, 36)
            top_text = f"-{sys_w}W"
            bot_text = "OUT"
        else:
            bg_color = QColor(30, 64, 175, 235)      # Blue (Limit / Full)
            border_color = QColor(96, 165, 250)
            top_text = f"{sys_w}W"
            bot_text = f"0|{sys_w}"

        painter.setBrush(bg_color)
        painter.setPen(border_color)
        painter.drawRoundedRect(2, 2, size - 4, size - 4, 12, 12)

        font_top = QFont("Sans Serif", 16 if len(top_text) <= 4 else 13, QFont.Weight.Bold)
        painter.setFont(font_top)
        painter.setPen(QColor(255, 255, 255))
        painter.drawText(QRect(0, 4, size, 32), int(Qt.AlignmentFlag.AlignCenter), top_text)

        font_bot = QFont("Sans Serif", 10 if len(bot_text) > 4 else 11, QFont.Weight.Bold)
        painter.setFont(font_bot)
        painter.setPen(border_color)
        painter.drawText(QRect(0, 34, size, 24), int(Qt.AlignmentFlag.AlignCenter), bot_text)

        painter.end()
        return QIcon(pix)

    menu = QMenu()
    act_header = QAction(tr("menu_header"), menu)
    act_header.setEnabled(False)
    menu.addAction(act_header)

    act_total_in = QAction("...", menu)
    act_total_in.setEnabled(False)
    menu.addAction(act_total_in)

    act_bat = QAction("...", menu)
    act_bat.setEnabled(False)
    menu.addAction(act_bat)

    act_sys = QAction("...", menu)
    act_sys.setEnabled(False)
    menu.addAction(act_sys)

    act_sys_detail = QAction("...", menu)
    act_sys_detail.setEnabled(False)
    menu.addAction(act_sys_detail)

    menu.addSeparator()

    act_state = QAction("...", menu)
    act_state.setEnabled(False)
    menu.addAction(act_state)

    act_auto = QAction(tr("auto_mode"), menu)
    act_auto.setCheckable(True)
    act_auto.setChecked(True)

    def toggle_auto(checked):
        controller.auto_mode = checked
        if checked:
            controller.apply_state(force=True)
            update_tray()

    act_auto.triggered.connect(toggle_auto)
    menu.addAction(act_auto)
    menu.addSeparator()

    def manual_set(prof: str, hz: int):
        controller.auto_mode = False
        act_auto.setChecked(False)
        set_power_profile(prof)
        set_display_refresh_rate(hz)
        controller.cached_profile = get_current_power_profile()
        controller.cached_panel = get_panel_info()
        send_notification(tr("manual_title"), f"{tr(f'prof_{prof}')} | {hz} Hz")
        update_tray()

    act_ps = QAction(tr("menu_ps"), menu)
    act_ps.triggered.connect(lambda: manual_set("power-saver", 60))
    menu.addAction(act_ps)

    act_bal = QAction(tr("menu_bal"), menu)
    act_bal.triggered.connect(lambda: manual_set("balanced", 60))
    menu.addAction(act_bal)

    act_perf = QAction(tr("menu_perf"), menu)
    act_perf.triggered.connect(lambda: manual_set("performance", 240))
    menu.addAction(act_perf)

    menu.addSeparator()
    act_gpu_status = QAction("...", menu)
    act_gpu_status.setEnabled(False)
    menu.addAction(act_gpu_status)

    def get_gpu_mode() -> str:
        try:
            res = subprocess.check_output(["supergfxctl", "-g"], stderr=subprocess.DEVNULL, timeout=1.5).decode().strip()
        except Exception:
            res = "n/a"
        pm_path = "/sys/bus/pci/devices/0000:01:00.0/power/runtime_status"
        if os.path.exists(pm_path):
            try:
                with open(pm_path, "r") as f:
                    pm = f.read().strip()
                if pm == "suspended":
                    return f"{res} (RTX 3060: {tr('gpu_sleep')})"
                elif pm == "active":
                    return f"{res} (RTX 3060: {tr('gpu_active')})"
            except Exception:
                pass
        return res

    def switch_gpu_mode(mode: str):
        current = get_gpu_mode()
        try:
            if mode == "AsusMuxDgpu":
                mux_file = "/sys/devices/platform/asus-nb-wmi/gpu_mux_mode"
                mux_written = False
                if os.path.exists(mux_file):
                    try:
                        with open(mux_file, "w") as f:
                            f.write("0\n")
                        mux_written = True
                    except Exception:
                        pass
                if not mux_written:
                    if os.path.exists("/usr/local/bin/rog-gpu-mux-helper"):
                        subprocess.check_call(["sudo", "-n", "/usr/local/bin/rog-gpu-mux-helper", "AsusMuxDgpu"])
                send_notification(tr("gpu_mux_nv_title"), tr("gpu_mux_nv_msg"))
            else:
                try:
                    subprocess.check_call(["supergfxctl", "-m", mode])
                    mux_file = "/sys/devices/platform/asus-nb-wmi/gpu_mux_mode"
                    if os.path.exists(mux_file):
                        try:
                            with open(mux_file, "w") as f:
                                f.write("1\n")
                        except Exception:
                            pass
                    send_notification(tr("gpu_super_title"), tr("gpu_super_msg", mode=mode))
                except Exception:
                    if os.path.exists("/usr/local/bin/rog-gpu-mux-helper"):
                        subprocess.check_call(["sudo", "-n", "/usr/local/bin/rog-gpu-mux-helper", mode])
                        send_notification(tr("gpu_mux_prep_title", mode=mode), tr("gpu_mux_prep_msg", mode=mode))
        except Exception as e:
            send_notification(tr("gpu_err_title"), str(e))
        update_tray()

    act_gpu_int = QAction(tr("gpu_int_menu"), menu)
    act_gpu_int.triggered.connect(lambda: switch_gpu_mode("Integrated"))
    menu.addAction(act_gpu_int)

    act_gpu_hyb = QAction(tr("gpu_hyb_menu"), menu)
    act_gpu_hyb.triggered.connect(lambda: switch_gpu_mode("Hybrid"))
    menu.addAction(act_gpu_hyb)

    act_gpu_ded = QAction(tr("gpu_ded_menu"), menu)
    act_gpu_ded.triggered.connect(lambda: switch_gpu_mode("AsusMuxDgpu"))
    menu.addAction(act_gpu_ded)

    menu.addSeparator()
    act_bat_prot = QAction("...", menu)
    act_bat_prot.setEnabled(False)
    menu.addAction(act_bat_prot)

    def set_charge_limit(limit: int, label: str):
        written = False
        thresh_file = "/sys/class/power_supply/BAT0/charge_control_end_threshold"
        if os.path.exists(thresh_file):
            try:
                with open(thresh_file, "w") as f:
                    f.write(f"{limit}\n")
                written = True
            except (PermissionError, IOError):
                pass
            except Exception as e:
                print(f"[ChargeLimit] Sysfs error: {e}")

        if not written:
            if os.path.exists("/usr/local/bin/rog-gpu-mux-helper"):
                try:
                    subprocess.check_call(["sudo", "-n", "/usr/local/bin/rog-gpu-mux-helper", "charge-limit", str(limit)])
                    written = True
                except Exception as e:
                    send_notification(tr("bat_prot_err"), str(e))
                    return
            else:
                send_notification(tr("bat_prot_err"), "Permission denied writing charge threshold")
                return

        send_notification(tr("bat_prot_title"), tr("bat_prot_msg", label=label, limit=limit))
        update_tray()

    def stop_charging_now():
        tel_now = get_power_telemetry()
        cur_cap = max(20, min(100, tel_now["capacity"]))
        set_charge_limit(cur_cap, tr("stop_now_msg", cap=cur_cap))

    act_lim_stop = QAction(tr("menu_lim_stop"), menu)
    act_lim_stop.triggered.connect(stop_charging_now)
    menu.addAction(act_lim_stop)

    act_lim_60 = QAction(tr("menu_lim_60"), menu)
    act_lim_60.triggered.connect(lambda: set_charge_limit(60, tr("lbl_lim_60")))
    menu.addAction(act_lim_60)

    act_lim_80 = QAction(tr("menu_lim_80"), menu)
    act_lim_80.triggered.connect(lambda: set_charge_limit(80, tr("lbl_lim_80")))
    menu.addAction(act_lim_80)

    act_lim_100 = QAction(tr("menu_lim_100"), menu)
    act_lim_100.triggered.connect(lambda: set_charge_limit(100, tr("lbl_lim_100")))
    menu.addAction(act_lim_100)

    menu.addSeparator()

    # Inhibit / Presentation mode toggle
    act_inhibit = QAction(tr("menu_inhibit"), menu)
    act_inhibit.setCheckable(True)
    act_inhibit.setChecked(False)

    def toggle_inhibit(checked):
        if pm_service:
            pm_service.manual_inhibit = checked
        if checked:
            send_notification(tr("inhibit_on_title"), tr("inhibit_on_msg"))
        else:
            send_notification(tr("inhibit_off_title"), tr("inhibit_off_msg"))
        if idle_mgr:
            idle_mgr.rearm()

    act_inhibit.triggered.connect(toggle_inhibit)
    menu.addAction(act_inhibit)

    menu.addSeparator()

    # Display brightness submenu
    menu_bright = menu.addMenu(tr("menu_brightness"))
    for pct in (100, 75, 50, 25, 10):
        act_b = QAction(f"{pct}%", menu_bright)
        act_b.triggered.connect(lambda _, p=pct: set_brightness_pct(p))
        menu_bright.addAction(act_b)

    # Keyboard backlight submenu
    menu_kbd = menu.addMenu(tr("menu_kbd"))
    kbd_levels = [
        (3, "Stufe 3 (Hell)" if LANG == "de" else "Level 3 (Bright)"),
        (2, "Stufe 2 (Mittel)" if LANG == "de" else "Level 2 (Medium)"),
        (1, "Stufe 1 (Schwach)" if LANG == "de" else "Level 1 (Dim)"),
        (0, "Aus" if LANG == "de" else "Off"),
    ]
    for lvl, lbl in kbd_levels:
        act_k = QAction(lbl, menu_kbd)
        act_k.triggered.connect(lambda _, l=lvl: set_kbd_brightness(l))
        menu_kbd.addAction(act_k)

    menu.addSeparator()

    # Fan Control submenu
    menu_fan = menu.addMenu(tr("menu_fan", status=tr("fan_status_off")))
    group_fan = QActionGroup(menu_fan)
    group_fan.setExclusive(True)

    fan_actions = {}
    for f_mode, f_label_key in (
        ("auto", "fan_mode_auto"),
        ("silent", "fan_mode_silent"),
        ("balanced", "fan_mode_balanced"),
        ("turbo", "fan_mode_turbo"),
        ("max", "fan_mode_max"),
    ):
        act_f = QAction(tr(f_label_key), menu_fan)
        act_f.setCheckable(True)
        if f_mode == CURRENT_FAN_MODE:
            act_f.setChecked(True)
        group_fan.addAction(act_f)
        menu_fan.addAction(act_f)

        def make_fan_handler(m=f_mode):
            def handler():
                set_fan_mode(m)
                send_notification(tr("fan_set_title"), tr("fan_set_msg", mode=tr(f"fan_mode_{m}")))
                update_tray()
            return handler

        act_f.triggered.connect(make_fan_handler(f_mode))
        fan_actions[f_mode] = act_f

    menu.addSeparator()

    # Session actions (Lock, Suspend, Hibernate)
    act_lock = QAction(tr("menu_lock"), menu)
    act_lock.triggered.connect(lambda: subprocess.Popen(["loginctl", "lock-session"]))
    menu.addAction(act_lock)

    act_suspend = QAction(tr("menu_suspend"), menu)
    act_suspend.triggered.connect(lambda: subprocess.Popen(["loginctl", "suspend"]))
    menu.addAction(act_suspend)

    act_hibernate = QAction(tr("menu_hibernate"), menu)
    act_hibernate.triggered.connect(lambda: subprocess.Popen(["loginctl", "hibernate"]))
    menu.addAction(act_hibernate)

    menu.addSeparator()

    def open_settings():
        dlg = SettingsDialog(controller)
        dlg.exec()

    act_settings = QAction(tr("menu_settings"), menu)
    act_settings.triggered.connect(open_settings)
    menu.addAction(act_settings)

    def open_about():
        dlg = AboutDialog()
        dlg.exec()

    act_about = QAction(tr("menu_about"), menu)
    act_about.triggered.connect(open_about)
    menu.addAction(act_about)

    tray.setContextMenu(menu)

    tick_count = 0
    cached_gpu_mode = get_gpu_mode()
    low_bat_alerted = False
    crit_bat_alerted = False

    def update_tray():
        nonlocal tick_count, cached_gpu_mode, low_bat_alerted, crit_bat_alerted
        tick_count += 1

        if tick_count % 3 == 1:
            controller.apply_state()
            cached_gpu_mode = get_gpu_mode()

        prof = controller.cached_profile
        prof_disp = tr(f"prof_{prof}")
        panel_name, _, rate, _ = controller.cached_panel
        rate_val = rate if rate else 60.0
        rate_str = f"{int(rate_val)} Hz"

        tel = get_power_telemetry(rate_val)
        status = tel["status"]
        cap = tel["capacity"]
        limit = tel["charge_limit"]
        c_rate = tel["c_rate"]
        src = get_power_source()
        src_label = get_state_config()[src]["label"]

        total_in = tel["total_in_watts"]
        bat_w = tel["bat_watts"]
        sys_w = tel["sys_watts"]
        apu_w = tel["apu_watts"]
        dgpu_w = tel["dgpu_watts"]
        rest_w = tel["rest_watts"]

        # Low & Critical Battery Alerts & Automations
        if status == "Discharging":
            a_cfg = CONFIG_MGR.data.get("alerts", {})
            low_thresh = a_cfg.get("low_battery_percent", 10)
            crit_thresh = a_cfg.get("critical_battery_percent", 5)

            if cap <= crit_thresh and not crit_bat_alerted:
                send_notification(tr("bat_crit_title"), tr("bat_crit_msg", cap=cap))
                crit_bat_alerted = True
                cmd_crit = a_cfg.get("on_critical_command", "")
                if cmd_crit:
                    CONFIG_MGR.run_command(cmd_crit, {"ROG_CAP": str(cap)})
                CONFIG_MGR.run_hook("battery_critical", str(cap))
                QTimer.singleShot(15000, lambda: subprocess.Popen(["loginctl", "suspend"]))
            elif cap <= low_thresh and not low_bat_alerted:
                send_notification(tr("bat_low_title"), tr("bat_low_msg", cap=cap))
                low_bat_alerted = True
                cmd_low = a_cfg.get("on_low_command", "")
                if cmd_low:
                    CONFIG_MGR.run_command(cmd_low, {"ROG_CAP": str(cap)})
                CONFIG_MGR.run_hook("battery_low", str(cap))
        elif status == "Charging" or total_in > 0:
            low_bat_alerted = False
            crit_bat_alerted = False

        usbc_extra = f" (Max {tel['usbc_max_watts']:.0f}W)" if src == "usbc" and tel["usbc_max_watts"] > 0 else ""

        if status == "Charging":
            rem_wh = max(0.0, tel["energy_full_wh"] * (limit / 100.0) - tel["energy_now_wh"])
            eta = tr("eta_to_limit", h=rem_wh / bat_w, limit=limit) if bat_w > 1.0 and rem_wh > 0 else tr("eta_almost", limit=limit)
            line_in = tr("line_in", total_in=total_in, src_label=src_label, usbc_extra=usbc_extra)
            line_bat = tr("line_bat_chg", bat_w=bat_w, c_rate=c_rate, cap=cap, limit=limit, eta=eta)
            line_sys = tr("line_sys_run", sys_w=sys_w)
        elif status == "Discharging":
            eta = tr("eta_rem", h=tel["energy_now_wh"] / bat_w) if bat_w > 1.0 else ""
            line_in = tr("line_in_none")
            line_bat = tr("line_bat_dis", bat_w=bat_w, c_rate=c_rate, cap=cap, eta=eta)
            line_sys = tr("line_sys_bat", sys_w=sys_w)
        else:
            line_in = tr("line_in", total_in=total_in, src_label=src_label, usbc_extra=usbc_extra)
            line_bat = tr("line_bat_byp", cap=cap, limit=limit)
            line_sys = tr("line_sys_ac", sys_w=sys_w)

        line_detail = tr("line_detail", apu_w=apu_w, dgpu_w=dgpu_w, rest_w=rest_w)

        tray.setIcon(create_watt_icon(tel))

        fans = get_fan_telemetry()
        cpu_f = fans["cpu_rpm"]
        gpu_f = fans["gpu_rpm"]
        if cpu_f == 0 and gpu_f == 0:
            fan_stat_str = tr("fan_status_off")
        else:
            fan_stat_str = tr("fan_status_active", cpu=cpu_f, gpu=gpu_f)

        menu_fan.setTitle(tr("menu_fan", status=fan_stat_str))
        if CURRENT_FAN_MODE in fan_actions and not fan_actions[CURRENT_FAN_MODE].isChecked():
            fan_actions[CURRENT_FAN_MODE].setChecked(True)

        act_total_in.setText(line_in)
        act_bat.setText(line_bat)
        act_sys.setText(line_sys)
        act_sys_detail.setText(line_detail)
        act_state.setText(tr("menu_profile", prof=prof_disp, panel=panel_name, rate=rate_str))
        act_gpu_status.setText(tr("menu_gpu", gpu=cached_gpu_mode))
        act_bat_prot.setText(tr("menu_bat_prot", limit=limit, c_rate=c_rate))

        tooltip = (
            f"{tr('tt_header')}\n"
            f"{line_in}\n"
            f"{line_bat}\n"
            f"{line_sys}\n"
            f"{line_detail}\n"
            f"──────────────────────────\n"
            f"{tr('tt_fan', status=fan_stat_str, mode=tr(f'fan_mode_{CURRENT_FAN_MODE}'))}\n"
            f"{tr('tt_limit', limit=limit, c_rate=c_rate)}\n"
            f"{tr('tt_profile', prof=prof_disp, rate=rate_str)}\n"
            f"{tr('tt_gpu', gpu=cached_gpu_mode)}"
        )
        tray.setToolTip(tooltip)

    try:
        bus = dbus.SystemBus()
        bus.add_signal_receiver(
            lambda interface, changed, invalidated: (
                controller.apply_state(),
                update_tray()
            ) if ("OnBattery" in changed or "Online" in changed) else None,
            signal_name="PropertiesChanged",
            dbus_interface="org.freedesktop.DBus.Properties",
            path=None
        )
    except Exception:
        pass

    controller.apply_state(force=True)
    update_tray()
    tray.show()

    timer = QTimer()
    timer.timeout.connect(update_tray)
    timer.start(2000)

    print(f"[ROG Power Manager] Tray indicator & daemon started (Language: {LANG}).")
    sys.exit(app.exec())


def main():
    global LANG
    parser = argparse.ArgumentParser(
        prog=APP_NAME,
        description="ASUS ROG Strix Power Manager, Fan Control (silent/balanced/turbo/max), Refresh Rate Switcher & Live Wattage Tray Indicator",
        epilog=f"Created by {APP_AUTHOR} <{APP_EMAIL}> ({APP_WEBSITE} | {APP_URL})",
    )
    parser.add_argument("-v", "--version", action="store_true", help="Print version, copyright and license information")
    parser.add_argument("--once", action="store_true", help="Apply current settings once and exit")
    parser.add_argument("--status", action="store_true", help="Print current status and detection")
    parser.add_argument("--fan", choices=["auto", "silent", "balanced", "turbo", "max"], help="Set ASUS fan mode")
    parser.add_argument("--lang", choices=["de", "en"], help="Override display language (de/en)")
    args = parser.parse_args()

    if args.lang:
        LANG = args.lang

    if args.version:
        print(f"{APP_NAME} {APP_VERSION}")
        print(f"Copyright (C) 2026 {APP_AUTHOR} <{APP_EMAIL}>")
        print(f"Website:    {APP_WEBSITE}")
        print(f"Repository: {APP_URL}")
        print(f"License:    {APP_LICENSE} <https://www.gnu.org/licenses/gpl-3.0.html>")
        print(tr("ver_free"))
        print(tr("ver_attr"))
        return

    if args.fan:
        set_fan_mode(args.fan)
        print(f"[FanControl] Fan mode set to: {args.fan}")
        return

    if args.status:
        src = get_power_source()
        cfg = get_state_config()[src]
        current_prof = get_current_power_profile()
        prof_disp = tr(f"prof_{current_prof}")
        panel_name, current_res, current_rate, available_rates = get_panel_info()
        tel = get_power_telemetry(current_rate or 60.0)
        st_disp = tr(f"st_{tel['status']}")
        dst_disp = tr(f"dst_{tel['dgpu_state']}")
        sign = "+" if tel["status"] == "Charging" else ("-" if tel["status"] == "Discharging" else "")

        fans = get_fan_telemetry()
        fan_mode_txt = tr(f"fan_mode_{CURRENT_FAN_MODE}")
        if fans["cpu_rpm"] == 0 and fans["gpu_rpm"] == 0:
            fan_cli = f"{tr('fan_status_off')} ({fan_mode_txt})"
        else:
            fan_cli = f"CPU {fans['cpu_rpm']} RPM | GPU {fans['gpu_rpm']} RPM ({fan_mode_txt})"

        print(tr("cli_header"))
        print(tr("cli_source", label=cfg["label"]))
        print(tr("cli_total_in", w=tel["total_in_watts"]))
        print(tr("cli_bat", sign=sign, w=tel["bat_watts"], c=tel["c_rate"], st=st_disp, cap=tel["capacity"], lim=tel["charge_limit"]))
        print(tr("cli_sys", w=tel["sys_watts"], apu=tel["apu_watts"], dgpu=tel["dgpu_watts"], dst=dst_disp, rest=tel["rest_watts"]))
        print(tr("cli_prof", prof=prof_disp))
        print(tr("cli_panel", panel=panel_name, res=current_res, rate=current_rate))
        print(tr("cli_fan", status=fan_cli))
        return

    controller = PowerManagerController()
    if args.once:
        controller.apply_state(force=True)
    else:
        run_gui_daemon(controller)


if __name__ == "__main__":
    main()
