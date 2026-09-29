#include "common/project_config.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include "common/app_data.h"

namespace fkw {
ProjectConfig::ProjectConfig(const QString& configFolder)
    : path_(QDir(configFolder.isEmpty() ? userDataFolder() : configFolder)
        .filePath(QStringLiteral("project.json"))) {
    load();
}
bool ProjectConfig::load() {
    QFile input(path_);
    if (!input.exists()) {
        data_ = {{QStringLiteral("project_link"), QJsonArray{}}};
        return save();
    }
    if (!input.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument parsed = QJsonDocument::fromJson(input.readAll());
    if (!parsed.isObject()) { data_ = {}; return false; }
    data_ = parsed.object();
    return true;
}
bool ProjectConfig::save() const {
    QSaveFile output(path_);
    if (!output.open(QIODevice::WriteOnly)) return false;
    const QByteArray json = QJsonDocument(data_).toJson(QJsonDocument::Indented);
    return output.write(json) == json.size() && output.commit();
}
QJsonValue ProjectConfig::get(const QString& key, const QJsonValue& fallback) const {
    const QJsonValue v = data_.value(key);
    return v.isUndefined() ? fallback : v;
}
bool ProjectConfig::set(const QString& key, const QJsonValue& value) {
    data_.insert(key, value);
    return save();
}
bool ProjectConfig::remove(const QString& key) {
    if (!data_.contains(key)) return true;
    data_.remove(key);
    return save();
}
QStringList ProjectConfig::getProjectOrder() const {
    const QJsonValue value = get(QStringLiteral("project_order"));
    if (!value.isArray()) return {};
    QStringList paths;
    for (const QJsonValue& item : value.toArray())
        if (item.isString()) paths << item.toString();
    return paths;
}
bool ProjectConfig::setProjectOrder(const QStringList& order) {
    return set(QStringLiteral("project_order"), QJsonArray::fromStringList(order));
}
}  // namespace fkw
