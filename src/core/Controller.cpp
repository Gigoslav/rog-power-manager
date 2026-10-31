#include "Controller.hpp"
#include "Config.hpp"
#include "Translations.hpp"

#include <iostream>

PowerManagerController::PowerManagerController(bool verbose, QObject *parent)
    : QObject(parent), m_verbose(verbose) {
    m_cachedPanel = Hardware::getPanelInfo();
    m_cachedProfile = Hardware::getCurrentPowerProfile();
}

void PowerManagerController::applyState(bool force) {
    if (!m_autoMode && !force) return;

    QString newState = Hardware::getPowerSource();
    bool stateChanged = (m_lastState.isEmpty() || m_lastState != newState);

    if (!stateChanged && !force) {
        m_cachedPanel = Hardware::getPanelInfo();
        m_cachedProfile = Hardware::getCurrentPowerProfile();
        return;
    }

    auto profilesCfg = ConfigManager::instance().getData().value("profiles").toObject();
    auto cfg = profilesCfg.value(newState).toObject();

    QString prof = cfg.value("profile").toString(newState == "battery" ? "power-saver" : (newState == "usbc" ? "balanced" : "performance"));
    int hz = cfg.value("hz").toInt(newState == "ac" ? 240 : 60);
    int brightness = cfg.value("brightness").toInt(newState == "ac" ? 100 : (newState == "usbc" ? 75 : 50));
    int kbd = cfg.value("kbd").toInt(newState == "ac" ? 3 : 1);
    QString fanMode = cfg.value("fan_mode").toString("auto");

    Hardware::setBrightnessPct(brightness);
    Hardware::setKbdBrightness(kbd);
    Hardware::applyHardwareTunings(newState, prof);
    Hardware::setPowerProfile(prof);
    Hardware::setDisplayRefreshRate(hz);
    Hardware::setBatteryLimit(ConfigManager::instance().getChargeLimit());

    if (Hardware::currentFanMode == "auto") {
        Hardware::setFanMode(fanMode == "auto" ? (newState == "battery" ? "silent" : (newState == "usbc" ? "balanced" : "turbo")) : fanMode);
        Hardware::currentFanMode = "auto";
    }

    if (m_idleMgr) {
        m_idleMgr->updateState(newState);
    }

    if (!m_lastState.isEmpty() && m_lastState != newState) {
        QString cmd = cfg.value("command").toString();
        if (!cmd.trimmed().isEmpty()) {
            QMap<QString, QString> env = {{"ROG_POWER_STATE", newState}, {"ROG_PREV_STATE", m_lastState}};
            ConfigManager::instance().runCommand(cmd, env);
        }
        ConfigManager::instance().runHook("on_state_change", {newState, m_lastState});
    }

    m_cachedPanel = Hardware::getPanelInfo();
    m_cachedProfile = Hardware::getCurrentPowerProfile();
    bool displayOk = (std::abs(m_cachedPanel.currentRate - hz) < 1.0);

    if (displayOk || m_lastState.isEmpty()) {
        m_lastState = newState;
    }

    if (m_verbose) {
        std::cout << "[AutoPower] State: " << newState.toUpper().toStdString()
                  << " | Profile: " << prof.toStdString()
                  << " | Refresh Rate: " << hz << "Hz"
                  << " (Display OK: " << (displayOk ? "true" : "false") << ")" << std::endl;
    }

    QString prefix = (newState == "battery") ? "bat" : newState;
    QString title = I18n::tr(prefix + "_title");
    QString msg = I18n::tr(prefix + "_msg");
    QString icon = (newState == "battery") ? "battery-good" : "ac-adapter";
    Hardware::sendNotification(title, msg, icon);

    emit stateApplied();
}

void PowerManagerController::cyclePowerProfile() {
    QString cur = Hardware::getCurrentPowerProfile();
    QStringList cycle = {"power-saver", "balanced", "performance"};
    int idx = cycle.indexOf(cur);
    QString nextProf = (idx >= 0) ? cycle[(idx + 1) % cycle.size()] : "balanced";

    m_autoMode = false;
    QString curState = Hardware::getPowerSource();
    Hardware::applyHardwareTunings(curState, nextProf);
    Hardware::setPowerProfile(nextProf);

    QMap<QString, QString> fanMap = {{"power-saver", "silent"}, {"balanced", "balanced"}, {"performance", "turbo"}};
    QMap<QString, int> hzMap = {{"power-saver", 60}, {"balanced", 60}, {"performance", 240}};

    if (fanMap.contains(nextProf)) {
        Hardware::setFanMode(fanMap[nextProf]);
    }
    if (hzMap.contains(nextProf)) {
        Hardware::setDisplayRefreshRate(hzMap[nextProf]);
    }

    m_cachedProfile = nextProf;
    m_cachedPanel = Hardware::getPanelInfo();
    Hardware::showKdeOsdProfile(nextProf);

    emit stateApplied();
}

void PowerManagerController::manualSet(const QString &prof, int hz) {
    m_autoMode = false;
    QString curState = Hardware::getPowerSource();
    Hardware::applyHardwareTunings(curState, prof);
    Hardware::setPowerProfile(prof);
    Hardware::setDisplayRefreshRate(hz);

    QMap<QString, QString> fanMap = {{"power-saver", "silent"}, {"balanced", "balanced"}, {"performance", "turbo"}};
    if (fanMap.contains(prof)) {
        Hardware::setFanMode(fanMap[prof]);
    }

    m_cachedProfile = Hardware::getCurrentPowerProfile();
    m_cachedPanel = Hardware::getPanelInfo();

    QString title = I18n::tr("manual_title");
    QString msg = QString("%1 | %2 Hz").arg(I18n::tr("prof_" + prof)).arg(hz);
    Hardware::sendNotification(title, msg, "power-profile-balanced");

    emit stateApplied();
}
