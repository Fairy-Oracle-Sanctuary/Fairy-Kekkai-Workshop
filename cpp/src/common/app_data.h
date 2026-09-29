#pragma once

#include <QJsonObject>
#include <QString>

namespace fkw {
QString sourceRoot();
QString userDataFolder();
// 项目库的默认根目录：优先软件目录的平行目录（D 盘 -> C 盘），
// 其次系统文档目录，都不可用才退回用户数据目录
QString defaultProjectFolder();
// path 是否就是软件目录，或者与软件目录互为父子目录
bool insideSoftwareFolder(const QString& path);
QJsonObject readJson(const QString& relativePath);
QString configString(const QJsonObject& config, const QString& section,
                     const QString& key, const QString& fallback = {});
bool configBool(const QJsonObject& config, const QString& section,
                const QString& key, bool fallback = false);
QString trText(const char* source);
}
