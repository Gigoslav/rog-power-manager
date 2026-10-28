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

    QJsonObject root;
    root["profiles"] = profiles;
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
            if (!m_data.contains("lid")) {
                m_data["lid"] = getDefaultConfig().value("lid").toObject();
                file.close();
                save();
                return;
            }
            file.close();
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
