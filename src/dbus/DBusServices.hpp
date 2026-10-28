#pragma once

#include <QObject>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QMap>

class PowerManagementAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PowerManagement")
public:
    explicit PowerManagementAdaptor(QObject *parent);

public slots:
    bool CanSuspend();
    bool CanHibernate();
    bool CanHybridSuspend();
    bool CanSuspendThenHibernate();
    void Suspend();
    void Hibernate();
    bool GetPowerSaveStatus();
};

class InhibitAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.PowerManagement.Inhibit")
public:
    explicit InhibitAdaptor(QObject *parent);

    static uint addInhibit(const QString &app, const QString &reason);
    static void removeInhibit(uint cookie);
    static bool hasInhibit();
    static void setManualInhibit(bool val);
    static bool manualInhibit();

public slots:
    uint Inhibit(const QString &application, const QString &reason);
    void UnInhibit(uint cookie);
    bool HasInhibit();

private:
    static inline QMap<uint, QPair<QString, QString>> s_inhibitions;
    static inline uint s_cookieCounter = 1000;
    static inline bool s_manualInhibit = false;
};

class BrightnessControlAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.BrightnessControl")
public:
    explicit BrightnessControlAdaptor(QObject *parent);

public slots:
    int brightness();
    int brightnessMax();
    int brightnessMin();
    int brightnessSteps();
    void setBrightness(int val);
    void setBrightnessSilent(int val);

signals:
    void brightnessChanged(int val);
};

class KeyboardBrightnessControlAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl")
public:
    explicit KeyboardBrightnessControlAdaptor(QObject *parent);

public slots:
    int keyboardBrightness();
    int keyboardBrightnessMax();
    int keyboardBrightnessSteps();
    void setKeyboardBrightness(int val);
    void setKeyboardBrightnessSilent(int val);

signals:
    void keyboardBrightnessChanged(int val);
};

class PolicyAgentAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Solid.PowerManagement.PolicyAgent")
public:
    explicit PolicyAgentAdaptor(QObject *parent);

public slots:
    bool HasInhibition(uint types);
    uint AddInhibition(uint types, const QString &appName, const QString &reason);
    void ReleaseInhibition(uint cookie);
};

class DBusServiceManager : public QObject {
    Q_OBJECT
public:
    static DBusServiceManager& instance();

    bool registerServices();
    void notifyBrightness(int val);
    void notifyKeyboardBrightness(int val);

signals:
    void shortcutTriggered(const QString &shortcut);
    void powerSourceChanged();

public slots:
    void onGlobalShortcut(const QString &component, const QString &shortcut, qlonglong timestamp);
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    DBusServiceManager();

    QObject *m_pmObject = nullptr;
    QObject *m_inhibitObject = nullptr;
    QObject *m_kdeActionsObject = nullptr;
    QObject *m_policyAgentObject = nullptr;

    BrightnessControlAdaptor *m_brightnessAdaptor = nullptr;
    KeyboardBrightnessControlAdaptor *m_kbdAdaptor = nullptr;
};
