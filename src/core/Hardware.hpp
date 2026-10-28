#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace Hardware {

struct PowerTelemetry {
    QString status;
    int capacity = 0;
    int chargeLimit = 80;
    double cRate = 0.0;
    double energyNowWh = 0.0;
    double energyFullWh = 0.0;
    double totalInWatts = 0.0;
    double batWatts = 0.0;
    double sysWatts = 0.0;
    double apuWatts = 0.0;
    double dgpuWatts = 0.0;
    QString dgpuState = "suspended";
    double restWatts = 0.0;
    double usbcMaxWatts = 0.0;
};

struct FanTelemetry {
    int cpuRpm = 0;
    int gpuRpm = 0;
};

struct PanelInfo {
    QString name = "eDP-1";
    QString resolution = "2560x1440";
    double currentRate = 60.0;
    QList<int> availableRates;
};

// Power Source
QString getPowerSource();

// Telemetry
PowerTelemetry getPowerTelemetry(double refreshRate = 60.0);
FanTelemetry getFanTelemetry();
PanelInfo getPanelInfo();

// Fan Control
extern QString currentFanMode;
bool setFanMode(const QString &mode);

// Display Brightness
int getRawBrightness();
int getMaxBrightness();
int getBrightnessPct();
void setRawBrightness(int val);
void setBrightnessPct(int pct);
int stepScreenBrightness(int deltaPct);

// Keyboard Brightness
int getKbdBrightness();
void setKbdBrightness(int level);
int stepKbdBrightness(int delta);
int toggleKbdBrightness();

// Power Profiles
QString getCurrentPowerProfile();
bool setPowerProfile(const QString &profile);

// Refresh Rate
bool setDisplayRefreshRate(int hz);

// Hardware Tunings
void applyHardwareTunings(const QString &state, const QString &profile = "");

// Battery Care Limit
bool setBatteryLimit(int limit);

// Notifications & KDE OSD
void showKdeOsdBrightness(int percent);
void showKdeOsdKbd(int level);
void showKdeOsdProfile(const QString &profile);
void sendNotification(const QString &title, const QString &msg, const QString &icon = "battery");

} // namespace Hardware
