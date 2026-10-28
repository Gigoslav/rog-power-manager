#pragma once

#include <QObject>
#include <QDBusUnixFileDescriptor>
#include <QTimer>

class LidManager : public QObject {
    Q_OBJECT
public:
    explicit LidManager(QObject *parent = nullptr);
    ~LidManager() override;

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

    bool m_lastLidClosed = false;
    bool m_screenTurnedOffByLid = false;
    QDBusUnixFileDescriptor m_inhibitorFd;
    QTimer *m_pollTimer = nullptr;
};
