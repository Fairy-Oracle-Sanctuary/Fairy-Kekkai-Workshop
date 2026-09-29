#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fkw {
QString sourceRoot();
// 判定「软件目录」时需要考虑的全部位置：
// 发行版的软件目录就是 exe 所在目录（旁边带 PADDLEOCR 标识或 tools 目录），
// 开发构建的 exe 落在 build 输出目录里，此时编译期源码根才是老版本软件的目录。
// 两处都可能被用户放进项目，因此都要检查；盘符根目录会被剔除，避免把整个盘算成软件目录。
QStringList softwareRoots();
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
