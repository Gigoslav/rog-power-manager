#include "TrayIcon.hpp"
#include "Config.hpp"
#include "Hardware.hpp"
#include "Translations.hpp"
#include "SettingsDialog.hpp"
#include "AboutDialog.hpp"
#include "DBusServices.hpp"

#include <QPainter>
#include <QProcess>
#include <QFile>
#include <iostream>

TrayIcon::TrayIcon(PowerManagerController *controller, QObject *parent)
    : QSystemTrayIcon(parent), m_controller(controller) {
    createMenu();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TrayIcon::updateTray);
    m_timer->start(2000);

    connect(m_controller, &PowerManagerController::stateApplied, this, &TrayIcon::updateTray);

    updateTray();
}

QIcon TrayIcon::createWattIcon(const Hardware::PowerTelemetry &tel) {
    const int size = 64;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    int totalIn = static_cast<int>(std::round(tel.totalInWatts));
    int batW = static_cast<int>(std::round(tel.batWatts));
    int sysW = static_cast<int>(std::round(tel.sysWatts));

    QColor bgColor, borderColor;
    QString topText, botText;

    if (tel.status == "Charging") {
        bgColor = QColor(16, 124, 65, 235);    // Emerald green
        borderColor = QColor(52, 211, 153);    // Bright mint
        topText = QString("%1W").arg(totalIn);
        botText = QString("+%1|%2").arg(batW).arg(sysW);
    } else if (tel.status == "Discharging") {
        if (sysW >= 25) {
            bgColor = QColor(185, 28, 28, 235); // Red
            borderColor = QColor(248, 113, 113);
        } else {
            bgColor = QColor(180, 83, 9, 235);  // Amber
            borderColor = QColor(251, 191, 36);
        }
        topText = QString("-%1W").arg(sysW);
        botText = "OUT";
    } else {
        bgColor = QColor(30, 64, 175, 235);    // Blue (Bypass/Limit)
        borderColor = QColor(96, 165, 250);
        topText = QString("%1W").arg(sysW);
        botText = QString("0|%1").arg(sysW);
    }

    painter.setBrush(bgColor);
    painter.setPen(borderColor);
    painter.drawRoundedRect(2, 2, size - 4, size - 4, 12, 12);

    // Profile indicator dot at top-right
    QString curProf = m_controller ? m_controller->cachedProfile() : Hardware::getCurrentPowerProfile();
    QColor dotColor;
    if (curProf == "performance") {
        dotColor = QColor(248, 113, 113); // Coral Red
    } else if (curProf == "balanced") {
        dotColor = QColor(96, 165, 250);  // Sky Blue
    } else {
        dotColor = QColor(52, 211, 153);  // Mint Green
    }
    painter.setBrush(dotColor);
    painter.setPen(QPen(QColor(15, 23, 42, 220), 1.5));
    painter.drawEllipse(size - 13, 5, 8, 8);

    QFont fontTop("Sans Serif", (topText.length() <= 4 ? 16 : 13), QFont::Bold);
    painter.setFont(fontTop);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 4, size, 32), Qt::AlignCenter, topText);

    QFont fontBot("Sans Serif", (botText.length() > 4 ? 10 : 11), QFont::Bold);
    painter.setFont(fontBot);
    painter.setPen(borderColor);
    painter.drawText(QRect(0, 34, size, 24), Qt::AlignCenter, botText);

    painter.end();
    return QIcon(pix);
}

void TrayIcon::createMenu() {
    m_menu = new QMenu();

    m_actHeader = m_menu->addAction(I18n::tr("menu_header"));
    m_actHeader->setEnabled(false);

    m_actTotalIn = m_menu->addAction("...");
    m_actTotalIn->setEnabled(false);

    m_actBat = m_menu->addAction("...");
    m_actBat->setEnabled(false);

    m_actSys = m_menu->addAction("...");
    m_actSys->setEnabled(false);

    m_actSysDetail = m_menu->addAction("...");
    m_actSysDetail->setEnabled(false);

    m_menu->addSeparator();

    m_actState = m_menu->addAction("...");
    m_actState->setEnabled(false);

    m_actAuto = m_menu->addAction(I18n::tr("auto_mode"));
    m_actAuto->setCheckable(true);
    m_actAuto->setChecked(true);
    connect(m_actAuto, &QAction::toggled, [this](bool checked) {
        m_controller->setAutoMode(checked);
        if (checked) {
            m_controller->applyState(true);
            updateTray();
        }
    });

    m_menu->addSeparator();

    m_profileGroup = new QActionGroup(this);
    m_profileGroup->setExclusive(true);

    m_actPs = m_menu->addAction(I18n::tr("menu_ps"));
    m_actPs->setCheckable(true);
    m_profileGroup->addAction(m_actPs);
    connect(m_actPs, &QAction::triggered, [this]() {
        m_actAuto->setChecked(false);
        m_controller->manualSet("power-saver", 60);
        updateTray();
    });

    m_actBal = m_menu->addAction(I18n::tr("menu_bal"));
    m_actBal->setCheckable(true);
    m_profileGroup->addAction(m_actBal);
    connect(m_actBal, &QAction::triggered, [this]() {
        m_actAuto->setChecked(false);
        m_controller->manualSet("balanced", 60);
        updateTray();
    });

    m_actPerf = m_menu->addAction(I18n::tr("menu_perf"));
    m_actPerf->setCheckable(true);
    m_profileGroup->addAction(m_actPerf);
    connect(m_actPerf, &QAction::triggered, [this]() {
        m_actAuto->setChecked(false);
        m_controller->manualSet("performance", 240);
        updateTray();
    });

    m_menu->addSeparator();

    m_actGpuStatus = m_menu->addAction("...");
    m_actGpuStatus->setEnabled(false);

    auto gpuMenu = m_menu->addMenu("🎮 GPU Modus");
    auto actGpuInt = gpuMenu->addAction(I18n::tr("gpu_int_menu"));
    connect(actGpuInt, &QAction::triggered, []() {
        QProcess::execute("supergfxctl", {"-m", "Integrated"});
        Hardware::sendNotification(I18n::tr("gpu_super_title"), I18n::tr("gpu_super_msg").arg("Integrated"), "preferences-system-power-management");
    });
    auto actGpuHyb = gpuMenu->addAction(I18n::tr("gpu_hyb_menu"));
    connect(actGpuHyb, &QAction::triggered, []() {
        QProcess::execute("supergfxctl", {"-m", "Hybrid"});
        Hardware::sendNotification(I18n::tr("gpu_super_title"), I18n::tr("gpu_super_msg").arg("Hybrid"), "preferences-system-power-management");
    });
    auto actGpuDed = gpuMenu->addAction(I18n::tr("gpu_ded_menu"));
    connect(actGpuDed, &QAction::triggered, []() {
        if (QFile::exists("/usr/local/bin/rog-gpu-mux-helper")) {
            QProcess::execute("sudo", {"-n", "/usr/local/bin/rog-gpu-mux-helper", "AsusMuxDgpu"});
            Hardware::sendNotification(I18n::tr("gpu_mux_nv_title"), I18n::tr("gpu_mux_nv_msg"), "preferences-system-power-management");
        }
    });

    // Battery limit submenu
    m_actBatProt = m_menu->addAction("...");
    m_actBatProt->setEnabled(false);

    auto batMenu = m_menu->addMenu("🛡️ Akku-Schutz Limit");
    m_batLimitGroup = new QActionGroup(batMenu);
    m_batLimitGroup->setExclusive(true);

    m_actLimStop = batMenu->addAction(I18n::tr("menu_lim_stop"));
    connect(m_actLimStop, &QAction::triggered, [this]() {
        auto tel = Hardware::getPowerTelemetry();
        Hardware::setBatteryLimit(tel.capacity);
        ConfigManager::instance().setChargeLimit(tel.capacity);
        Hardware::sendNotification(I18n::tr("bat_prot_title"), I18n::tr("stop_now_msg").arg(tel.capacity));
        updateTray();
    });

    m_actLim60 = batMenu->addAction(I18n::tr("menu_lim_60"));
    m_actLim60->setCheckable(true);
    m_batLimitGroup->addAction(m_actLim60);
    connect(m_actLim60, &QAction::triggered, [this]() {
        Hardware::setBatteryLimit(60);
        ConfigManager::instance().setChargeLimit(60);
        Hardware::sendNotification(I18n::tr("bat_prot_title"), I18n::tr("bat_prot_msg").arg(I18n::tr("lbl_lim_60")).arg(60));
        updateTray();
    });

    m_actLim80 = batMenu->addAction(I18n::tr("menu_lim_80"));
    m_actLim80->setCheckable(true);
    m_batLimitGroup->addAction(m_actLim80);
    connect(m_actLim80, &QAction::triggered, [this]() {
        Hardware::setBatteryLimit(80);
        ConfigManager::instance().setChargeLimit(80);
        Hardware::sendNotification(I18n::tr("bat_prot_title"), I18n::tr("bat_prot_msg").arg(I18n::tr("lbl_lim_80")).arg(80));
        updateTray();
    });

    m_actLim100 = batMenu->addAction(I18n::tr("menu_lim_100"));
    m_actLim100->setCheckable(true);
    m_batLimitGroup->addAction(m_actLim100);
    connect(m_actLim100, &QAction::triggered, [this]() {
        Hardware::setBatteryLimit(100);
        ConfigManager::instance().setChargeLimit(100);
        Hardware::sendNotification(I18n::tr("bat_prot_title"), I18n::tr("bat_prot_msg").arg(I18n::tr("lbl_lim_100")).arg(100));
        updateTray();
    });

    m_menu->addSeparator();

    // Fan Control
    m_fanMenu = m_menu->addMenu(I18n::tr("menu_fan").arg(I18n::tr("fan_status_off")));
    m_fanGroup = new QActionGroup(m_fanMenu);
    m_fanGroup->setExclusive(true);

    QList<QPair<QString, QString>> fanModes = {
        {"auto", "fan_mode_auto"},
        {"silent", "fan_mode_silent"},
        {"balanced", "fan_mode_balanced"},
        {"turbo", "fan_mode_turbo"},
        {"max", "fan_mode_max"}
    };

    for (const auto &fm : fanModes) {
        auto act = m_fanMenu->addAction(I18n::tr(fm.second));
        act->setCheckable(true);
        if (fm.first == Hardware::currentFanMode) act->setChecked(true);
        m_fanGroup->addAction(act);
        m_fanActions[fm.first] = act;

        connect(act, &QAction::triggered, [this, m = fm.first]() {
            Hardware::setFanMode(m);
            Hardware::sendNotification(I18n::tr("fan_set_title"), I18n::tr("fan_set_msg").arg(I18n::tr("fan_mode_" + m)));
            updateTray();
        });
    }

    m_menu->addSeparator();

    // Brightness submenu
    auto brightMenu = m_menu->addMenu(I18n::tr("menu_brightness"));
    QList<int> brightSteps = {100, 75, 50, 25, 10};
    for (int bs : brightSteps) {
        auto actB = brightMenu->addAction(QString("%1%").arg(bs));
        connect(actB, &QAction::triggered, [bs]() {
            Hardware::setBrightnessPct(bs);
            Hardware::showKdeOsdBrightness(bs);
        });
    }

    // Keyboard backlight submenu
    auto kbdMenu = m_menu->addMenu(I18n::tr("menu_kbd"));
    QList<QPair<int, QString>> kbdLevels = {
        {3, "Stufe 3 (Hell)"},
        {2, "Stufe 2 (Mittel)"},
        {1, "Stufe 1 (Schwach)"},
        {0, "Aus"}
    };
    for (const auto &kl : kbdLevels) {
        auto actK = kbdMenu->addAction(kl.second);
        connect(actK, &QAction::triggered, [lvl = kl.first]() {
            Hardware::setKbdBrightness(lvl);
            Hardware::showKdeOsdKbd(lvl);
        });
    }

    m_menu->addSeparator();

    // Keep Awake
    m_actInhibit = m_menu->addAction(I18n::tr("menu_inhibit"));
    m_actInhibit->setCheckable(true);
    connect(m_actInhibit, &QAction::toggled, [](bool checked) {
        InhibitAdaptor::setManualInhibit(checked);
        if (checked) {
            Hardware::sendNotification(I18n::tr("inhibit_on_title"), I18n::tr("inhibit_on_msg"), "caffeine");
        } else {
            Hardware::sendNotification(I18n::tr("inhibit_off_title"), I18n::tr("inhibit_off_msg"), "battery");
        }
    });

    // Session
    auto actLock = m_menu->addAction(I18n::tr("menu_lock"));
    connect(actLock, &QAction::triggered, []() { QProcess::startDetached("loginctl", {"lock-session"}); });

    auto actSuspend = m_menu->addAction(I18n::tr("menu_suspend"));
    connect(actSuspend, &QAction::triggered, []() { QProcess::startDetached("loginctl", {"suspend"}); });

    auto actHibernate = m_menu->addAction(I18n::tr("menu_hibernate"));
    connect(actHibernate, &QAction::triggered, []() { QProcess::startDetached("loginctl", {"hibernate"}); });

    m_menu->addSeparator();

    auto actSettings = m_menu->addAction(I18n::tr("menu_settings"));
    connect(actSettings, &QAction::triggered, [this]() {
        SettingsDialog dlg(m_controller);
        dlg.exec();
    });

    auto actAbout = m_menu->addAction(I18n::tr("menu_about"));
    connect(actAbout, &QAction::triggered, []() {
        AboutDialog dlg;
        dlg.exec();
    });

    setContextMenu(m_menu);
}

void TrayIcon::updateTray() {
    m_tickCount++;

    if (m_tickCount % 3 == 1) {
        m_controller->applyState();
    }

    auto panel = m_controller->cachedPanel();
    double rateVal = (panel.currentRate > 0) ? panel.currentRate : 60.0;
    auto tel = Hardware::getPowerTelemetry(rateVal);
    QString src = Hardware::getPowerSource();
    QString prof = m_controller->cachedProfile();
    QString profDisp = I18n::tr("prof_" + prof);
    QString srcLabel = (src == "ac") ? I18n::tr("ac_label") : ((src == "usbc") ? I18n::tr("usbc_label") : I18n::tr("bat_label"));

    // Alerts
    if (tel.status == "Discharging") {
        auto aCfg = ConfigManager::instance().getData().value("alerts").toObject();
        int lowThresh = aCfg.value("low_battery_percent").toInt(10);
        int critThresh = aCfg.value("critical_battery_percent").toInt(5);

        if (tel.capacity <= critThresh && !m_critBatAlerted) {
            Hardware::sendNotification(I18n::tr("bat_crit_title"), I18n::tr("bat_crit_msg").arg(tel.capacity));
            m_critBatAlerted = true;
            QString cmdCrit = aCfg.value("on_critical_command").toString();
            if (!cmdCrit.trimmed().isEmpty()) {
                ConfigManager::instance().runCommand(cmdCrit, {{"ROG_CAP", QString::number(tel.capacity)}});
            }
            ConfigManager::instance().runHook("battery_critical", {QString::number(tel.capacity)});
            QTimer::singleShot(15000, []() { QProcess::startDetached("loginctl", {"suspend"}); });
        } else if (tel.capacity <= lowThresh && !m_lowBatAlerted) {
            Hardware::sendNotification(I18n::tr("bat_low_title"), I18n::tr("bat_low_msg").arg(tel.capacity));
            m_lowBatAlerted = true;
            QString cmdLow = aCfg.value("on_low_command").toString();
            if (!cmdLow.trimmed().isEmpty()) {
                ConfigManager::instance().runCommand(cmdLow, {{"ROG_CAP", QString::number(tel.capacity)}});
            }
            ConfigManager::instance().runHook("battery_low", {QString::number(tel.capacity)});
        }
    } else {
        m_lowBatAlerted = false;
        m_critBatAlerted = false;
    }

    // Lines
    QString lineIn, lineBat, lineSys;
    QString etaStr;
    if (tel.status == "Charging") {
        double targetWh = tel.energyFullWh * (tel.chargeLimit / 100.0);
        double remWh = std::max(0.0, targetWh - tel.energyNowWh);
        if (remWh <= 0.2 || tel.capacity >= tel.chargeLimit) {
            etaStr = QString(" • %1").arg(I18n::tr("eta_almost").arg(tel.chargeLimit));
        } else if (tel.batWatts > 0.5) {
            etaStr = QString(" • %1").arg(I18n::tr("eta_to_limit").arg(remWh / tel.batWatts, 0, 'f', 1).arg(tel.chargeLimit));
        }
        lineIn = QString("🔌 Eingang Gesamt: %1 W  [%2]").arg(tel.totalInWatts, 5, 'f', 1).arg(srcLabel);
        lineBat = QString("├── 🔋 Akku:     +%1 W  (%2 C • Lädt • %3%/%4%%5)").arg(tel.batWatts, 5, 'f', 1).arg(tel.cRate, 0, 'f', 2).arg(tel.capacity).arg(tel.chargeLimit).arg(etaStr);
        lineSys = QString("└── 💻 System:    %1 W  (Laufender Betrieb)").arg(tel.sysWatts, 5, 'f', 1);
    } else if (tel.status == "Discharging") {
        if (tel.batWatts > 0.5 && tel.energyNowWh > 0.0) {
            etaStr = QString(" • %1").arg(I18n::tr("eta_rem").arg(tel.energyNowWh / tel.batWatts, 0, 'f', 1));
        }
        lineIn = "🔌 Eingang Gesamt:   0.0 W  [Kein Netzteil]";
        lineBat = QString("├── 🔋 Akku:     -%1 W  (%2 C • Entlädt • %3%%4)").arg(tel.batWatts, 5, 'f', 1).arg(tel.cRate, 0, 'f', 2).arg(tel.capacity).arg(etaStr);
        lineSys = QString("└── 💻 System:    %1 W  (Aus dem Akku)").arg(tel.sysWatts, 5, 'f', 1);
    } else {
        lineIn = QString("🔌 Eingang Gesamt: %1 W  [%2]").arg(tel.totalInWatts, 5, 'f', 1).arg(srcLabel);
        lineBat = QString("├── 🔋 Akku:        0.0 W  (0.00 C • %1% • Direktbetrieb bei %2% Limit)").arg(tel.capacity).arg(tel.chargeLimit);
        lineSys = QString("└── 💻 System:    %1 W  (Direkt vom Netzteil)").arg(tel.sysWatts, 5, 'f', 1);
    }

    QString lineDetail = QString("       ↳ CPU/iGPU: %1W | RTX 3060: %2W | Bildschirm/Mainboard: %3W")
                             .arg(tel.apuWatts, 0, 'f', 1)
                             .arg(tel.dgpuWatts, 0, 'f', 1)
                             .arg(tel.restWatts, 0, 'f', 1);

    setIcon(createWattIcon(tel));

    auto fans = Hardware::getFanTelemetry();
    QString fanStatStr = (fans.cpuRpm == 0 && fans.gpuRpm == 0) ? I18n::tr("fan_status_off") : QString("CPU %1 | GPU %2 RPM").arg(fans.cpuRpm).arg(fans.gpuRpm);
    m_fanMenu->setTitle(I18n::tr("menu_fan").arg(fanStatStr));

    QString fMode = (Hardware::currentFanMode == "auto") ? "auto" : Hardware::detectFanMode();
    if (m_fanActions.contains(fMode)) {
        m_fanActions[fMode]->setChecked(true);
    }

    if (prof == "power-saver") {
        m_actPs->setChecked(true);
    } else if (prof == "balanced") {
        m_actBal->setChecked(true);
    } else if (prof == "performance") {
        m_actPerf->setChecked(true);
    }

    m_actTotalIn->setText(lineIn);
    m_actBat->setText(lineBat);
    m_actSys->setText(lineSys);
    m_actSysDetail->setText(lineDetail);
    m_actState->setText(QString("⚡ Aktives Profil: %1 (%2 Hz)").arg(profDisp).arg(static_cast<int>(panel.currentRate)));
    m_actGpuStatus->setText(QString("🎮 GPU: %1").arg(tel.dgpuState == "suspended" ? I18n::tr("gpu_sleep") : I18n::tr("gpu_active")));
    m_actBatProt->setText(QString("🛡️ Akku-Schutz: Limit %1% (%2 C)").arg(tel.chargeLimit).arg(tel.cRate, 0, 'f', 2));
    if (m_actLim60) m_actLim60->setChecked(tel.chargeLimit == 60);
    if (m_actLim80) m_actLim80->setChecked(tel.chargeLimit == 80);
    if (m_actLim100) m_actLim100->setChecked(tel.chargeLimit == 100);

    QString tooltip = QString(
        "%1\n%2\n%3\n%4\n%5\n──────────────────────────\n%6\n%7\n%8\n%9"
    ).arg(I18n::tr("tt_header"))
     .arg(lineIn)
     .arg(lineBat)
     .arg(lineSys)
     .arg(lineDetail)
     .arg(I18n::tr("tt_fan").arg(fanStatStr, I18n::tr("fan_mode_" + fMode)))
     .arg(I18n::tr("tt_limit").arg(tel.chargeLimit).arg(tel.cRate, 0, 'f', 2))
     .arg(I18n::tr("tt_profile").arg(profDisp, QString::number(panel.currentRate) + " Hz"))
     .arg(I18n::tr("tt_gpu").arg(tel.dgpuState == "suspended" ? I18n::tr("gpu_sleep") : I18n::tr("gpu_active")));

    setToolTip(tooltip);
}
