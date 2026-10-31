#pragma once

#include "Controller.hpp"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QActionGroup>

class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayIcon(PowerManagerController *controller, QObject *parent = nullptr);

public slots:
    void updateTray();

private:
    void createMenu();
    QIcon createWattIcon(const Hardware::PowerTelemetry &tel);

    PowerManagerController *m_controller;
    QMenu *m_menu;
    QTimer *m_timer;

    // Menu Actions
    QAction *m_actHeader;
    QAction *m_actTotalIn;
    QAction *m_actBat;
    QAction *m_actSys;
    QAction *m_actSysDetail;
    QAction *m_actState;
    QAction *m_actAuto;

    QActionGroup *m_profileGroup;
    QAction *m_actPs;
    QAction *m_actBal;
    QAction *m_actPerf;

    QAction *m_actGpuStatus;
    QAction *m_actBatProt;
    QAction *m_actLimStop;
    QAction *m_actLim60;
    QAction *m_actLim80;
    QAction *m_actLim100;
    QActionGroup *m_batLimitGroup;
    QAction *m_actInhibit;

    QMenu *m_fanMenu;
    QActionGroup *m_fanGroup;
    QMap<QString, QAction*> m_fanActions;

    int m_tickCount = 1;
    bool m_lowBatAlerted = false;
    bool m_critBatAlerted = false;
};
