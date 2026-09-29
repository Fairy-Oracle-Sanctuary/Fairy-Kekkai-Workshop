#include "service/project_service.h"

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include "common/app_data.h"

namespace fkw::projects {
namespace {
QString configPath() { return QDir(userDataFolder()).filePath(QStringLiteral("project.json")); }
QJsonObject config() { return readJson(QStringLiteral("project.json")); }
QStringList stringList(const QJsonValue& value) {
    QStringList result;
    for (const auto& entry : value.toArray()) {
        if (!entry.isString()) continue;
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(entry.toString()));
        if (!result.contains(path)) result << path;
    }
    return result;
}
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}
bool setValue(const QString& key, const QJsonValue& value, QString* error) {
    QJsonObject object = config();
    object.insert(key, value);
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit())
        return fail(error, file.errorString());
    return true;
}
bool setList(const QString& key, const QStringList& values, QString* error) {
    return setValue(key, QJsonArray::fromStringList(values), error);
}
bool validName(const QString& name) {
    return !name.isEmpty() && name != QStringLiteral(".") && name != QStringLiteral("..")
        && !name.endsWith(QLatin1Char('.')) && !name.endsWith(QLatin1Char(' '))
        && !name.contains(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")));
}
bool saveDocument(const QString& path, const Document& doc, QString* error) {
    QSaveFile file(QDir(path).filePath(QStringLiteral("标题.txt")));
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    QStringList lines = doc.indexLines;
    if (lines.size() != doc.episodes.size()) {
        lines.clear();
        for (int i = 1; i <= doc.episodes.size(); ++i) lines << QString::number(i);
    }
    lines << QString();
    for (const Episode& episode : doc.episodes) {
        lines << episode.originalTitle;
        if (doc.translated) lines << episode.translatedTitle;
        lines << episode.videoUrl << QString();
    }
    lines << QString() << QStringLiteral("---");
    const QByteArray bytes = lines.join(QLatin1Char('\n')).toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, file.errorString());
    return true;
}
bool copyTree(const QString& from, const QString& to, QString* error) {
    if (!QDir().mkpath(to)) return fail(error, QStringLiteral("无法创建目标目录"));
    const QDir dir(from);
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, error)) return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            return fail(error, QStringLiteral("无法复制：") + entry.absoluteFilePath());
        }
    }
    return true;
}
QString localPath(const QString& name) { return QDir(root()).filePath(name); }
bool isNumericName(const QString& name, int* number) {
    bool numeric = false;
    const int value = name.toInt(&numeric);
    if (!numeric || value < 1 || QString::number(value) != name) return false;
    if (number) *number = value;
    return true;
}
bool parsesAsDocument(const QString& filePath) {
    Document document;
    return readFile(filePath, &document, nullptr);
}
// 数字目录是否恰好构成连续的 1..n（如 1 2 3）；出现 1 2 4 这类中间缺失即返回 false
bool isContiguousSequence(const QVector<int>& numbers) {
    if (numbers.isEmpty()) return false;
    return numbers.last() == numbers.size();
}
// 项目库根目录的进程内缓存：切换目录后要立刻生效，不必反复读配置
QString& cachedRoot() {
    static QString value;
    return value;
}
bool usableRoot(const QString& path) {
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return false;
    // 落在软件目录里等于没搬，直接判为不可用
    if (insideSoftwareFolder(path)) return false;
    return QDir().mkpath(path);
}
bool emptyFolder(const QString& path) {
    const QDir dir(path);
    return dir.exists()
        && dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}
QString resolveRoot() {
    const QString stored = QDir::cleanPath(
        config().value(QStringLiteral("project_root")).toString());
    if (usableRoot(stored)) return QDir(stored).absolutePath();
    QString detected = defaultProjectFolder();
    if (insideSoftwareFolder(detected)) {
        // 极端情况：软件正好就装在探测出来的位置上，退回用户数据目录
        detected = QDir(userDataFolder()).filePath(QStringLiteral("Projects"));
        QDir().mkpath(detected);
    }
    if (QDir::cleanPath(detected) != stored)
        setValue(QStringLiteral("project_root"), detected, nullptr);
    return detected;
}
}
QString root() {
    QString& cached = cachedRoot();
    if (cached.isEmpty()) cached = resolveRoot();
    return cached;
}
bool checkRoot(const QString& path, QString* error) {
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean.isEmpty() || !QDir(clean).exists())
        return fail(error, QStringLiteral("所选目录不存在"));
    if (insideSoftwareFolder(clean))
        return fail(error, QStringLiteral("项目目录不能是软件目录，也不能包含软件目录"));
    if (!emptyFolder(clean))
        return fail(error, QStringLiteral("只能选择空文件夹作为项目目录"));
    return true;
}
bool rememberRoot(const QString& path, QString* error) {
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean.isEmpty() || !QDir(clean).exists())
        return fail(error, QStringLiteral("所选目录不存在"));
    cachedRoot() = clean;
    return setValue(QStringLiteral("project_root"), clean, error);
}
QStringList links() { return stringList(config().value(QStringLiteral("project_link"))); }
QStringList order() { return stringList(config().value(QStringLiteral("project_order"))); }
bool setLinks(const QStringList& paths, QString* error) {
    return setList(QStringLiteral("project_link"), paths, error);
}
bool setOrder(const QStringList& paths, QString* error) {
    return setList(QStringLiteral("project_order"), paths, error);
}
QVector<int> episodeNumbers(const QString& path) {
    QVector<int> numbers;
    const QDir dir(path);
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        int number = 0;
        if (isNumericName(entry.fileName(), &number)) numbers.append(number);
    }
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}
bool isProject(const QString& path) {
    const QDir dir(path);
    if (!dir.exists() || !QFileInfo::exists(dir.filePath(QStringLiteral("标题.txt"))))
        return false;
    const QVector<int> numbers = episodeNumbers(path);
    for (int i = 1; i <= numbers.size(); ++i)
        if (!QFileInfo(dir.filePath(QString::number(i))).isDir()) return false;
    return true;
}
bool looksLikeProject(const QString& path) {
    const QDir dir(path);
    if (!dir.exists()) return false;
    const QVector<int> numbers = episodeNumbers(path);
    const QString titleFile = dir.filePath(QStringLiteral("标题.txt"));
    if (QFileInfo::exists(titleFile))
        return !numbers.isEmpty() || parsesAsDocument(titleFile);
    if (numbers.isEmpty()) return false;
    // 标题.txt 可能被改名：非空的 .txt 只要能被解析就仍然认定为项目
    for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files)) {
        if (entry.fileName() == QStringLiteral("icon.txt") || entry.size() == 0) continue;
        if (parsesAsDocument(entry.absoluteFilePath())) return true;
    }
    // 标题.txt 被整个删除：只有数字目录恰好是连续的 1..n 才认定为“缺标题.txt”的项目。
    // 出现 1 2 4 这类中间缺失说明这些数字目录不是分集目录，直接判为非项目，避免认错。
    return isContiguousSequence(numbers);
}
QStringList paths() {
    QStringList result;
    const QDir rootFolder(root());
    for (const QFileInfo& folder : rootFolder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (isProject(folder.absoluteFilePath())) result << folder.absoluteFilePath();
    for (const QString& path : links())
        if (isProject(path) && !result.contains(path)) result << path;
    const QStringList savedOrder = order();
    std::stable_sort(result.begin(), result.end(), [&](const QString& a, const QString& b) {
        const int ai = savedOrder.indexOf(a), bi = savedOrder.indexOf(b);
        return (ai < 0 ? savedOrder.size() : ai) < (bi < 0 ? savedOrder.size() : bi);
    });
    return result;
}
QStringList candidates() {
    QStringList result;
    const QDir rootFolder(root());
    for (const QFileInfo& folder : rootFolder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (looksLikeProject(folder.absoluteFilePath())) result << folder.absoluteFilePath();
    for (const QString& path : links())
        if ((looksLikeProject(path) || !QFileInfo::exists(path)) && !result.contains(path))
            result << path;
    const QStringList savedOrder = order();
    std::stable_sort(result.begin(), result.end(), [&](const QString& a, const QString& b) {
        const int ai = savedOrder.indexOf(a), bi = savedOrder.indexOf(b);
        return (ai < 0 ? savedOrder.size() : ai) < (bi < 0 ? savedOrder.size() : bi);
    });
    return result;
}
bool create(const QString& name, int count, const QString& title, QString* error) {
    const QString clean = name.trimmed(), label = title.trimmed();
    if (!validName(clean) || !validName(label) || label == QStringLiteral("标题") ||
        label == QStringLiteral("icon") || count < 1 || count > 127)
        return fail(error, QStringLiteral("项目名称、原标题或集数无效"));
    const QString path = localPath(clean);
    if (QFileInfo::exists(path)) return fail(error, QStringLiteral("项目目录已存在"));
    if (!QDir().mkpath(path)) return fail(error, QStringLiteral("无法创建项目目录"));
    Document doc;
    for (int i = 1; i <= count; ++i) {
        if (!QDir().mkpath(QDir(path).filePath(QString::number(i)))) {
            QDir(path).removeRecursively();
            return fail(error, QStringLiteral("无法创建分集目录"));
        }
        doc.indexLines << QString::number(i);
        doc.episodes.append({QString::number(i), QString(),
                             QStringLiteral("https://www.youtube.com/watch?v=")});
    }
    QFile marker(QDir(path).filePath(label + QStringLiteral(".txt")));
    const bool markerReady = marker.open(QIODevice::WriteOnly);
    marker.close();
    if (!markerReady || !saveDocument(path, doc, error)) {
        QDir(path).removeRecursively();
        return fail(error, error && !error->isEmpty() ? *error : QStringLiteral("无法写入项目文件"));
    }
    QSaveFile icon(QDir(path).filePath(QStringLiteral("icon.txt")));
    const QByteArray iconValue = QStringLiteral(":/app/images/icons/牛排.svg").toUtf8();
    if (!icon.open(QIODevice::WriteOnly) ||
        icon.write(iconValue) != iconValue.size() || !icon.commit()) {
        QDir(path).removeRecursively();
        return fail(error, QStringLiteral("无法写入项目图标"));
    }
    return true;
}
bool createPlaylist(const QString& name, const QString& title,
                    const QVector<Episode>& episodes, QString* error) {
    if (episodes.isEmpty() || episodes.size() > 127)
        return fail(error, QStringLiteral("播放列表为空或集数超过 127"));
    if (!create(name, episodes.size(), title, error)) return false;
    Document doc;
    doc.episodes = episodes;
    for (const auto& episode : episodes) doc.indexLines << episode.originalTitle;
    if (saveDocument(localPath(name.trimmed()), doc, error)) return true;
    QDir(localPath(name.trimmed())).removeRecursively();
    return false;
}
bool importProject(const QString& path, bool copy, QString* error) {
    if (!looksLikeProject(path)) return fail(error, QStringLiteral("所选目录不是有效项目"));
    if (copy) {
        const QString target = localPath(QFileInfo(path).fileName());
        if (QFileInfo::exists(target)) return fail(error, QStringLiteral("同名项目已存在"));
        if (!copyTree(path, target, error)) {
            QDir(target).removeRecursively();
            return false;
        }
        return true;
    }
    QStringList list = links();
    if (list.contains(path) ||
        QDir::cleanPath(QDir(path).absolutePath()).toLower()
            == QDir::cleanPath(localPath(QFileInfo(path).fileName())).toLower())
        return fail(error, QStringLiteral("该项目已经导入"));
    list << path;
    return setLinks(list, error);
}
bool readFile(const QString& filePath, Document* doc, QString* error) {
    if (!doc) return fail(error, QStringLiteral("缺少输出参数"));
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    QString text = QString::fromUtf8(file.readAll());
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    const int separator = lines.indexOf(QString());
    if (separator < 0) return fail(error, QStringLiteral("标题.txt 格式无效"));
    Document result;
    result.indexLines = lines.mid(0, separator);
    if (result.indexLines.isEmpty() || separator + 3 >= lines.size())
        return fail(error, QStringLiteral("标题.txt 缺少分集记录"));
    result.translated = !lines.at(separator + 3).isEmpty();
    const int stride = result.translated ? 4 : 3;
    for (int i = 0; i < result.indexLines.size(); ++i) {
        const int start = separator + 1 + i * stride;
        if (start + stride > lines.size() || !lines.at(start + stride - 1).isEmpty())
            return fail(error, QStringLiteral("标题.txt 分集记录格式无效"));
        Episode ep;
        ep.originalTitle = lines.at(start);
        if (result.translated) ep.translatedTitle = lines.at(start + 1);
        ep.videoUrl = lines.at(start + stride - 2);
        result.episodes.append(ep);
    }
    *doc = result;
    return true;
}
bool read(const QString& path, Document* doc, QString* error) {
    return readFile(QDir(path).filePath(QStringLiteral("标题.txt")), doc, error);
}
bool writeDocument(const QString& path, const Document& doc, QString* error) {
    return saveDocument(path, doc, error);
}
bool update(const QString& path, const QString& name, const QString& title,
            const QString& icon, QString* error) {
    const QString clean = name.trimmed(), label = title.trimmed();
    if (!looksLikeProject(path) || !validName(clean) || !validName(label) ||
        label == QStringLiteral("标题") || label == QStringLiteral("icon"))
        return fail(error, QStringLiteral("项目信息无效"));
    const QDir folder(path);
    QString oldMarker;
    for (const QFileInfo& entry : folder.entryInfoList({QStringLiteral("*.txt")}, QDir::Files))
        if (entry.fileName() != QStringLiteral("标题.txt") && entry.fileName() != QStringLiteral("icon.txt")) {
            oldMarker = entry.absoluteFilePath();
            break;
        }
    const QString destination = QDir(QFileInfo(path).absolutePath()).filePath(clean);
    if (destination != folder.absolutePath() && QFileInfo::exists(destination))
        return fail(error, QStringLiteral("同名项目已存在"));
    const QString newMarker = folder.filePath(label + QStringLiteral(".txt"));
    const bool markerChanged = !oldMarker.isEmpty() && oldMarker != newMarker;
    const bool markerCreated = oldMarker.isEmpty();
    if (QFileInfo::exists(newMarker) && (markerChanged || markerCreated))
        return fail(error, QStringLiteral("原标题文件已存在"));
    if (markerChanged && !QFile::rename(oldMarker, newMarker))
        return fail(error, QStringLiteral("原标题文件重命名失败"));
    if (markerCreated) {
        QFile marker(newMarker);
        if (!marker.open(QIODevice::WriteOnly)) return fail(error, marker.errorString());
    }
    if (destination != folder.absolutePath() && !QDir().rename(folder.absolutePath(), destination)) {
        if (markerChanged) QFile::rename(newMarker, oldMarker);
        if (markerCreated) QFile::remove(newMarker);
        return fail(error, QStringLiteral("项目目录重命名失败"));
    }
    QSaveFile iconFile(QDir(destination).filePath(QStringLiteral("icon.txt")));
    const QByteArray iconBytes = icon.toUtf8();
    if (!iconFile.open(QIODevice::WriteOnly) ||
        iconFile.write(iconBytes) != iconBytes.size() || !iconFile.commit()) {
        if (destination != folder.absolutePath()) QDir().rename(destination, folder.absolutePath());
        if (markerChanged) QFile::rename(newMarker, oldMarker);
        if (markerCreated) QFile::remove(newMarker);
        return fail(error, QStringLiteral("项目图标写入失败"));
    }
    if (destination != folder.absolutePath()) {
        auto replace = [&](QStringList list, bool isLink) {
            for (QString& item : list) if (item == folder.absolutePath()) item = destination;
            return isLink ? setLinks(list, error) : setOrder(list, error);
        };
        if (!replace(links(), true) || !replace(order(), false)) return false;
    }
    return true;
}
bool remove(const QString& path, bool unlink, QString* error) {
    if (unlink) {
        QStringList list = links();
        if (!list.removeAll(path)) return fail(error, QStringLiteral("项目链接不存在"));
        QStringList sorted = order();
        sorted.removeAll(path);
        return setLinks(list, error) && setOrder(sorted, error);
    }
    // 项目库里的项目可以删除；历史遗留在软件目录里的项目也允许删，避免变成清理不掉的僵尸
    const QString parent = QDir::cleanPath(QFileInfo(path).absolutePath()).toLower();
    const bool inLibrary = parent == QDir::cleanPath(QDir(root()).absolutePath()).toLower();
    bool inSoftware = false;
    for (const QString& folder : softwareRoots())
        if (parent == folder.toLower()) {
            inSoftware = true;
            break;
        }
    if ((!inLibrary && !inSoftware) || !looksLikeProject(path))
        return fail(error, QStringLiteral("只能删除本地项目"));
    if (!QDir(path).removeRecursively())
        return fail(error, QStringLiteral("无法删除项目目录"));
    QStringList sorted = order();
    sorted.removeAll(path);
    return setOrder(sorted, error);
}
bool insertEpisode(const QString& path, int number, const Episode& episode, QString* error) {
    Document doc;
    if (!isProject(path) || !read(path, &doc, error)) return false;
    const int count = doc.episodes.size();
    if (QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() != count)
        return fail(error, QStringLiteral("分集目录与标题记录数量不一致"));
    if (number < 1 || number > count + 1)
        return fail(error, QStringLiteral("插入位置无效"));
    const auto episodePath = [&](int index) {
        return QDir(path).filePath(QString::number(index));
    };
    const auto restore = [&](int start) {
        for (int i = start; i <= count; ++i)
            QDir().rename(episodePath(i + 1), episodePath(i));
    };
    for (int i = count; i >= number; --i) {
        if (!QDir().rename(episodePath(i), episodePath(i + 1))) {
            restore(i + 1);
            return fail(error, QStringLiteral("分集目录重命名失败"));
        }
    }
    if (!QDir().mkpath(episodePath(number))) {
        restore(number);
        return fail(error, QStringLiteral("无法创建分集目录"));
    }
    doc.episodes.insert(number - 1, episode);
    doc.indexLines.insert(number - 1, episode.videoUrl);
    if (saveDocument(path, doc, error)) return true;
    QDir(episodePath(number)).removeRecursively();
    restore(number);
    return false;
}
bool deleteEpisode(const QString& path, int number, QString* error) {
    Document doc;
    if (!isProject(path) || !read(path, &doc, error)) return false;
    const int count = doc.episodes.size();
    if (QDir(path).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() != count)
        return fail(error, QStringLiteral("分集目录与标题记录数量不一致"));
    if (count <= 1 || number < 1 || number > count)
        return fail(error, QStringLiteral("不能删除最后一集或集数无效"));
    const auto episodePath = [&](int index) {
        return QDir(path).filePath(QString::number(index));
    };
    const QString backup = path + QStringLiteral(".deleted-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().rename(episodePath(number), backup))
        return fail(error, QStringLiteral("无法暂存待删除的分集目录"));
    const auto restore = [&](int lastMoved) {
        for (int i = lastMoved; i > number; --i)
            QDir().rename(episodePath(i - 1), episodePath(i));
        QDir().rename(backup, episodePath(number));
    };
    for (int i = number + 1; i <= count; ++i) {
        if (!QDir().rename(episodePath(i), episodePath(i - 1))) {
            restore(i - 1);
            return fail(error, QStringLiteral("分集目录重命名失败"));
        }
    }
    doc.episodes.removeAt(number - 1);
    if (doc.indexLines.size() >= number) doc.indexLines.removeAt(number - 1);
    if (!saveDocument(path, doc, error)) {
        restore(count);
        return false;
    }
    if (!QDir(backup).removeRecursively())
        return fail(error, QStringLiteral("分集已移除，但临时目录未能清理：") + backup);
    return true;
}
bool editEpisode(const QString& path, int number, const Episode& episode, QString* error) {
    Document doc;
    if (!read(path, &doc, error)) return false;
    if (number < 1 || number > doc.episodes.size())
        return fail(error, QStringLiteral("集数无效"));
    doc.episodes[number - 1] = episode;
    return saveDocument(path, doc, error);
}
} // namespace fkw::projects
