#include "Config.hpp"
#include "Hardware.hpp"
#include "Translations.hpp"
#include "DBusServices.hpp"
#include "IdleManager.hpp"
#include "LidManager.hpp"
#include "Controller.hpp"
#include "TrayIcon.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QThread>
#include <iostream>

int main(int argc, char *argv[]) {
    I18n::detectLanguage();

    QCommandLineParser parser;
    parser.setApplicationDescription("ASUS ROG Strix Power Manager, Fan Control & Live Wattage Tray Indicator (C++ Native)");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption optOnce("once", "Apply current power settings once and exit");
    QCommandLineOption optStatus("status", "Print current energy telemetry and hardware detection");
    QCommandLineOption optFan("fan", "Set ASUS fan mode (auto, silent, balanced, turbo, max)", "mode");
    QCommandLineOption optProfile("profile", "Set power profile (power-saver, balanced, performance)", "profile");
    QCommandLineOption optBrightness("brightness", "Set screen brightness percentage (1-100)", "pct");
    QCommandLineOption optKbd("kbd", "Set keyboard backlight level (0-3)", "level");
    QCommandLineOption optChargeLimit("charge-limit", "Set battery charge limit percentage (20-100)", "pct");
    QCommandLineOption optRefreshRate("refresh-rate", "Set background telemetry refresh interval in ms (250-5000)", "ms");
    QCommandLineOption optWatch("watch", "Continuously refresh live telemetry in terminal", "interval_sec");
    QCommandLineOption optLang("lang", "Override display language (de/en)", "lang");

    parser.addOption(optOnce);
    parser.addOption(optStatus);
    parser.addOption(optWatch);
    parser.addOption(optFan);
    parser.addOption(optProfile);
    parser.addOption(optBrightness);
    parser.addOption(optKbd);
    parser.addOption(optChargeLimit);
    parser.addOption(optRefreshRate);
    parser.addOption(optLang);

    // Create QApplication early for event loop & QCommandLineParser
    QApplication app(argc, argv);
    app.setApplicationName("rog-power-manager");
#ifdef APP_VERSION
    app.setApplicationVersion(APP_VERSION);
#else
    app.setApplicationVersion("0.4.3");
#endif
    app.setOrganizationName("Mountainhouse Software Solutions");
    app.setOrganizationDomain("mountain-house.dev");
    app.setQuitOnLastWindowClosed(false);

    parser.process(app);

    if (parser.isSet(optLang)) {
        I18n::currentLanguage = parser.value(optLang);
    }

    if (parser.isSet(optFan)) {
        QString fMode = parser.value(optFan);
        Hardware::setFanMode(fMode);
        std::cout << "[FanControl] Fan mode set to: " << fMode.toStdString() << std::endl;
        return 0;
    }

    if (parser.isSet(optProfile)) {
        QString prof = parser.value(optProfile);
        Hardware::applyHardwareTunings(Hardware::getPowerSource(), prof);
        Hardware::setPowerProfile(prof);
        std::cout << "[PowerProfile] Profile set to: " << prof.toStdString() << std::endl;
        return 0;
    }

    if (parser.isSet(optBrightness)) {
        int val = std::clamp(parser.value(optBrightness).toInt(), 1, 100);
        Hardware::setBrightnessPct(val);
        Hardware::showKdeOsdBrightness(val);
        std::cout << "[Brightness] Screen brightness set to: " << val << "%" << std::endl;
        return 0;
    }

    if (parser.isSet(optKbd)) {
        int val = std::clamp(parser.value(optKbd).toInt(), 0, 3);
        Hardware::setKbdBrightness(val);
        Hardware::showKdeOsdKbd(val);
        std::cout << "[Keyboard] Keyboard backlight set to level: " << val << std::endl;
        return 0;
    }

    if (parser.isSet(optChargeLimit)) {
        int val = std::clamp(parser.value(optChargeLimit).toInt(), 20, 100);
        Hardware::setBatteryLimit(val);
        ConfigManager::instance().setChargeLimit(val);
        std::cout << "[Battery] Battery charge limit set and saved to: " << val << "%" << std::endl;
        return 0;
    }

    if (parser.isSet(optRefreshRate)) {
        int val = std::clamp(parser.value(optRefreshRate).toInt(), 250, 5000);
        ConfigManager::instance().setRefreshInterval(val);
        std::cout << "[Config] Telemetry refresh interval set and saved to: " << val << " ms" << std::endl;
        return 0;
    }

    auto printStatus = []() {
        QString src = Hardware::getPowerSource();
        QString srcLabel = (src == "ac") ? I18n::tr("ac_label") : ((src == "usbc") ? I18n::tr("usbc_label") : I18n::tr("bat_label"));
        QString prof = Hardware::getCurrentPowerProfile();
        QString profDisp = I18n::tr("prof_" + prof);
        auto panel = Hardware::getPanelInfo();
        auto tel = Hardware::getPowerTelemetry(panel.currentRate);
        auto fans = Hardware::getFanTelemetry();

        QString sign = (tel.status == "Charging") ? "+" : ((tel.status == "Discharging") ? "-" : "");
        QString stDisp = I18n::tr("st_" + tel.status);
        QString dstDisp = I18n::tr("dst_" + tel.dgpuState);

        QString fMode = Hardware::detectFanMode();
        QString fanCli;
        if (fans.cpuRpm == 0 && fans.gpuRpm == 0) {
            fanCli = QString("%1 (%2)").arg(I18n::tr("fan_status_off"), I18n::tr("fan_mode_" + fMode));
        } else {
            fanCli = QString("CPU %1 RPM | GPU %2 RPM (%3)").arg(fans.cpuRpm).arg(fans.gpuRpm).arg(I18n::tr("fan_mode_" + fMode));
        }

        QString etaCli;
        if (tel.status == "Charging") {
            double targetWh = tel.energyFullWh * (tel.chargeLimit / 100.0);
            double remWh = std::max(0.0, targetWh - tel.energyNowWh);
            if (remWh <= 0.2 || tel.capacity >= tel.chargeLimit) {
                etaCli = QString(" • %1").arg(I18n::tr("eta_almost").arg(tel.chargeLimit));
            } else if (tel.batWatts > 0.5) {
                etaCli = QString(" • %1").arg(I18n::tr("eta_to_limit").arg(remWh / tel.batWatts, 0, 'f', 1).arg(tel.chargeLimit));
            }
        } else if (tel.status == "Discharging") {
            if (tel.batWatts > 0.5 && tel.energyNowWh > 0.0) {
                etaCli = QString(" • %1").arg(I18n::tr("eta_rem").arg(tel.energyNowWh / tel.batWatts, 0, 'f', 1));
            }
        }

        std::cout << I18n::tr("cli_header").toStdString() << std::endl;
        std::cout << I18n::tr("cli_source").arg(srcLabel).toStdString() << std::endl;
        std::cout << I18n::tr("cli_total_in").arg(tel.totalInWatts, 0, 'f', 2).toStdString() << std::endl;
        std::cout << I18n::tr("cli_bat").arg(sign).arg(tel.batWatts, 0, 'f', 2).arg(tel.cRate, 0, 'f', 2).arg(stDisp).arg(tel.capacity).arg(tel.chargeLimit).arg(etaCli).toStdString() << std::endl;
        std::cout << I18n::tr("cli_sys").arg(tel.sysWatts, 6, 'f', 2).arg(tel.apuWatts, 0, 'f', 1).arg(tel.dgpuWatts, 0, 'f', 1).arg(dstDisp).arg(tel.restWatts, 0, 'f', 1).toStdString() << std::endl;
        std::cout << I18n::tr("cli_prof").arg(profDisp).toStdString() << std::endl;
        std::cout << I18n::tr("cli_panel").arg(panel.name, panel.resolution).arg(panel.currentRate).toStdString() << std::endl;
        std::cout << I18n::tr("cli_fan").arg(fanCli).toStdString() << std::endl;
    };

    if (parser.isSet(optWatch)) {
        double intervalSec = 1.0;
        if (!parser.value(optWatch).isEmpty()) {
            intervalSec = std::max(0.25, parser.value(optWatch).toDouble());
        }
        while (true) {
            std::cout << "\033[2J\033[H";
            printStatus();
            std::cout << "\n[rog-power-manager] Live-Telemetrie (" << intervalSec << "s Refresh | Beenden: Strg+C)..." << std::flush;
            QThread::msleep(static_cast<unsigned long>(intervalSec * 1000));
        }
    }

    if (parser.isSet(optStatus)) {
        printStatus();
        return 0;
    }

    PowerManagerController controller;

    if (parser.isSet(optOnce)) {
        controller.applyState(true);
        return 0;
    }

    // Full Tray & Daemon Mode
    DBusServiceManager::instance().registerServices();

    IdleManager idleMgr;
    controller.setIdleManager(&idleMgr);

    LidManager lidMgr(&controller);
    controller.setLidManager(&lidMgr);

    TrayIcon tray(&controller);
    tray.show();

    QObject::connect(&DBusServiceManager::instance(), &DBusServiceManager::shortcutTriggered, [&](const QString &sc) {
        if (sc == "powerProfile") {
            controller.cyclePowerProfile();
            tray.updateTray();
        }
    });

    QObject::connect(&DBusServiceManager::instance(), &DBusServiceManager::powerSourceChanged, [&]() {
        controller.applyState();
        tray.updateTray();
    });

    controller.applyState(true);
    tray.updateTray();

    std::cout << "[ROG Power Manager] Native C++ Qt6 daemon & tray indicator started (Language: "
              << I18n::currentLanguage.toStdString() << ")." << std::endl;

    return app.exec();
}
