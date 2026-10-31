#pragma once

#include <QString>
#include <QJsonObject>
#include <QMap>

class ConfigManager {
public:
    static ConfigManager& instance();

    void load();
    void save();

    QJsonObject getData() const { return m_data; }
    void setData(const QJsonObject &data);

    int getChargeLimit() const;
    void setChargeLimit(int limit);

    int getRefreshInterval() const;
    void setRefreshInterval(int intervalMs);

    void runCommand(const QString &cmd, const QMap<QString, QString> &customEnv = {});
    void runHook(const QString &hookName, const QStringList &args = {});

    QString configPath() const;
    QString hooksDir() const;

private:
    ConfigManager();
    QJsonObject getDefaultConfig();

    QJsonObject m_data;
};
