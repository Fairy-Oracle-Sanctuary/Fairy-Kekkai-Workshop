#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include "common/config_keys.h"

namespace fkw {

// Shared access to config.json in the user data directory.
class AppConfig : public QObject {
    Q_OBJECT
public:
    static AppConfig& instance();
    QJsonValue value(const QString& group, const QString& key,
                     const QJsonValue& fallback = {}) const;
    QJsonValue value(ConfigKeys::Key key, const QJsonValue& fallback = {}) const {
        return value(QLatin1String(key.group), QLatin1String(key.name), fallback);
    }
    QJsonObject snapshot() const;
    bool set(const QString& group, const QString& key, const QJsonValue& value,
             bool restart = false);
    bool set(ConfigKeys::Key key, const QJsonValue& value, bool restart = false) {
        return set(QLatin1String(key.group), QLatin1String(key.name), value, restart);
    }
signals:
    void valueChanged(const QString& group, const QString& key, const QJsonValue& value);
    void restartRequired();
private:
    AppConfig() = default;
};
}  // namespace fkw
