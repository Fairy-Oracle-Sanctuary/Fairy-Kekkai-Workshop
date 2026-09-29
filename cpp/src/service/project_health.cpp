#include "service/project_health.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace fkw::projects {
namespace {
const QString kTitleName = QStringLiteral("标题.txt");
const QString kIconName = QStringLiteral("icon.txt");
const QString kDefaultIcon = QStringLiteral(":/app/images/icons/牛排.svg");
const QString kDefaultUrl = QStringLiteral("https://www.youtube.com/watch?v=");

bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

QString numberList(const QVector<int>& numbers) {
    QStringList items;
    const int limit = 12;
    for (int i = 0; i < numbers.size() && i < limit; ++i) items << QString::number(numbers.at(i));
    if (numbers.size() > limit) items << QStringLiteral("…");
    return items.join(QStringLiteral(", "));
}

QStringList splitLines(const QString& filePath, bool* opened) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (opened) *opened = false;
        return {};
    }
    QString text = QString::fromUtf8(file.readAll());
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& line : lines) line = line.trimmed();
    if (opened) *opened = true;
    return lines;
}

bool isUrlLine(const QString& line) {
    return line.contains(QStringLiteral("://")) || line.startsWith(QStringLiteral("http"));
}

// 宽松解析：容忍 CRLF、多余或缺失的空行、缺失末尾 "---"、首集译文为空等情况。
bool looseRead(const QString& filePath, Document* document) {
    bool opened = false;
    QStringList lines = splitLines(filePath, &opened);
    if (!opened || lines.isEmpty()) return false;
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    if (!lines.isEmpty() && lines.last() == QStringLiteral("---")) lines.removeLast();
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    if (lines.isEmpty()) return false;

    const int separator = lines.indexOf(QString());
    QStringList indexLines;
    QStringList body;
    if (separator >= 0) {
        indexLines = lines.mid(0, separator);
        body = lines.mid(separator + 1);
    } else {
        body = lines;
    }
    QStringList content;
    for (const QString& line : body)
        if (!line.isEmpty() && line != QStringLiteral("---")) content << line;
    if (content.isEmpty()) return false;

    QVector<QStringList> groups;
    if (!indexLines.isEmpty() && content.size() % indexLines.size() == 0) {
        const int stride = content.size() / indexLines.size();
        if (stride == 2 || stride == 3)
            for (int i = 0; i < indexLines.size(); ++i)
                groups.append(content.mid(i * stride, stride));
    }
    if (groups.isEmpty()) {
        // 无法按固定步长切分时，用 URL 行做锚点
        QStringList current;
        for (const QString& line : content) {
            current << line;
            if (isUrlLine(line)) {
                groups.append(current);
                current.clear();
            }
        }
        if (!current.isEmpty()) groups.append(current);
        if (groups.isEmpty()) return false;
    }

    Document result;
    result.indexLines = indexLines;
    for (const QStringList& group : groups) {
        Episode episode;
        episode.originalTitle = group.first();
        episode.videoUrl = group.last();
        if (group.size() >= 3) {
            result.translated = true;
            episode.translatedTitle = group.at(1);
        }
        result.episodes.append(episode);
    }
    if (result.episodes.isEmpty()) return false;
    *document = result;
    return true;
}

bool loadDocument(const QString& filePath, Document* document) {
    return readFile(filePath, document, nullptr) || looseRead(filePath, document);
}

QString markerFile(const QDir& dir) {
    for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files)) {
        if (entry.fileName() == kTitleName || entry.fileName() == kIconName) continue;
        return entry.absoluteFilePath();
    }
    return {};
}

QString iconValue(const QDir& dir) {
    QFile file(dir.filePath(kIconName));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QStringList tempLeftovers(const QString& path) {
    QStringList result;
    const QFileInfo info(path);
    const QString prefix = info.fileName() + QStringLiteral(".deleted-");
    const QDir parent(info.absolutePath());
    for (const QFileInfo& entry : parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (entry.fileName().startsWith(prefix)) result << entry.absoluteFilePath();
    return result;
}
} // namespace

Health check(const QString& path) {
    Health health;
    const QDir dir(path);
    if (!dir.exists()) {
        health.level = HealthLevel::Broken;
        health.blocking = true;
        health.issues.append({IssueCode::ProjectDirMissing,
            QStringLiteral("项目目录不存在或已被移动"), {}});
        return health;
    }

    const QVector<int> numbers = episodeNumbers(path);
    health.dirCount = numbers.size();
    const int maxNumber = numbers.isEmpty() ? 0 : numbers.last();

    const QString titleFile = dir.filePath(kTitleName);
    Document document;
    bool recordsKnown = false;
    if (QFileInfo::exists(titleFile)) {
        if (loadDocument(titleFile, &document)) recordsKnown = true;
        else health.issues.append({IssueCode::TitleFileBroken,
            QStringLiteral("标题.txt 内容已被改乱，无法按原格式读取"), {}});
    } else {
        QString renamed;
        for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files)) {
            if (entry.fileName() == kIconName || entry.size() == 0) continue;
            Document probe;
            if (loadDocument(entry.absoluteFilePath(), &probe)) {
                renamed = entry.fileName();
                document = probe;
                break;
            }
        }
        if (!renamed.isEmpty()) {
            recordsKnown = true;
            health.issues.append({IssueCode::TitleFileRenamed,
                QStringLiteral("标题.txt 被改名为 %1").arg(renamed), {}});
        } else {
            health.issues.append({IssueCode::TitleFileMissing,
                numbers.isEmpty()
                    ? QStringLiteral("缺少 标题.txt")
                    : QStringLiteral("缺少 标题.txt，将按连续分集目录 1..%1 补建标题记录，"
                                     "原有标题与链接已无法恢复")
                          .arg(numbers.size()), {}});
        }
    }

    health.recordsKnown = recordsKnown;
    health.recordCount = recordsKnown ? document.episodes.size() : 0;
    health.targetCount = qMax(health.recordCount, maxNumber);
    if (health.targetCount <= 0) {
        health.level = HealthLevel::Broken;
        health.blocking = true;
        health.issues.append({IssueCode::NoEpisodeContent,
            QStringLiteral("没有任何分集目录或标题记录，无法自动修复"), {}});
        return health;
    }
    if (recordsKnown) {
        for (int i = 1; i <= health.targetCount; ++i)
            if (!numbers.contains(i)) health.missingNumbers.append(i);
        for (int number : numbers)
            if (number > health.recordCount) health.extraNumbers.append(number);
        if (!health.missingNumbers.isEmpty())
            health.issues.append({IssueCode::EpisodeDirMissing,
                QStringLiteral("缺少分集目录 %1，将补建空目录")
                    .arg(numberList(health.missingNumbers)), health.missingNumbers});
        if (!health.extraNumbers.isEmpty())
            health.issues.append({IssueCode::EpisodeRecordMissing,
                QStringLiteral("目录 %1 缺少标题记录，将补写记录")
                    .arg(numberList(health.extraNumbers)), health.extraNumbers});
    }
    if (markerFile(dir).isEmpty())
        health.issues.append({IssueCode::MarkerFileMissing,
            QStringLiteral("缺少原标题标记文件，将按目录名补建"), {}});
    if (iconValue(dir).isEmpty())
        health.issues.append({IssueCode::IconFileMissing,
            QStringLiteral("缺少项目图标，将写入默认图标"), {}});
    if (!tempLeftovers(path).isEmpty())
        health.issues.append({IssueCode::TempDirLeftover,
            QStringLiteral("存在删除操作残留的暂存目录，将清理"), {}});

    // 结构性损坏才会阻止打开；缺图标、缺标记文件、残留暂存目录只提示可优化
    for (const Issue& issue : health.issues) {
        switch (issue.code) {
        case IssueCode::MarkerFileMissing:
        case IssueCode::IconFileMissing:
        case IssueCode::TempDirLeftover:
            break;
        default:
            health.blocking = true;
            break;
        }
    }
    health.level = health.issues.isEmpty() ? HealthLevel::Ok : HealthLevel::Repairable;
    return health;
}

bool repair(const QString& path, RepairReport* report, QString* error) {
    RepairReport local;
    if (!report) report = &local;
    *report = RepairReport{};

    const QDir dir(path);
    if (!dir.exists()) return fail(error, QStringLiteral("项目目录不存在，无法自动修复"));

    const Health health = check(path);
    if (health.level == HealthLevel::Broken)
        return fail(error, QStringLiteral("项目损坏严重，无法自动修复：")
            + (health.issues.isEmpty() ? QStringLiteral("原因未知")
                                       : health.issues.first().detail));
    if (health.level == HealthLevel::Ok) return true;

    // 1. 定位 标题.txt，必要时恢复被改名的文件
    const QString titleFile = dir.filePath(kTitleName);
    Document document;
    bool recordsKnown = false;
    bool strictParsed = false;
    if (QFileInfo::exists(titleFile)) {
        strictParsed = readFile(titleFile, &document, nullptr);
        recordsKnown = strictParsed || looseRead(titleFile, &document);
    } else {
        for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files)) {
            if (entry.fileName() == kIconName || entry.size() == 0) continue;
            Document probe;
            if (!loadDocument(entry.absoluteFilePath(), &probe)) continue;
            if (!QFile::rename(entry.absoluteFilePath(), titleFile))
                return fail(error, QStringLiteral("无法把 %1 恢复为 标题.txt")
                    .arg(entry.fileName()));
            document = probe;
            recordsKnown = true;
            strictParsed = readFile(titleFile, &document, nullptr);
            report->changed = true;
            report->actions << QStringLiteral("恢复被改名的 标题.txt（原文件名 %1）")
                .arg(entry.fileName());
            break;
        }
    }

    // 2. 集数目标：标题记录与分集目录互相补齐
    const QVector<int> numbers = episodeNumbers(path);
    const int maxNumber = numbers.isEmpty() ? 0 : numbers.last();
    const int target = qMax(recordsKnown ? static_cast<int>(document.episodes.size()) : 0,
                            maxNumber);
    if (target <= 0)
        return fail(error, QStringLiteral("没有任何分集目录或标题记录，无法自动修复"));

    const int previousCount = document.episodes.size();
    while (document.episodes.size() < target) {
        const int number = document.episodes.size() + 1;
        document.episodes.append({QStringLiteral("第 %1 集").arg(number), QString(), kDefaultUrl});
    }
    if (document.episodes.size() > previousCount) {
        QStringList added;
        for (int i = previousCount; i < document.episodes.size(); ++i)
            added << QString::number(i + 1);
        report->actions << QStringLiteral("补写分集记录 %1")
            .arg(added.join(QStringLiteral(", ")));
    }

    // 3. 备份 标题.txt
    QString backup;
    if (QFileInfo::exists(titleFile)) {
        backup = titleFile + QStringLiteral(".bak-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        if (!QFile::copy(titleFile, backup))
            return fail(error, QStringLiteral("无法备份 标题.txt，已取消修复"));
        report->backupPath = backup;
    }

    // 4. 回写 标题.txt（仅在内容非规范或补写了记录时）
    Document verify;
    const bool canonical = QFileInfo::exists(titleFile) && readFile(titleFile, &verify, nullptr);
    if (!canonical || document.episodes.size() > previousCount) {
        if (!writeDocument(path, document, error)) {
            if (!backup.isEmpty()) QFile::remove(backup);
            return false;
        }
        report->changed = true;
        report->actions << QStringLiteral("重写 标题.txt");
    }

    // 5. 补齐空分集目录
    QVector<int> created;
    for (int number = 1; number <= target; ++number) {
        const QString episodePath = dir.filePath(QString::number(number));
        if (QFileInfo(episodePath).isDir()) continue;
        if (QFileInfo(episodePath).exists()) {
            if (!backup.isEmpty()) QFile::remove(backup);
            return fail(error, QStringLiteral("第 %1 集路径被同名文件占用，已取消修复")
                .arg(number));
        }
        if (!QDir().mkpath(episodePath)) {
            for (int made : created) QDir(dir.filePath(QString::number(made))).removeRecursively();
            if (!backup.isEmpty()) {
                QFile::remove(titleFile);
                QFile::rename(backup, titleFile);
            }
            return fail(error, QStringLiteral("无法创建第 %1 集目录，已回滚").arg(number));
        }
        created.append(number);
    }
    if (!created.isEmpty()) {
        QStringList added;
        for (int number : created) added << QString::number(number);
        report->changed = true;
        report->actions << QStringLiteral("补齐空分集目录 %1")
            .arg(added.join(QStringLiteral(", ")));
    }

    // 6. 辅助文件
    if (markerFile(dir).isEmpty()) {
        const QString name = dir.dirName();
        if (name != QStringLiteral("标题") && name != QStringLiteral("icon")) {
            QFile marker(dir.filePath(name + QStringLiteral(".txt")));
            if (marker.open(QIODevice::WriteOnly)) {
                marker.close();
                report->changed = true;
                report->actions << QStringLiteral("补建原标题标记文件 %1.txt").arg(name);
            }
        }
    }
    if (iconValue(dir).isEmpty()) {
        QSaveFile icon(dir.filePath(kIconName));
        const QByteArray value = kDefaultIcon.toUtf8();
        if (icon.open(QIODevice::WriteOnly) && icon.write(value) == value.size() && icon.commit()) {
            report->changed = true;
            report->actions << QStringLiteral("写入默认项目图标");
        }
    }
    for (const QString& leftover : tempLeftovers(path))
        if (QDir(leftover).removeRecursively()) {
            report->changed = true;
            report->actions << QStringLiteral("清理残留暂存目录 %1")
                .arg(QFileInfo(leftover).fileName());
        }

    // 7. 回读校验，失败回滚
    const Health after = check(path);
    if (!after.ok()) {
        for (int number : created) QDir(dir.filePath(QString::number(number))).removeRecursively();
        if (!backup.isEmpty()) {
            QFile::remove(titleFile);
            QFile::rename(backup, titleFile);
        }
        return fail(error, QStringLiteral("修复后校验未通过，已回滚：")
            + (after.issues.isEmpty() ? QStringLiteral("原因未知")
                                      : after.issues.first().detail));
    }
    return true;
}
} // namespace fkw::projects
