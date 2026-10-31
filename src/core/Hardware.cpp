#include "Hardware.hpp"
#include "Config.hpp"
#include "Translations.hpp"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <cmath>
#include <iostream>

namespace Hardware {

QString currentFanMode = "auto";

// Internal file helpers
static QString readSysfs(const QString &path) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        QString s = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
        return s;
    }
    return "";
}

static QString findHelper(const QString &name) {
    if (QFile::exists("/usr/local/bin/" + name)) return "/usr/local/bin/" + name;
    if (QFile::exists("/usr/bin/" + name)) return "/usr/bin/" + name;
    return "/usr/local/bin/" + name;
}

bool writeSysfs(const QString &path, const QString &val) {
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write((val + "\n").toUtf8());
        f.close();
        return true;
    }
    return false;
}

QString getPowerSource() {
    int adp0 = readSysfs("/sys/class/power_supply/ADP0/online").toInt();
    int ucsi = readSysfs("/sys/class/power_supply/ucsi-source-psy-USBC000:001/online").toInt();
    int chargeMode = readSysfs("/sys/devices/platform/asus-nb-wmi/charge_mode").toInt();

    if (adp0 == 1 && ucsi == 0) {
        return "ac";
    }
    if (ucsi == 1 || chargeMode == 2) {
        return "usbc";
    }
    return "battery";
}

int getRawBrightness() {
    QDir dir("/sys/class/backlight");
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString bPath = QString("/sys/class/backlight/%1/brightness").arg(entry);
        bool ok = false;
        int val = readSysfs(bPath).toInt(&ok);
        if (ok) return val;
    }
    return 32768;
}

int getMaxBrightness() {
    QDir dir("/sys/class/backlight");
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString mPath = QString("/sys/class/backlight/%1/max_brightness").arg(entry);
        bool ok = false;
        int val = readSysfs(mPath).toInt(&ok);
        if (ok && val > 0) return val;
    }
    return 65535;
}

int getBrightnessPct() {
    int mx = getMaxBrightness();
    if (mx > 0) {
        return std::clamp(static_cast<int>(std::round((getRawBrightness() / static_cast<double>(mx)) * 100.0)), 0, 100);
    }
    return 50;
}

void setRawBrightness(int val) {
    int mx = getMaxBrightness();
    int clamped = std::clamp(val, 100, mx);
    QDir dir("/sys/class/backlight");
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        writeSysfs(QString("/sys/class/backlight/%1/brightness").arg(entry), QString::number(clamped));
    }
}

void setBrightnessPct(int pct) {
    int mx = getMaxBrightness();
    int target = static_cast<int>(std::round((pct / 100.0) * mx));
    setRawBrightness(target);
}

int stepScreenBrightness(int deltaPct) {
    int cur = getBrightnessPct();
    int target = 0;
    if (std::abs(deltaPct) >= 5) {
        if (deltaPct > 0) {
            target = std::min(100, (cur % 5 == 0 ? cur + 5 : (cur / 5 + 1) * 5));
        } else {
            target = std::max(1, (cur % 5 == 0 ? cur - 5 : (cur / 5) * 5));
        }
    } else {
        target = std::clamp(cur + deltaPct, 1, 100);
    }
    setBrightnessPct(target);
    showKdeOsdBrightness(target);
    return target;
}

int getKbdBrightness() {
    QDir dir("/sys/class/leds");
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.contains("kbd_backlight")) {
            QString bPath = QString("/sys/class/leds/%1/brightness").arg(entry);
            bool ok = false;
            int val = readSysfs(bPath).toInt(&ok);
            if (ok) return val;
        }
    }
    return 1;
}

void setKbdBrightness(int level) {
    int clamped = std::clamp(level, 0, 3);
    QDir dir("/sys/class/leds");
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.contains("kbd_backlight")) {
            writeSysfs(QString("/sys/class/leds/%1/brightness").arg(entry), QString::number(clamped));
        }
    }
}

int stepKbdBrightness(int delta) {
    int cur = getKbdBrightness();
    int target = std::clamp(cur + delta, 0, 3);
    setKbdBrightness(target);
    showKdeOsdKbd(target);
    return target;
}

int toggleKbdBrightness() {
    int cur = getKbdBrightness();
    int target = (cur > 0) ? 0 : 3;
    setKbdBrightness(target);
    showKdeOsdKbd(target);
    return target;
}

FanTelemetry getFanTelemetry() {
    FanTelemetry ft;
    QDir baseDir("/sys/devices/platform/asus-nb-wmi/hwmon");
    for (const QString &hwEntry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString f1Path = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1/fan1_input").arg(hwEntry);
        QString f2Path = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1/fan2_input").arg(hwEntry);
        if (QFile::exists(f1Path) || QFile::exists(f2Path)) {
            ft.cpuRpm = readSysfs(f1Path).toInt();
            ft.gpuRpm = readSysfs(f2Path).toInt();
            break;
        }
    }
    return ft;
}

QString detectFanMode() {
    QString customHwmon;
    QDir baseDir("/sys/devices/platform/asus-nb-wmi/hwmon");
    for (const QString &hwEntry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString namePath = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1/name").arg(hwEntry);
        if (readSysfs(namePath) == "asus_custom_fan_curve") {
            customHwmon = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1").arg(hwEntry);
            break;
        }
    }

    if (!customHwmon.isEmpty()) {
        int pwmEn = readSysfs(customHwmon + "/pwm1_enable").toInt();
        if (pwmEn == 1) {
            int p1 = readSysfs(customHwmon + "/pwm1_auto_point1_pwm").toInt();
            if (p1 >= 250) return "max";
            return "turbo";
        }
    }

    QString therm = readSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy");
    if (therm == "2") return "silent";
    if (therm == "0") return "balanced";
    if (therm == "1") return "turbo";
    return currentFanMode.isEmpty() ? "auto" : currentFanMode;
}

bool setFanMode(const QString &mode) {
    currentFanMode = mode;
    bool written = false;

    // Locate asus_custom_fan_curve hwmon
    QString customHwmon;
    QDir baseDir("/sys/devices/platform/asus-nb-wmi/hwmon");
    for (const QString &hwEntry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString namePath = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1/name").arg(hwEntry);
        if (readSysfs(namePath) == "asus_custom_fan_curve") {
            customHwmon = QString("/sys/devices/platform/asus-nb-wmi/hwmon/%1").arg(hwEntry);
            break;
        }
    }

    if (mode == "max") {
        writeSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", "1");
        writeSysfs("/sys/firmware/acpi/platform_profile", "performance");
        if (!customHwmon.isEmpty()) {
            for (int i = 1; i <= 8; ++i) {
                writeSysfs(QString("%1/pwm1_auto_point%2_pwm").arg(customHwmon).arg(i), "255");
                writeSysfs(QString("%1/pwm2_auto_point%2_pwm").arg(customHwmon).arg(i), "255");
            }
            writeSysfs(customHwmon + "/pwm1_enable", "1");
            writeSysfs(customHwmon + "/pwm2_enable", "1");
        }
        written = true;
    } else if (mode == "turbo") {
        writeSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", "1");
        writeSysfs("/sys/firmware/acpi/platform_profile", "performance");
        if (!customHwmon.isEmpty()) {
            QList<int> pwms = {180, 190, 200, 210, 225, 240, 255, 255};
            for (int i = 1; i <= 8; ++i) {
                writeSysfs(QString("%1/pwm1_auto_point%2_pwm").arg(customHwmon).arg(i), QString::number(pwms[i - 1]));
                writeSysfs(QString("%1/pwm2_auto_point%2_pwm").arg(customHwmon).arg(i), QString::number(pwms[i - 1]));
            }
            writeSysfs(customHwmon + "/pwm1_enable", "1");
            writeSysfs(customHwmon + "/pwm2_enable", "1");
        }
        written = true;
    } else if (mode == "silent") {
        if (!customHwmon.isEmpty()) {
            writeSysfs(customHwmon + "/pwm1_enable", "2");
            writeSysfs(customHwmon + "/pwm2_enable", "2");
        }
        writeSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", "2");
        writeSysfs("/sys/firmware/acpi/platform_profile", "quiet");
        written = true;
    } else if (mode == "balanced") {
        if (!customHwmon.isEmpty()) {
            writeSysfs(customHwmon + "/pwm1_enable", "2");
            writeSysfs(customHwmon + "/pwm2_enable", "2");
        }
        writeSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", "0");
        writeSysfs("/sys/firmware/acpi/platform_profile", "balanced");
        written = true;
    } else if (mode == "auto") {
        if (!customHwmon.isEmpty()) {
            writeSysfs(customHwmon + "/pwm1_enable", "2");
            writeSysfs(customHwmon + "/pwm2_enable", "2");
        }
        written = true;
    }

    QString pHelper = findHelper("rog-power-helper");
    if (!written && QFile::exists(pHelper)) {
        QProcess::execute("sudo", {"-n", pHelper, "fan-mode", mode});
        written = true;
    }

    std::cout << "[FanControl] Fan mode applied: " << mode.toStdString() << " (Success: " << (written ? "true" : "false") << ")" << std::endl;
    return written;
}

QString getCurrentPowerProfile() {
    QProcess p;
    p.start("powerprofilesctl", {"get"});
    if (p.waitForFinished(1000) && p.exitCode() == 0) {
        QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) return out;
    }

    QString plat = readSysfs("/sys/firmware/acpi/platform_profile");
    if (plat == "quiet") return "power-saver";
    if (plat == "balanced") return "balanced";
    if (plat == "performance") return "performance";

    return "balanced";
}

bool setPowerProfile(const QString &profile) {
    QString current = getCurrentPowerProfile();
    if (current == profile) return false;

    // Ensure CPU boost is enabled so amd_pstate does not reject profile changes with Error 13
    writeSysfs("/sys/devices/system/cpu/cpufreq/boost", "1");

    bool success = false;
    QProcess p;
    p.start("powerprofilesctl", {"set", profile});
    if (p.waitForFinished(1500) && p.exitCode() == 0) {
        success = true;
    }

    QMap<QString, QString> platMap = {{"power-saver", "quiet"}, {"balanced", "balanced"}, {"performance", "performance"}};
    QMap<QString, QString> thermMap = {{"power-saver", "2"}, {"balanced", "0"}, {"performance", "1"}};

    if (platMap.contains(profile)) {
        if (writeSysfs("/sys/firmware/acpi/platform_profile", platMap[profile])) {
            success = true;
        }
    }
    if (thermMap.contains(profile)) {
        if (writeSysfs("/sys/devices/platform/asus-nb-wmi/throttle_thermal_policy", thermMap[profile])) {
            success = true;
        }
    }

    QString pHelper = findHelper("rog-power-helper");
    if (!success && QFile::exists(pHelper)) {
        if (QProcess::execute("sudo", {"-n", pHelper, "set-profile", profile}) == 0) {
            success = true;
        }
    }

    if (success) {
        std::cout << "[PowerProfile] Profile switched: " << current.toStdString() << " -> " << profile.toStdString() << std::endl;
    }
    return success;
}

PanelInfo getPanelInfo() {
    PanelInfo pi;
    QProcess p;
    p.start("kscreen-doctor", {"-j"});
    if (p.waitForFinished(1500) && p.exitCode() == 0) {
        QString out = QString::fromUtf8(p.readAllStandardOutput());
        QRegularExpression nameRe("\"name\":\\s*\"(eDP[^\"]*)\"");
        auto m = nameRe.match(out);
        if (m.hasMatch()) pi.name = m.captured(1);

        QRegularExpression resRe("\"width\":\\s*(\\d+),\\s*\"height\":\\s*(\\d+)");
        auto mRes = resRe.match(out);
        if (mRes.hasMatch()) {
            pi.resolution = QString("%1x%2").arg(mRes.captured(1), mRes.captured(2));
        }

        QRegularExpression rateRe("\"refreshRate\":\\s*(\\d+)");
        auto mRate = rateRe.match(out);
        if (mRate.hasMatch()) {
            pi.currentRate = mRate.captured(1).toDouble();
        }
    }
    return pi;
}

bool setDisplayRefreshRate(int hz) {
    PanelInfo pi = getPanelInfo();
    if (std::abs(pi.currentRate - hz) < 1.0) {
        return false;
    }

    std::cout << "[Display] Changing " << pi.name.toStdString() << " (" << pi.resolution.toStdString() << ") refresh rate: "
              << pi.currentRate << "Hz -> " << hz << "Hz" << std::endl;

    QString modeArg = QString("%1@%2").arg(pi.name).arg(hz);
    QProcess p;
    p.start("kscreen-doctor", {QString("output.%1.mode.%2").arg(pi.name).arg(modeArg)});
    if (p.waitForFinished(2000) && p.exitCode() == 0) {
        return true;
    }

    // Fallback: search mode list
    QProcess p2;
    p2.start("kscreen-doctor", {"-j"});
    if (p2.waitForFinished(2000) && p2.exitCode() == 0) {
        QString jsonStr = QString::fromUtf8(p2.readAllStandardOutput());
        QRegularExpression mRe(QString("\"id\":\\s*\"([^\"]+)\"[^}]*\"width\":\\s*2560[^}]*\"refreshRate\":\\s*%1").arg(hz));
        auto match = mRe.match(jsonStr);
        if (match.hasMatch()) {
            QString modeId = match.captured(1);
            QProcess::execute("kscreen-doctor", {QString("output.%1.mode.%2").arg(pi.name, modeId)});
            return true;
        }
    }

    return false;
}

void applyHardwareTunings(const QString &state, const QString &profile) {
    QString prof = profile;
    if (prof.isEmpty()) {
        auto cfg = ConfigManager::instance().getData().value("profiles").toObject();
        prof = cfg.value(state).toObject().value("profile").toString("balanced");
    }

    // 1. Panel Overdrive
    writeSysfs("/sys/devices/platform/asus-nb-wmi/panel_od", (state == "ac" || prof == "performance") ? "1" : "0");

    // 2. CPU Boost Master Switch
    writeSysfs("/sys/devices/system/cpu/cpufreq/boost", "1");

    // 3. PCIe Runtime PM
    QStringList pciDevs = {"0000:02:00.0", "0000:03:00.0", "0000:04:00.0", "0000:05:00.0", "0000:06:00.0", "0000:06:00.2"};
    for (const auto &dev : pciDevs) {
        writeSysfs(QString("/sys/bus/pci/devices/%1/power/control").arg(dev), "auto");
    }

    // 4. Background tunables
    QString gHelper = findHelper("rog-gpu-mux-helper");
    if (QFile::exists(gHelper)) {
        QProcess::startDetached("sudo", {"-n", gHelper, "power-" + state});
    }
}

int getBatteryLimit() {
    int val = readSysfs("/sys/class/power_supply/BAT0/charge_control_end_threshold").toInt();
    return (val >= 20 && val <= 100) ? val : 80;
}

bool setBatteryLimit(int limit) {
    int clamped = std::clamp(limit, 20, 100);
    int current = readSysfs("/sys/class/power_supply/BAT0/charge_control_end_threshold").toInt();
    if (current == clamped) {
        return true;
    }
    bool ok = writeSysfs("/sys/class/power_supply/BAT0/charge_control_end_threshold", QString::number(clamped));
    if (!ok) {
        QString helper = findHelper("rog-power-helper");
        if (!QFile::exists(helper)) helper = findHelper("rog-gpu-mux-helper");
        if (QFile::exists(helper)) {
            ok = (QProcess::execute("sudo", {"-n", helper, "charge-limit", QString::number(clamped)}) == 0);
        }
    }
    return ok;
}

PowerTelemetry getPowerTelemetry(double refreshRate) {
    PowerTelemetry t;
    t.status = readSysfs("/sys/class/power_supply/BAT0/status");
    if (t.status.isEmpty()) t.status = "Discharging";

    t.capacity = readSysfs("/sys/class/power_supply/BAT0/capacity").toInt();
    t.chargeLimit = readSysfs("/sys/class/power_supply/BAT0/charge_control_end_threshold").toInt();
    if (t.chargeLimit == 0) t.chargeLimit = 80;

    double pNow = readSysfs("/sys/class/power_supply/BAT0/power_now").toDouble() / 1000000.0;
    double cNow = readSysfs("/sys/class/power_supply/BAT0/current_now").toDouble() / 1000000.0;
    double vNow = readSysfs("/sys/class/power_supply/BAT0/voltage_now").toDouble() / 1000000.0;

    if (pNow <= 0.0 && cNow > 0.0 && vNow > 0.0) {
        pNow = cNow * vNow;
    }
    t.batWatts = std::abs(pNow);

    t.energyNowWh = readSysfs("/sys/class/power_supply/BAT0/energy_now").toDouble() / 1000000.0;
    t.energyFullWh = readSysfs("/sys/class/power_supply/BAT0/energy_full").toDouble() / 1000000.0;
    if (t.energyFullWh <= 0.0) t.energyFullWh = 90.0;

    t.cRate = t.batWatts / 80.0;

    // Check dGPU status
    QString dgpuPm = readSysfs("/sys/bus/pci/devices/0000:01:00.0/power/runtime_status");
    t.dgpuState = (dgpuPm == "suspended") ? "suspended" : "active";
    t.dgpuWatts = (t.dgpuState == "active") ? 10.0 : 0.0;

    t.apuWatts = (refreshRate >= 120.0) ? 9.5 : 5.0;
    t.restWatts = (refreshRate >= 120.0) ? 10.0 : 7.0;

    QString src = getPowerSource();
    if (src == "battery") {
        t.totalInWatts = 0.0;
        t.sysWatts = (t.status == "Discharging") ? t.batWatts : (t.apuWatts + t.dgpuWatts + t.restWatts);
    } else if (src == "usbc") {
        t.totalInWatts = (t.status == "Charging") ? (t.batWatts + 15.0) : 15.0;
        t.sysWatts = std::max(5.0, t.totalInWatts - ((t.status == "Charging") ? t.batWatts : 0.0));
        t.usbcMaxWatts = 100.0;
    } else {
        t.totalInWatts = (t.status == "Charging") ? (t.batWatts + 25.0) : 25.0;
        t.sysWatts = std::max(10.0, t.totalInWatts - ((t.status == "Charging") ? t.batWatts : 0.0));
    }

    return t;
}

void showKdeOsdBrightness(int percent) {
    auto msg = QDBusMessage::createMethodCall("org.kde.plasmashell", "/org/kde/osdService", "org.kde.osdService", "brightnessChanged");
    msg << percent;
    QDBusConnection::sessionBus().send(msg);
}

void showKdeOsdKbd(int level) {
    int pct = static_cast<int>(std::round((level / 3.0) * 100.0));
    auto msg = QDBusMessage::createMethodCall("org.kde.plasmashell", "/org/kde/osdService", "org.kde.osdService", "keyboardBrightnessChanged");
    msg << pct;
    QDBusConnection::sessionBus().send(msg);
}

void showKdeOsdProfile(const QString &profile) {
    auto msg = QDBusMessage::createMethodCall("org.kde.plasmashell", "/org/kde/osdService", "org.kde.osdService", "powerProfileChanged");
    msg << profile;
    QDBusConnection::sessionBus().send(msg);
}

void sendNotification(const QString &title, const QString &msg, const QString &icon) {
    QProcess::startDetached("notify-send", {"-u", "low", "-a", "ROG Power Manager", "-i", icon, title, msg, "-h", "string:desktop-entry:rog-power-manager"});
}

bool isLidClosed() {
    QFile f("/proc/acpi/button/lid/LID0/state");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString s = QString::fromUtf8(f.readAll());
        return s.contains("closed");
    }
    return false;
}

bool hasExternalMonitor() {
    QDir drmDir("/sys/class/drm");
    for (const QString &entry : drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.startsWith("card") && !entry.contains("eDP") && !entry.contains("Writeback")) {
            QString statusPath = QString("/sys/class/drm/%1/status").arg(entry);
            if (readSysfs(statusPath) == "connected") {
                return true;
            }
        }
    }
    return false;
}

} // namespace Hardware
