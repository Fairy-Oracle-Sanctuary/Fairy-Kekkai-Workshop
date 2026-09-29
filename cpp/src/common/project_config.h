#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace fkw {
class ProjectConfig {
public:
    explicit ProjectConfig(const QString& configFolder = {});
    bool load();
    bool save() const;
    QJsonValue get(const QString& key, const QJsonValue& fallback = {}) const;
    bool set(const QString& key, const QJsonValue& value);
    bool remove(const QString& key);
    QStringList getProjectOrder() const;
    bool setProjectOrder(const QStringList& order);
    QJsonObject getAll() const { return data_; }
private:
    QString path_;
    QJsonObject data_;
};
}  // namespace fkw
