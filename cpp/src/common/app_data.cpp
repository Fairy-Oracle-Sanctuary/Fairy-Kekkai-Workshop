#include "common/app_data.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace fkw {
namespace {
QByteArray mergeMissingConfig(const QByteArray& incoming, const QByteArray& existing) {
    const QJsonDocument oldDoc = QJsonDocument::fromJson(existing);
    const QJsonDocument newDoc = QJsonDocument::fromJson(incoming);
    if (!oldDoc.isObject() || !newDoc.isObject()) return incoming;
    QJsonObject merged = newDoc.object();
    const QJsonObject previous = oldDoc.object();
    for (auto group = previous.constBegin(); group != previous.constEnd(); ++group) {
        if (!group.value().isObject()) {
            if (!merged.contains(group.key())) merged.insert(group.key(), group.value());
            continue;
        }
        QJsonObject section = merged.value(group.key()).toObject();
        const QJsonObject oldSection = group.value().toObject();
        for (auto key = oldSection.constBegin(); key != oldSection.constEnd(); ++key) {
            if (!section.contains(key.key())) section.insert(key.key(), key.value());
        }
        merged.insert(group.key(), section);
    }
    return QJsonDocument(merged).toJson(QJsonDocument::Indented);
}
bool moveFile(const QString& source, const QString& destination) {
    if (QFile::rename(source, destination)) return true;
    if (!QFile::copy(source, destination)) return false;
    if (QFile::remove(source)) return true;
    QFile::remove(destination);
    return false;
}
bool replaceFile(const QString& source, const QString& destination, bool mergeConfig) {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) return false;
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly)) return false;
    if (mergeConfig) {
        QFile current(destination);
        const QByteArray existing = current.open(QIODevice::ReadOnly) ? current.readAll() : QByteArray();
        current.close();
        const QByteArray incoming = input.readAll();
        if (input.error() != QFileDevice::NoError) return false;
        const QByteArray bytes = mergeMissingConfig(incoming, existing);
        if (output.write(bytes) != bytes.size()) return false;
    } else {
        while (!input.atEnd()) {
            const QByteArray bytes = input.read(65536);
            if (bytes.isEmpty() || output.write(bytes) != bytes.size()) return false;
        }
        if (input.error() != QFileDevice::NoError) return false;
    }
    input.close();
    if (!output.commit()) return false;
    return QFile::remove(source);
}
void moveLegacyData(const QDir& old, const QString& target) {
    if (!old.exists() || QDir::cleanPath(old.absolutePath()) == QDir::cleanPath(target)) return;
    QStringList directories;
    QDirIterator entries(old.absolutePath(),
                         QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        const QFileInfo entry(entries.next());
        const QString relative = old.relativeFilePath(entry.absoluteFilePath());
        const QString destination = QDir(target).filePath(relative);
        if (entry.isDir()) {
            QDir().mkpath(destination);
            directories.append(entry.absoluteFilePath());
            continue;
        }
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())) continue;
        if (QFileInfo::exists(destination))
            replaceFile(entry.absoluteFilePath(), destination,
                        relative == QStringLiteral("config.json"));
        else
            moveFile(entry.absoluteFilePath(), destination);
    }
    std::sort(directories.begin(), directories.end(),
              [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& directory : directories) QDir().rmdir(directory);
    QDir().rmdir(old.absolutePath());
}
}
QString sourceRoot() {
    const auto nextToExe = QCoreApplication::applicationDirPath();
    if (QFileInfo(QDir(nextToExe).filePath(QStringLiteral("tools"))).isDir() ||
        QFileInfo::exists(QDir(nextToExe).filePath(QStringLiteral("PADDLEOCR"))))
        return nextToExe;
    return QStringLiteral(FKW_SOURCE_ROOT);
}

QStringList softwareRoots() {
    QStringList result;
    const auto add = [&result](const QString& path) {
        if (path.isEmpty()) return;
        const QString clean = QDir::cleanPath(QDir(path).absolutePath());
        // 盘符根目录会让「是否在软件目录内」对整块盘都成立，必须排除
        if (clean.isEmpty() || QDir(clean).isRoot()) return;
        for (const QString& existing : result)
            if (existing.compare(clean, Qt::CaseInsensitive) == 0) return;
        result << clean;
    };
    add(sourceRoot());
    add(QCoreApplication::applicationDirPath());
    return result;
}

bool insideSoftwareFolder(const QString& path) {
    if (path.isEmpty()) return false;
    // Windows 路径大小写不敏感，统一转小写后再比较
    const QString target = QDir::cleanPath(QDir(path).absolutePath()).toLower();
    // 软件正好装在盘符根目录时 cleanPath 会留下结尾斜杠，这里补一次避免拼出双斜杠
    const auto withSeparator = [](const QString& value) {
        return value.endsWith(QLatin1Char('/')) ? value : value + QLatin1Char('/');
    };
    for (const QString& root : softwareRoots()) {
        const QString software = root.toLower();
        if (target == software || target.startsWith(withSeparator(software))
            || software.startsWith(withSeparator(target)))
            return true;
    }
    return false;
}

QString defaultProjectFolder() {
    // 项目放在软件目录里风险太大：卸载、重装、清理软件目录都会连累项目数据。
    // 所以优先落在与软件同盘的「平行目录」，例如软件装在 D:\Fairy-Kekkai-Workshop 时
    // 项目就落在 D:\Fairy-Kekkai-Workshop-Projects —— 同盘但完全独立。
    QStringList candidates;
    const QStringList drives{QStringLiteral("D:/"), QStringLiteral("C:/")};
    for (const QString& drive : drives) {
        if (!QFileInfo(drive).isDir()) continue;
        candidates << QDir(drive).filePath(QStringLiteral("Fairy-Kekkai-Workshop-Projects"));
    }
    // 数据盘都不可写时退回文档目录，最后才是用户数据目录
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty())
        candidates << QDir(documents).filePath(QStringLiteral("Fairy-Kekkai-Workshop/Projects"));
    candidates << QDir(userDataFolder()).filePath(QStringLiteral("Projects"));
    for (const QString& candidate : candidates) {
        // 相对路径会把目录建到当前工作目录，同样按不可用处理
        if (candidate.isEmpty() || !QDir::isAbsolutePath(candidate)) continue;
        if (insideSoftwareFolder(candidate)) continue;
        // mkpath 同时验证了目录可创建，只读盘、无权写入的目录会在这里被跳过
        if (QDir().mkpath(candidate)) return QDir(candidate).absolutePath();
    }
    return userDataFolder();
}

QString userDataFolder() {
    static const QString path = [] {
#ifdef Q_OS_WIN
        const QString base = qEnvironmentVariable("LOCALAPPDATA",
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
#else
        const QString base = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation);
#endif
        const QString target = QDir(base).filePath(
            QStringLiteral("Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop"));
        if (QDir().mkpath(target)) {
#ifdef Q_OS_WIN
            moveLegacyData(QDir(QDir(sourceRoot()).filePath(QStringLiteral("AppData"))),
                           target);
#endif
        }
        return target;
    }();
    return path;
}

QJsonObject readJson(const QString& relativePath) {
    QFile file(QDir(userDataFolder()).filePath(relativePath));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString configString(const QJsonObject& config, const QString& section,
                     const QString& key, const QString& fallback) {
    const auto value = config.value(section).toObject().value(key);
    return value.isString() ? value.toString() : fallback;
}

bool configBool(const QJsonObject& config, const QString& section,
                const QString& key, bool fallback) {
    const auto value = config.value(section).toObject().value(key);
    return value.isBool() ? value.toBool() : fallback;
}

QString trText(const char* source) { return QCoreApplication::translate("Text", source); }
}
