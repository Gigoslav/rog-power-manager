#include "DBusServices.hpp"
#include "Hardware.hpp"

#include <QProcess>
#include <iostream>

// PowerManagementAdaptor
PowerManagementAdaptor::PowerManagementAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

bool PowerManagementAdaptor::CanSuspend() { return true; }
bool PowerManagementAdaptor::CanHibernate() { return true; }
bool PowerManagementAdaptor::CanHybridSuspend() { return false; }
bool PowerManagementAdaptor::CanSuspendThenHibernate() { return false; }

void PowerManagementAdaptor::Suspend() {
    QProcess::startDetached("loginctl", {"suspend"});
}

void PowerManagementAdaptor::Hibernate() {
    QProcess::startDetached("loginctl", {"hibernate"});
}

bool PowerManagementAdaptor::GetPowerSaveStatus() {
    return Hardware::getPowerSource() == "battery";
}

// InhibitAdaptor
InhibitAdaptor::InhibitAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

uint InhibitAdaptor::addInhibit(const QString &app, const QString &reason) {
    uint cookie = s_cookieCounter++;
    s_inhibitions[cookie] = qMakePair(app, reason);
    std::cout << "[PowerManagement] Inhibit from '" << app.toStdString() << "': " << reason.toStdString()
              << " (Cookie " << cookie << ")" << std::endl;
    return cookie;
}

void InhibitAdaptor::removeInhibit(uint cookie) {
    if (s_inhibitions.contains(cookie)) {
        auto p = s_inhibitions.take(cookie);
        std::cout << "[PowerManagement] Released Inhibit for '" << p.first.toStdString() << "' (Cookie " << cookie << ")" << std::endl;
    }
}

bool InhibitAdaptor::hasInhibit() {
    return !s_inhibitions.isEmpty() || s_manualInhibit;
}

void InhibitAdaptor::setManualInhibit(bool val) {
    s_manualInhibit = val;
}

bool InhibitAdaptor::manualInhibit() {
    return s_manualInhibit;
}

uint InhibitAdaptor::Inhibit(const QString &application, const QString &reason) {
    return addInhibit(application, reason);
}

void InhibitAdaptor::UnInhibit(uint cookie) {
    removeInhibit(cookie);
}

bool InhibitAdaptor::HasInhibit() {
    return hasInhibit();
}

// BrightnessControlAdaptor
BrightnessControlAdaptor::BrightnessControlAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

int BrightnessControlAdaptor::brightness() {
    return Hardware::getRawBrightness();
}

int BrightnessControlAdaptor::brightnessMax() {
    return Hardware::getMaxBrightness();
}

int BrightnessControlAdaptor::brightnessMin() {
    return 0;
}

int BrightnessControlAdaptor::brightnessSteps() {
    return 20;
}

void BrightnessControlAdaptor::setBrightness(int val) {
    Hardware::setRawBrightness(val);
    int mx = Hardware::getMaxBrightness();
    int pct = (mx > 0) ? static_cast<int>(std::round((val / static_cast<double>(mx)) * 100.0)) : 50;
    Hardware::showKdeOsdBrightness(pct);
    emit brightnessChanged(val);
}

void BrightnessControlAdaptor::setBrightnessSilent(int val) {
    Hardware::setRawBrightness(val);
    emit brightnessChanged(val);
}

// KeyboardBrightnessControlAdaptor
KeyboardBrightnessControlAdaptor::KeyboardBrightnessControlAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

int KeyboardBrightnessControlAdaptor::keyboardBrightness() {
    return Hardware::getKbdBrightness();
}

int KeyboardBrightnessControlAdaptor::keyboardBrightnessMax() {
    return 3;
}

int KeyboardBrightnessControlAdaptor::keyboardBrightnessSteps() {
    return 3;
}

void KeyboardBrightnessControlAdaptor::setKeyboardBrightness(int val) {
    Hardware::setKbdBrightness(val);
    Hardware::showKdeOsdKbd(val);
    emit keyboardBrightnessChanged(val);
}

void KeyboardBrightnessControlAdaptor::setKeyboardBrightnessSilent(int val) {
    Hardware::setKbdBrightness(val);
    emit keyboardBrightnessChanged(val);
}

// PolicyAgentAdaptor
PolicyAgentAdaptor::PolicyAgentAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

bool PolicyAgentAdaptor::HasInhibition(uint) {
    return InhibitAdaptor::hasInhibit();
}

uint PolicyAgentAdaptor::AddInhibition(uint, const QString &appName, const QString &reason) {
    return InhibitAdaptor::addInhibit(appName, reason);
}

void PolicyAgentAdaptor::ReleaseInhibition(uint cookie) {
    InhibitAdaptor::removeInhibit(cookie);
}

// DBusServiceManager
DBusServiceManager& DBusServiceManager::instance() {
    static DBusServiceManager inst;
    return inst;
}

DBusServiceManager::DBusServiceManager() {}

bool DBusServiceManager::registerServices() {
    auto session = QDBusConnection::sessionBus();

    m_pmObject = new QObject(this);
    new PowerManagementAdaptor(m_pmObject);
    new InhibitAdaptor(m_pmObject);

    m_inhibitObject = new QObject(this);
    new InhibitAdaptor(m_inhibitObject);

    if (session.registerService("org.freedesktop.PowerManagement")) {
        session.registerObject("/org/freedesktop/PowerManagement", m_pmObject);
        session.registerObject("/org/freedesktop/PowerManagement/Inhibit", m_inhibitObject);
        std::cout << "[D-Bus] Registered org.freedesktop.PowerManagement & Inhibit" << std::endl;
    }

    m_kdeActionsObject = new QObject(this);
    m_brightnessAdaptor = new BrightnessControlAdaptor(m_kdeActionsObject);
    m_kbdAdaptor = new KeyboardBrightnessControlAdaptor(m_kdeActionsObject);

    m_policyAgentObject = new QObject(this);
    new PolicyAgentAdaptor(m_policyAgentObject);

    if (session.registerService("org.kde.Solid.PowerManagement")) {
        session.registerObject("/org/kde/Solid/PowerManagement/Actions/BrightnessControl", m_kdeActionsObject);
        session.registerObject("/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl", m_kdeActionsObject);
        session.registerObject("/org/kde/Solid/PowerManagement/PolicyAgent", m_policyAgentObject);
        std::cout << "[D-Bus] Registered org.kde.Solid.PowerManagement Actions & PolicyAgent" << std::endl;
    }

    session.connect("", "", "org.kde.kglobalaccel.Component", "globalShortcutPressed",
                    this, SLOT(onGlobalShortcut(QString,QString,qlonglong)));
    std::cout << "[D-Bus] Listening for KDE global shortcuts (Fn Brightness/Kbd/Profile)" << std::endl;

    QDBusConnection::systemBus().connect("", "", "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                         this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    return true;
}

void DBusServiceManager::notifyBrightness(int val) {
    if (m_brightnessAdaptor) {
        emit m_brightnessAdaptor->brightnessChanged(val);
    }
}

void DBusServiceManager::notifyKeyboardBrightness(int val) {
    if (m_kbdAdaptor) {
        emit m_kbdAdaptor->keyboardBrightnessChanged(val);
    }
}

void DBusServiceManager::onGlobalShortcut(const QString &component, const QString &shortcut, qlonglong) {
    if (component != "org_kde_powerdevil") return;

    if (shortcut == "Increase Screen Brightness") {
        int target = Hardware::stepScreenBrightness(5);
        int mx = Hardware::getMaxBrightness();
        notifyBrightness(static_cast<int>(std::round((target / 100.0) * mx)));
    } else if (shortcut == "Decrease Screen Brightness") {
        int target = Hardware::stepScreenBrightness(-5);
        int mx = Hardware::getMaxBrightness();
        notifyBrightness(static_cast<int>(std::round((target / 100.0) * mx)));
    } else if (shortcut == "Increase Screen Brightness Small") {
        int target = Hardware::stepScreenBrightness(1);
        int mx = Hardware::getMaxBrightness();
        notifyBrightness(static_cast<int>(std::round((target / 100.0) * mx)));
    } else if (shortcut == "Decrease Screen Brightness Small") {
        int target = Hardware::stepScreenBrightness(-1);
        int mx = Hardware::getMaxBrightness();
        notifyBrightness(static_cast<int>(std::round((target / 100.0) * mx)));
    } else if (shortcut == "Increase Keyboard Brightness") {
        int lvl = Hardware::stepKbdBrightness(1);
        notifyKeyboardBrightness(lvl);
    } else if (shortcut == "Decrease Keyboard Brightness") {
        int lvl = Hardware::stepKbdBrightness(-1);
        notifyKeyboardBrightness(lvl);
    } else if (shortcut == "Toggle Keyboard Backlight") {
        int lvl = Hardware::toggleKbdBrightness();
        notifyKeyboardBrightness(lvl);
    } else if (shortcut == "powerProfile") {
        emit shortcutTriggered("powerProfile");
    } else if (shortcut == "Turn Off Screen") {
        QProcess::startDetached("kscreen-doctor", {"--dpms", "off"});
    } else if (shortcut == "Sleep") {
        QProcess::startDetached("loginctl", {"suspend"});
    } else if (shortcut == "Hibernate") {
        QProcess::startDetached("loginctl", {"hibernate"});
    } else if (shortcut == "PowerOff" || shortcut == "PowerDown") {
        QProcess::startDetached("loginctl", {"poweroff"});
    }
}

void DBusServiceManager::onPropertiesChanged(const QString&, const QVariantMap &changed, const QStringList&) {
    if (changed.contains("OnBattery") || changed.contains("Online")) {
        emit powerSourceChanged();
    }
}
