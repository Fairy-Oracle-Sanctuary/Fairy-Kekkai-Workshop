#pragma once
#include <QString>
#include <QtGlobal>
namespace fkw {
bool openUrl(const QString& url);
bool showInFolder(const QString& path);
// os.path.dirname/basename 语义（Windows 下 / 与 \ 均为分隔符）
QString pathDirname(const QString& path);
QString pathBasename(const QString& path);
// 最后一个路径分隔符的索引，无分隔符时返回 -1（供其它模块复用）
qsizetype lastSeparator(const QString& path);
}
