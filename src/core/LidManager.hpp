#pragma once

#include <QObject>
#include <QDBusUnixFileDescriptor>
#include <QTimer>

class PowerManagerController;

class LidManager : public QObject {
    Q_OBJECT
public:
    explicit LidManager(PowerManagerController *controller = nullptr, QObject *parent = nullptr);
    ~LidManager() override;

    void setController(PowerManagerController *ctrl) { m_controller = ctrl; }
    void checkLidState();
    bool isLidClosed() const { return m_lastLidClosed; }

public slots:
    void onUPowerPropertiesChanged(const QString &iface, const QVariantMap &changedProps, const QStringList &invalidated);

private slots:
    void pollLid();

private:
    void acquireLogindInhibitor();
    void releaseLogindInhibitor();
    void handleLidChange(bool closed);

    PowerManagerController *m_controller = nullptr;
    bool m_lastLidClosed = false;
    bool m_screenTurnedOffByLid = false;
    bool m_kbdTurnedOffByLid = false;
    bool m_maxPowerSaveActive = false;
    int m_savedKbdBrightness = 1;
    QDBusUnixFileDescriptor m_inhibitorFd;
    QTimer *m_pollTimer = nullptr;
};
