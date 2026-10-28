#include "IdleManager.hpp"
#include "Config.hpp"
#include "Hardware.hpp"
#include "DBusServices.hpp"

#include <QProcess>
#include <iostream>

IdleManager::IdleManager(QObject *parent)
    : QObject(parent) {
    initKIdleTime();
}

void IdleManager::initKIdleTime() {
    m_lib.setFileName("libKF6IdleTime.so.6");
    if (!m_lib.load()) {
        std::cout << "[IdleManager] libKF6IdleTime.so.6 not available: " << m_lib.errorString().toStdString() << std::endl;
        return;
    }

    auto instFn = reinterpret_cast<InstanceFn>(m_lib.resolve("_ZN9KIdleTime8instanceEv"));
    m_addTimeout = reinterpret_cast<AddTimeoutFn>(m_lib.resolve("_ZN9KIdleTime14addIdleTimeoutEi"));
    m_removeAll = reinterpret_cast<RemoveAllFn>(m_lib.resolve("_ZN9KIdleTime21removeAllIdleTimeoutsEv"));
    m_catchResume = reinterpret_cast<CatchResumeFn>(m_lib.resolve("_ZN9KIdleTime20catchNextResumeEventEv"));

    if (instFn && m_addTimeout && m_removeAll && m_catchResume) {
        m_inst = instFn();
        m_kIdleTimeObj = reinterpret_cast<QObject*>(m_inst);
        if (m_kIdleTimeObj) {
            connect(m_kIdleTimeObj, SIGNAL(timeoutReached(int,int)), this, SLOT(onTimeoutReached(int,int)));
            connect(m_kIdleTimeObj, SIGNAL(resumingFromIdle()), this, SLOT(onResumingFromIdle()));
            m_available = true;
            std::cout << "[IdleManager] KIdleTime Wayland/X11 idle listener initialized." << std::endl;
        }
    }
}

void IdleManager::updateState(const QString &state) {
    m_currentState = state;
    rearm();
}

void IdleManager::rearm() {
    if (!m_available || !m_inst) return;

    m_removeAll(m_inst);
    m_timeoutActions.clear();

    auto idleCfg = ConfigManager::instance().getData().value("idle").toObject();
    int dimSec = idleCfg.value(QString("dim_%1").arg(m_currentState)).toInt(180);
    int offSec = idleCfg.value(QString("dpms_%1").arg(m_currentState)).toInt(300);
    int susSec = idleCfg.value(QString("suspend_%1").arg(m_currentState)).toInt(600);

    if (dimSec > 0) {
        int tid = m_addTimeout(m_inst, dimSec * 1000);
        m_timeoutActions[tid] = "dim";
    }
    if (offSec > 0) {
        int tid = m_addTimeout(m_inst, offSec * 1000);
        m_timeoutActions[tid] = "off";
    }
    if (susSec > 0) {
        int tid = m_addTimeout(m_inst, susSec * 1000);
        m_timeoutActions[tid] = "suspend";
    }

    if (m_catchResume) {
        m_catchResume(m_inst);
    }
}

void IdleManager::onTimeoutReached(int id, int) {
    if (InhibitAdaptor::hasInhibit()) {
        std::cout << "[IdleManager] Inhibit active, skipping idle action" << std::endl;
        return;
    }

    QString action = m_timeoutActions.value(id, "");
    if (action == "dim" && !m_isDimmed) {
        m_savedBrightness = Hardware::getBrightnessPct();
        std::cout << "[IdleManager] Idle reached: dimming display (saved: " << m_savedBrightness << "%)" << std::endl;
        Hardware::setBrightnessPct(25);
        m_isDimmed = true;
    } else if (action == "off" && !m_isDpmsOff) {
        std::cout << "[IdleManager] Idle reached: DPMS screen off" << std::endl;
        QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
        m_isDpmsOff = true;
    } else if (action == "suspend") {
        std::cout << "[IdleManager] Idle reached: auto-suspend" << std::endl;
        QProcess::startDetached("loginctl", {"suspend"});
    }

    if (m_catchResume) {
        m_catchResume(m_inst);
    }
}

void IdleManager::onResumingFromIdle() {
    if (m_isDpmsOff) {
        std::cout << "[IdleManager] Resumed from idle: waking display" << std::endl;
        QProcess::startDetached("kscreen-doctor", {"--dpms", "on"});
        m_isDpmsOff = false;
    }
    if (m_isDimmed) {
        if (m_savedBrightness > 0) {
            std::cout << "[IdleManager] Resumed from idle: restoring brightness to " << m_savedBrightness << "%" << std::endl;
            Hardware::setBrightnessPct(m_savedBrightness);
        }
        m_isDimmed = false;
    }

    if (m_catchResume) {
        m_catchResume(m_inst);
    }
}
