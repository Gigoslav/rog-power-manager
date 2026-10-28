#include "LidManager.hpp"
#include "Hardware.hpp"
#include "Config.hpp"
#include "Translations.hpp"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnection>
#include <QProcess>
#include <iostream>

LidManager::LidManager(QObject *parent) : QObject(parent) {
    acquireLogindInhibitor();

    m_lastLidClosed = Hardware::isLidClosed();

    // Connect to UPower DBus properties changed
    QDBusConnection::systemBus().connect(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onUPowerPropertiesChanged(QString, QVariantMap, QStringList))
    );

    // Poll timer as reliable fallback
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &LidManager::pollLid);
    m_pollTimer->start(1500);

    std::cout << "[LidManager] Initialized (Initial lid state: " << (m_lastLidClosed ? "CLOSED" : "OPEN") << ")" << std::endl;
}

LidManager::~LidManager() {
    releaseLogindInhibitor();
}

void LidManager::acquireLogindInhibitor() {
    QDBusInterface login1("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", QDBusConnection::systemBus());
    if (login1.isValid()) {
        QDBusReply<QDBusUnixFileDescriptor> reply = login1.call("Inhibit", "handle-lid-switch", "rog-power-manager", "Custom lid action handling", "block");
        if (reply.isValid()) {
            m_inhibitorFd = reply.value();
            std::cout << "[LidManager] Successfully inhibited systemd-logind handle-lid-switch (fd: " << m_inhibitorFd.fileDescriptor() << ")" << std::endl;
        } else {
            std::cerr << "[LidManager] Warning: Failed to inhibit systemd-logind handle-lid-switch: " << reply.error().message().toStdString() << std::endl;
        }
    }
}

void LidManager::releaseLogindInhibitor() {
    // When m_inhibitorFd goes out of scope or is reassigned, Qt automatically closes the descriptor
}

void LidManager::onUPowerPropertiesChanged(const QString &iface, const QVariantMap &changedProps, const QStringList &invalidated) {
    Q_UNUSED(invalidated);
    if (iface == "org.freedesktop.UPower" && changedProps.contains("LidIsClosed")) {
        bool closed = changedProps.value("LidIsClosed").toBool();
        handleLidChange(closed);
    }
}

void LidManager::pollLid() {
    bool closed = Hardware::isLidClosed();
    if (closed != m_lastLidClosed) {
        handleLidChange(closed);
    }
}

void LidManager::checkLidState() {
    pollLid();
}

void LidManager::handleLidChange(bool closed) {
    if (closed == m_lastLidClosed) return;
    m_lastLidClosed = closed;

    auto lidCfg = ConfigManager::instance().getData().value("lid").toObject();
    QString state = Hardware::getPowerSource();

    if (closed) {
        std::cout << "[LidManager] Laptop Lid CLOSED (Power source: " << state.toStdString() << ")" << std::endl;

        bool clamshell = lidCfg.value("inhibit_when_external_monitor").toBool(true);
        if (clamshell && Hardware::hasExternalMonitor()) {
            std::cout << "[LidManager] External monitor connected — keeping laptop awake (Clamshell Mode)." << std::endl;
            Hardware::sendNotification(I18n::tr("lid_notify_title"), I18n::tr("lid_notify_clamshell"), "video-display");
            ConfigManager::instance().runHook("on_lid_close", {"clamshell", state});
            return;
        }

        QString actionKey = "action_" + state;
        QString action = lidCfg.value(actionKey).toString(state == "battery" ? "suspend" : "turn_off_screen");
        std::cout << "[LidManager] Executing lid close action: " << action.toStdString() << std::endl;

        if (action == "suspend") {
            QProcess::startDetached("loginctl", {"suspend"});
        } else if (action == "hibernate") {
            QProcess::startDetached("loginctl", {"hibernate"});
        } else if (action == "turn_off_all") {
            m_screenTurnedOffByLid = true;
            m_kbdTurnedOffByLid = true;
            m_savedKbdBrightness = Hardware::getKbdBrightness();
            Hardware::setKbdBrightness(0);
            QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
        } else if (action == "turn_off_screen") {
            m_screenTurnedOffByLid = true;
            QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
        } else if (action == "lock_and_turn_off_all") {
            m_screenTurnedOffByLid = true;
            m_kbdTurnedOffByLid = true;
            m_savedKbdBrightness = Hardware::getKbdBrightness();
            Hardware::setKbdBrightness(0);
            QProcess::startDetached("loginctl", {"lock-session"});
            QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
        } else if (action == "lock_and_turn_off") {
            m_screenTurnedOffByLid = true;
            QProcess::startDetached("loginctl", {"lock-session"});
            QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
        } else if (action == "lock") {
            QProcess::startDetached("loginctl", {"lock-session"});
        } else if (action == "nothing") {
            // Do nothing
        }

        QString cmdClose = lidCfg.value("command_on_close").toString();
        if (!cmdClose.trimmed().isEmpty()) {
            ConfigManager::instance().runCommand(cmdClose, {{"ROG_LID_ACTION", action}, {"ROG_POWER_STATE", state}});
        }
        ConfigManager::instance().runHook("on_lid_close", {action, state});
    } else {
        std::cout << "[LidManager] Laptop Lid OPENED (Power source: " << state.toStdString() << ")" << std::endl;

        if (m_screenTurnedOffByLid) {
            m_screenTurnedOffByLid = false;
            QProcess::startDetached("kscreen-doctor", {"--dpms", "on"});
        }
        if (m_kbdTurnedOffByLid) {
            m_kbdTurnedOffByLid = false;
            int restoreVal = (m_savedKbdBrightness > 0) ? m_savedKbdBrightness : 1;
            Hardware::setKbdBrightness(restoreVal);
        }

        QString cmdOpen = lidCfg.value("command_on_open").toString();
        if (!cmdOpen.trimmed().isEmpty()) {
            ConfigManager::instance().runCommand(cmdOpen, {{"ROG_POWER_STATE", state}});
        }
        ConfigManager::instance().runHook("on_lid_open", {state});
    }
}
