#include "SettingsDialog.hpp"
#include "Config.hpp"
#include "Translations.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

SettingsDialog::SettingsDialog(PowerManagerController *controller, QWidget *parent)
    : QDialog(parent), m_controller(controller) {
    setupUi();
}

void SettingsDialog::setupUi() {
    setWindowTitle(I18n::tr("settings_title"));
    setMinimumSize(640, 520);
    setStyleSheet(
        "QDialog { background-color: #121317; color: #E5E7EB; font-family: 'Segoe UI', 'Ubuntu', sans-serif; }"
        "QTabWidget::pane { border: 1px solid #2D3139; border-radius: 8px; background-color: #1A1C23; padding: 8px; }"
        "QTabBar::tab { background: #181920; color: #9CA3AF; padding: 8px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; font-weight: bold; }"
        "QTabBar::tab:selected { background: #1A1C23; color: #38BDF8; border-bottom: 2px solid #38BDF8; }"
        "QGroupBox { font-weight: bold; border: 1px solid #2D3139; border-radius: 6px; margin-top: 10px; padding: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #38BDF8; }"
        "QLabel { color: #D1D5DB; font-size: 13px; }"
        "QComboBox, QSpinBox, QLineEdit { background-color: #111216; color: #F3F4F6; border: 1px solid #374151; border-radius: 4px; padding: 5px; font-size: 13px; }"
        "QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border: 1px solid #38BDF8; }"
        "QPushButton { background-color: #2563EB; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: bold; font-size: 13px; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton#cancelBtn { background-color: #374151; }"
        "QPushButton#cancelBtn:hover { background-color: #4B5563; }"
    );

    auto mainLayout = new QVBoxLayout(this);

    auto tabs = new QTabWidget(this);
    tabs->addTab(createProfilesTab(), I18n::tr("tab_profiles"));
    tabs->addTab(createIdleTab(), I18n::tr("tab_idle"));
    tabs->addTab(createLidTab(), I18n::tr("tab_lid"));
    tabs->addTab(createAutomationsTab(), I18n::tr("tab_automations"));
    mainLayout->addWidget(tabs);

    auto btnBox = new QHBoxLayout();
    btnBox->addStretch();

    auto cancelBtn = new QPushButton(I18n::tr("btn_cancel"), this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnBox->addWidget(cancelBtn);

    auto saveBtn = new QPushButton(I18n::tr("btn_save"), this);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveAndApply);
    btnBox->addWidget(saveBtn);

    mainLayout->addLayout(btnBox);
}

QWidget* SettingsDialog::createProfilesTab() {
    auto w = new QWidget(this);
    auto layout = new QVBoxLayout(w);

    auto cfg = ConfigManager::instance().getData().value("profiles").toObject();
    QStringList states = {"battery", "usbc", "ac"};
    QMap<QString, QString> labels = {
        {"battery", I18n::tr("bat_label")},
        {"usbc", I18n::tr("usbc_label")},
        {"ac", I18n::tr("ac_label")}
    };

    for (const auto &state : states) {
        auto stateCfg = cfg.value(state).toObject();
        auto gb = new QGroupBox(labels[state], w);
        auto fLayout = new QFormLayout(gb);

        auto profBox = new QComboBox(gb);
        profBox->addItem("power-saver (" + I18n::tr("prof_power-saver") + ")", "power-saver");
        profBox->addItem("balanced (" + I18n::tr("prof_balanced") + ")", "balanced");
        profBox->addItem("performance (" + I18n::tr("prof_performance") + ")", "performance");
        profBox->setCurrentIndex(profBox->findData(stateCfg.value("profile").toString("balanced")));

        auto fanBox = new QComboBox(gb);
        fanBox->addItem("auto (" + I18n::tr("fan_mode_auto") + ")", "auto");
        fanBox->addItem("silent (" + I18n::tr("fan_mode_silent") + ")", "silent");
        fanBox->addItem("balanced (" + I18n::tr("fan_mode_balanced") + ")", "balanced");
        fanBox->addItem("turbo (" + I18n::tr("fan_mode_turbo") + ")", "turbo");
        fanBox->setCurrentIndex(fanBox->findData(stateCfg.value("fan_mode").toString("auto")));

        auto hzBox = new QComboBox(gb);
        hzBox->addItem("60 Hz", 60);
        hzBox->addItem("240 Hz", 240);
        hzBox->setCurrentIndex(hzBox->findData(stateCfg.value("hz").toInt(60)));

        auto brightSpin = new QSpinBox(gb);
        brightSpin->setRange(10, 100);
        brightSpin->setSuffix("%");
        brightSpin->setValue(stateCfg.value("brightness").toInt(50));

        auto kbdSpin = new QSpinBox(gb);
        kbdSpin->setRange(0, 3);
        kbdSpin->setValue(stateCfg.value("kbd").toInt(1));

        fLayout->addRow("Energieprofil:", profBox);
        fLayout->addRow("Lüftermodus:", fanBox);
        fLayout->addRow("Bildwiederholrate:", hzBox);
        fLayout->addRow("Helligkeit:", brightSpin);
        fLayout->addRow("Tastaturbeleuchtung (0-3):", kbdSpin);

        m_profileMap[state] = {profBox, fanBox, hzBox, brightSpin, kbdSpin};
        layout->addWidget(gb);
    }
    layout->addStretch();
    return w;
}

QWidget* SettingsDialog::createIdleTab() {
    auto w = new QWidget(this);
    auto layout = new QVBoxLayout(w);

    auto cfg = ConfigManager::instance().getData().value("idle").toObject();
    QStringList states = {"battery", "usbc", "ac"};
    QMap<QString, QString> labels = {
        {"battery", I18n::tr("bat_label")},
        {"usbc", I18n::tr("usbc_label")},
        {"ac", I18n::tr("ac_label")}
    };

    for (const auto &state : states) {
        auto gb = new QGroupBox(labels[state], w);
        auto fLayout = new QFormLayout(gb);

        auto dimSpin = new QSpinBox(gb);
        dimSpin->setRange(0, 3600);
        dimSpin->setSuffix(" Sek. (0=Nie)");
        dimSpin->setValue(cfg.value("dim_" + state).toInt(180));

        auto dpmsSpin = new QSpinBox(gb);
        dpmsSpin->setRange(0, 3600);
        dpmsSpin->setSuffix(" Sek. (0=Nie)");
        dpmsSpin->setValue(cfg.value("dpms_" + state).toInt(300));

        auto susSpin = new QSpinBox(gb);
        susSpin->setRange(0, 7200);
        susSpin->setSuffix(" Sek. (0=Nie)");
        susSpin->setValue(cfg.value("suspend_" + state).toInt(600));

        fLayout->addRow("Display abdunkeln nach:", dimSpin);
        fLayout->addRow("Display ausschalten nach:", dpmsSpin);
        fLayout->addRow("Automatischer Standby nach:", susSpin);

        m_idleMap[state] = {dimSpin, dpmsSpin, susSpin};
        layout->addWidget(gb);
    }
    layout->addStretch();
    return w;
}

QWidget* SettingsDialog::createLidTab() {
    auto w = new QWidget(this);
    auto layout = new QVBoxLayout(w);

    auto lidCfg = ConfigManager::instance().getData().value("lid").toObject();

    auto gbActions = new QGroupBox(I18n::tr("lid_group_actions"), w);
    auto fLayout = new QFormLayout(gbActions);

    QList<QPair<QString, QString>> actions = {
        {"suspend", I18n::tr("lid_act_suspend")},
        {"max_powersave", I18n::tr("lid_act_max_powersave")},
        {"lock_max_powersave", I18n::tr("lid_act_lock_max_powersave")},
        {"turn_off_all", I18n::tr("lid_act_turn_off_all")},
        {"turn_off_screen", I18n::tr("lid_act_turn_off_screen")},
        {"lock_and_turn_off_all", I18n::tr("lid_act_lock_and_turn_off_all")},
        {"lock_and_turn_off", I18n::tr("lid_act_lock_and_turn_off")},
        {"lock", I18n::tr("lid_act_lock")},
        {"hibernate", I18n::tr("lid_act_hibernate")},
        {"nothing", I18n::tr("lid_act_nothing")}
    };

    auto setupCombo = [&](QComboBox *cb, const QString &currentVal) {
        for (const auto &p : actions) {
            cb->addItem(p.second, p.first);
        }
        int idx = cb->findData(currentVal);
        if (idx >= 0) cb->setCurrentIndex(idx);
    };

    m_lidActionBattery = new QComboBox(gbActions);
    setupCombo(m_lidActionBattery, lidCfg.value("action_battery").toString("suspend"));

    m_lidActionUsbc = new QComboBox(gbActions);
    setupCombo(m_lidActionUsbc, lidCfg.value("action_usbc").toString("turn_off_all"));

    m_lidActionAc = new QComboBox(gbActions);
    setupCombo(m_lidActionAc, lidCfg.value("action_ac").toString("turn_off_all"));

    fLayout->addRow(I18n::tr("bat_label") + ":", m_lidActionBattery);
    fLayout->addRow(I18n::tr("usbc_label") + ":", m_lidActionUsbc);
    fLayout->addRow(I18n::tr("ac_label") + ":", m_lidActionAc);
    layout->addWidget(gbActions);

    auto gbOpt = new QGroupBox(I18n::tr("lid_group_options"), w);
    auto vOpt = new QVBoxLayout(gbOpt);
    m_lidClamshell = new QCheckBox(I18n::tr("lid_clamshell_label"), gbOpt);
    m_lidClamshell->setChecked(lidCfg.value("inhibit_when_external_monitor").toBool(true));
    vOpt->addWidget(m_lidClamshell);

    m_lidMaxPowerSave = new QCheckBox(I18n::tr("lid_max_powersave_label"), gbOpt);
    m_lidMaxPowerSave->setChecked(lidCfg.value("enable_max_powersave_while_closed").toBool(false));
    vOpt->addWidget(m_lidMaxPowerSave);

    auto ecoHint = new QLabel(I18n::tr("lid_max_powersave_desc"), gbOpt);
    ecoHint->setWordWrap(true);
    ecoHint->setStyleSheet("color: #9CA3AF; font-size: 11px; margin-left: 20px;");
    vOpt->addWidget(ecoHint);

    layout->addWidget(gbOpt);

    auto gbAuto = new QGroupBox(I18n::tr("lid_group_automations"), w);
    auto fAuto = new QFormLayout(gbAuto);
    m_lidCmdClose = new QLineEdit(lidCfg.value("command_on_close").toString(), gbAuto);
    m_lidCmdOpen = new QLineEdit(lidCfg.value("command_on_open").toString(), gbAuto);
    fAuto->addRow(I18n::tr("lid_cmd_close_label"), m_lidCmdClose);
    fAuto->addRow(I18n::tr("lid_cmd_open_label"), m_lidCmdOpen);

    auto hintLabel = new QLabel(I18n::tr("lid_hooks_hint"), gbAuto);
    hintLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; margin-top: 4px;");
    fAuto->addRow("", hintLabel);

    layout->addWidget(gbAuto);

    layout->addStretch();
    return w;
}

QWidget* SettingsDialog::createAutomationsTab() {
    auto w = new QWidget(this);
    auto layout = new QVBoxLayout(w);

    auto profCfg = ConfigManager::instance().getData().value("profiles").toObject();
    auto alertCfg = ConfigManager::instance().getData().value("alerts").toObject();

    auto gbState = new QGroupBox("Befehle bei Statuswechsel (Shell)", w);
    auto fLayout1 = new QFormLayout(gbState);

    m_cmdBattery = new QLineEdit(profCfg.value("battery").toObject().value("command").toString(), gbState);
    m_cmdUsbc = new QLineEdit(profCfg.value("usbc").toObject().value("command").toString(), gbState);
    m_cmdAc = new QLineEdit(profCfg.value("ac").toObject().value("command").toString(), gbState);

    fLayout1->addRow("Bei Wechsel auf Akku:", m_cmdBattery);
    fLayout1->addRow("Bei Wechsel auf USB-C:", m_cmdUsbc);
    fLayout1->addRow("Bei Wechsel auf Netzteil:", m_cmdAc);
    layout->addWidget(gbState);

    auto gbAlerts = new QGroupBox("Akkustand-Warnungen & Schutz", w);
    auto fLayout2 = new QFormLayout(gbAlerts);

    m_lowPercent = new QSpinBox(gbAlerts);
    m_lowPercent->setRange(5, 30);
    m_lowPercent->setSuffix("%");
    m_lowPercent->setValue(alertCfg.value("low_battery_percent").toInt(10));

    m_cmdLow = new QLineEdit(alertCfg.value("on_low_command").toString(), gbAlerts);

    m_critPercent = new QSpinBox(gbAlerts);
    m_critPercent->setRange(2, 15);
    m_critPercent->setSuffix("%");
    m_critPercent->setValue(alertCfg.value("critical_battery_percent").toInt(5));

    m_cmdCrit = new QLineEdit(alertCfg.value("on_critical_command").toString(), gbAlerts);

    fLayout2->addRow("Schwacher Akku ab:", m_lowPercent);
    fLayout2->addRow("Befehl bei schwachem Akku:", m_cmdLow);
    fLayout2->addRow("Kritischer Akku (Not-Standby) ab:", m_critPercent);
    fLayout2->addRow("Befehl bei kritischem Akku:", m_cmdCrit);
    layout->addWidget(gbAlerts);

    auto openHooksBtn = new QPushButton(I18n::tr("btn_open_hooks"), w);
    connect(openHooksBtn, &QPushButton::clicked, this, &SettingsDialog::openHooksFolder);
    layout->addWidget(openHooksBtn);

    layout->addStretch();
    return w;
}

void SettingsDialog::openHooksFolder() {
    QString hDir = ConfigManager::instance().hooksDir();
    QDir().mkpath(hDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(hDir));
}

void SettingsDialog::saveAndApply() {
    auto root = ConfigManager::instance().getData();

    auto profiles = root.value("profiles").toObject();
    for (auto it = m_profileMap.constBegin(); it != m_profileMap.constEnd(); ++it) {
        auto obj = profiles.value(it.key()).toObject();
        obj["profile"] = it.value().profile->currentData().toString();
        obj["fan_mode"] = it.value().fan->currentData().toString();
        obj["hz"] = it.value().hz->currentData().toInt();
        obj["brightness"] = it.value().brightness->value();
        obj["kbd"] = it.value().kbd->value();
        if (it.key() == "battery") obj["command"] = m_cmdBattery->text();
        else if (it.key() == "usbc") obj["command"] = m_cmdUsbc->text();
        else if (it.key() == "ac") obj["command"] = m_cmdAc->text();
        profiles[it.key()] = obj;
    }
    root["profiles"] = profiles;

    auto idle = root.value("idle").toObject();
    for (auto it = m_idleMap.constBegin(); it != m_idleMap.constEnd(); ++it) {
        idle["dim_" + it.key()] = it.value().dim->value();
        idle["dpms_" + it.key()] = it.value().dpms->value();
        idle["suspend_" + it.key()] = it.value().suspend->value();
    }
    root["idle"] = idle;

    auto alerts = root.value("alerts").toObject();
    alerts["low_battery_percent"] = m_lowPercent->value();
    alerts["on_low_command"] = m_cmdLow->text();
    alerts["critical_battery_percent"] = m_critPercent->value();
    alerts["on_critical_command"] = m_cmdCrit->text();
    root["alerts"] = alerts;

    auto lid = root.value("lid").toObject();
    if (m_lidActionBattery) lid["action_battery"] = m_lidActionBattery->currentData().toString();
    if (m_lidActionUsbc) lid["action_usbc"] = m_lidActionUsbc->currentData().toString();
    if (m_lidActionAc) lid["action_ac"] = m_lidActionAc->currentData().toString();
    if (m_lidClamshell) lid["inhibit_when_external_monitor"] = m_lidClamshell->isChecked();
    if (m_lidMaxPowerSave) lid["enable_max_powersave_while_closed"] = m_lidMaxPowerSave->isChecked();
    if (m_lidCmdClose) lid["command_on_close"] = m_lidCmdClose->text().trimmed();
    if (m_lidCmdOpen) lid["command_on_open"] = m_lidCmdOpen->text().trimmed();
    root["lid"] = lid;

    ConfigManager::instance().setData(root);

    if (m_controller) {
        m_controller->applyState(true);
    }

    accept();
}
