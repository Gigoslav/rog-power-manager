#pragma once

#include "Hardware.hpp"
#include "IdleManager.hpp"
#include <QObject>

class LidManager;

class PowerManagerController : public QObject {
    Q_OBJECT
public:
    explicit PowerManagerController(bool verbose = true, QObject *parent = nullptr);

    void applyState(bool force = false);
    void cyclePowerProfile();
    void manualSet(const QString &prof, int hz);

    bool autoMode() const { return m_autoMode; }
    void setAutoMode(bool val) { m_autoMode = val; }

    QString lastState() const { return m_lastState; }
    QString cachedProfile() const { return m_cachedProfile; }
    Hardware::PanelInfo cachedPanel() const { return m_cachedPanel; }

    void setIdleManager(IdleManager *mgr) { m_idleMgr = mgr; }
    void setLidManager(LidManager *mgr) { m_lidMgr = mgr; }
    LidManager* lidManager() const { return m_lidMgr; }

    void notifyRefreshIntervalChanged(int ms) { emit refreshIntervalChanged(ms); }

signals:
    void stateApplied();
    void refreshIntervalChanged(int ms);

private:
    bool m_verbose = true;
    bool m_autoMode = true;
    QString m_lastState;
    QString m_cachedProfile = "balanced";
    Hardware::PanelInfo m_cachedPanel;
    IdleManager *m_idleMgr = nullptr;
    LidManager *m_lidMgr = nullptr;
};
