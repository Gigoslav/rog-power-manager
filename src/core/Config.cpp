#include "Config.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <iostream>

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager() {
    load();
}

QString ConfigManager::configPath() const {
    QString configDir = QDir::homePath() + "/.config/rog-power-manager";
    return configDir + "/config.json";
}

QString ConfigManager::hooksDir() const {
    return QDir::homePath() + "/.config/rog-power-manager/hooks.d";
}

QJsonObject ConfigManager::getDefaultConfig() {
    QJsonObject bat;
    bat["profile"] = "power-saver";
    bat["fan_mode"] = "auto";
    bat["hz"] = 60;
    bat["brightness"] = 50;
    bat["kbd"] = 1;
    bat["command"] = "";

    QJsonObject usbc;
    usbc["profile"] = "balanced";
    usbc["fan_mode"] = "auto";
    usbc["hz"] = 60;
    usbc["brightness"] = 75;
    usbc["kbd"] = 1;
    usbc["command"] = "";

    QJsonObject ac;
    ac["profile"] = "performance";
    ac["fan_mode"] = "auto";
    ac["hz"] = 240;
    ac["brightness"] = 100;
    ac["kbd"] = 3;
    ac["command"] = "";

    QJsonObject profiles;
    profiles["battery"] = bat;
    profiles["usbc"] = usbc;
    profiles["ac"] = ac;

    QJsonObject idle;
    idle["dim_battery"] = 180;
    idle["dpms_battery"] = 300;
    idle["suspend_battery"] = 600;

    idle["dim_usbc"] = 300;
    idle["dpms_usbc"] = 600;
    idle["suspend_usbc"] = 1200;

    idle["dim_ac"] = 600;
    idle["dpms_ac"] = 900;
    idle["suspend_ac"] = 0;

    QJsonObject alerts;
    alerts["low_battery_percent"] = 10;
    alerts["critical_battery_percent"] = 5;
    alerts["on_low_command"] = "";
    alerts["on_critical_command"] = "";

    QJsonObject lid;
    lid["action_battery"] = "suspend";
    lid["action_usbc"] = "turn_off_screen";
    lid["action_ac"] = "turn_off_screen";
    lid["inhibit_when_external_monitor"] = true;
    lid["command_on_close"] = "";
    lid["command_on_open"] = "";

    QJsonObject battery;
    battery["charge_limit"] = 80;

    QJsonObject general;
    general["refresh_interval_ms"] = 1000;

    QJsonObject root;
    root["general"] = general;
    root["profiles"] = profiles;
    root["battery"] = battery;
    root["idle"] = idle;
    root["alerts"] = alerts;
    root["lid"] = lid;

    return root;
}

void ConfigManager::load() {
    QString cPath = configPath();
    QFile file(cPath);
    if (!file.exists()) {
        m_data = getDefaultConfig();
        save();
        return;
    }

    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            m_data = doc.object();
            bool modified = false;
            if (!m_data.contains("general")) {
                m_data["general"] = getDefaultConfig().value("general").toObject();
                modified = true;
            } else {
                auto gen = m_data.value("general").toObject();
                if (!gen.contains("refresh_interval_ms")) {
                    gen["refresh_interval_ms"] = 1000;
                    m_data["general"] = gen;
                    modified = true;
                }
            }
            if (!m_data.contains("lid")) {
                m_data["lid"] = getDefaultConfig().value("lid").toObject();
                modified = true;
            }
            if (!m_data.contains("battery")) {
                m_data["battery"] = getDefaultConfig().value("battery").toObject();
                modified = true;
            } else {
                auto bat = m_data.value("battery").toObject();
                if (!bat.contains("charge_limit")) {
                    bat["charge_limit"] = 80;
                    m_data["battery"] = bat;
                    modified = true;
                }
            }
            file.close();
            if (modified) {
                save();
            }
            return;
        }
        file.close();
    }
    m_data = getDefaultConfig();
}

void ConfigManager::save() {
    QString cDir = QDir::homePath() + "/.config/rog-power-manager";
    QDir().mkpath(cDir);
    QDir().mkpath(hooksDir());

    QFile file(configPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(m_data);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void ConfigManager::setData(const QJsonObject &data) {
    m_data = data;
    save();
}

int ConfigManager::getChargeLimit() const {
    return m_data.value("battery").toObject().value("charge_limit").toInt(80);
}

void ConfigManager::setChargeLimit(int limit) {
    int clamped = std::clamp(limit, 20, 100);
    auto bat = m_data.value("battery").toObject();
    bat["charge_limit"] = clamped;
    m_data["battery"] = bat;
    save();
}

int ConfigManager::getRefreshInterval() const {
    return m_data.value("general").toObject().value("refresh_interval_ms").toInt(1000);
}

void ConfigManager::setRefreshInterval(int intervalMs) {
    int clamped = std::clamp(intervalMs, 250, 5000);
    auto gen = m_data.value("general").toObject();
    gen["refresh_interval_ms"] = clamped;
    m_data["general"] = gen;
    save();
}

void ConfigManager::runCommand(const QString &cmd, const QMap<QString, QString> &customEnv) {
    if (cmd.trimmed().isEmpty()) return;

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = customEnv.constBegin(); it != customEnv.constEnd(); ++it) {
        env.insert(it.key(), it.value());
    }
    process.setProcessEnvironment(env);
    QProcess::startDetached("/bin/sh", {"-c", cmd});
}

void ConfigManager::runHook(const QString &hookName, const QStringList &args) {
    QString hPath = hooksDir() + "/" + hookName;
    QFileInfo fi(hPath);
    if (fi.exists() && fi.isExecutable()) {
        QProcess::startDetached(hPath, args);
    }
}
