#include "service/project_relocate.h"

#include <functional>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "service/project_service.h"
#include "common/event_bus.h"

namespace fkw::projects {
namespace {
qint64 treeBytes(const QString& path) {
    qint64 bytes = 0;
    QDirIterator entries(path, QDir::Files | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        entries.next();
        bytes += entries.fileInfo().size();
    }
    return bytes;
}

// 逐层复制目录树，边复制边汇报已完成字节；任何一步失败都由调用方清理目标目录
bool copyTree(const QString& from, const QString& to, qint64* done,
              const std::function<void()>& tick, QString* error) {
    if (!QDir().mkpath(to)) {
        *error = QStringLiteral("无法创建目录：") + to;
        return false;
    }
    const QDir source(from);
    const QDir destination(to);
    for (const QFileInfo& entry : source.entryInfoList(
             QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString target = destination.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, done, tick, error)) return false;
            continue;
        }
        QFile input(entry.absoluteFilePath());
        if (!input.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("无法读取：") + entry.fileName();
            return false;
        }
        QFile output(target);
        if (!output.open(QIODevice::WriteOnly)) {
            *error = QStringLiteral("无法写入：") + entry.fileName();
            return false;
        }
        while (!input.atEnd()) {
            const QByteArray chunk = input.read(1 << 16);
            if (chunk.isEmpty()) {
                if (input.error() == QFileDevice::NoError) break;
                *error = QStringLiteral("读取中断：") + entry.fileName();
                return false;
            }
            if (output.write(chunk) != chunk.size()) {
                *error = QStringLiteral("写入中断：") + entry.fileName();
                return false;
            }
            *done += chunk.size();
            tick();
        }
        input.close();
        output.close();
        if (QFileInfo(target).size() != entry.size()) {
            *error = QStringLiteral("复制后大小不一致：") + entry.fileName();
            return false;
        }
    }
    return true;
}

// 删除原目录之前再逐个文件核对一遍，确认真的复制完整了
bool verifyTree(const QString& from, const QString& to, QString* error) {
    const QDir source(from);
    QDirIterator entries(from, QDir::Files | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        entries.next();
        const QFileInfo entry = entries.fileInfo();
        const QString relative = source.relativeFilePath(entry.absoluteFilePath());
        const QFileInfo copied(QDir(to).filePath(relative));
        if (!copied.exists() || copied.size() != entry.size()) {
            *error = QStringLiteral("校验失败：") + relative;
            return false;
        }
    }
    return true;
}
} // namespace

bool RelocateReport::allOk() const {
    for (const RelocateRecord& record : records)
        if (!record.ok) return false;
    return true;
}

QVector<QPair<QString, QString>> RelocateReport::moved() const {
    QVector<QPair<QString, QString>> result;
    for (const RelocateRecord& record : records)
        if (record.ok && !record.destination.isEmpty())
            result.append({record.source, record.destination});
    return result;
}

QStringList RelocateReport::failedSources() const {
    QStringList result;
    for (const RelocateRecord& record : records)
        if (!record.ok && !result.contains(record.source)) result << record.source;
    return result;
}

QStringList projectsIn(const QString& folder) {
    QStringList result;
    const QDir dir(folder);
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (looksLikeProject(entry.absoluteFilePath())) result << entry.absoluteFilePath();
    return result;
}

void applyRelocation(const RelocateReport& report) {
    const QVector<QPair<QString, QString>> moved = report.moved();
    const auto rewrite = [&moved](QStringList list) {
        for (QString& item : list)
            for (const auto& pair : moved)
                if (item == pair.first) item = pair.second;
        // 路径改写后可能撞车，这里顺手去重
        QStringList unique;
        for (const QString& item : list)
            if (!unique.contains(item)) unique << item;
        return unique;
    };
    QStringList currentLinks = rewrite(links());
    // 没搬走的本地项目降级成链接，否则它会因为不在项目库里而从列表里消失
    for (const QString& path : report.failedSources())
        if (!currentLinks.contains(path)) currentLinks << path;
    setLinks(currentLinks, nullptr);
    setOrder(rewrite(order()), nullptr);
    for (const auto& pair : moved)
        emit GlobalEventBus::instance().project_updated(QJsonObject{
            {QStringLiteral("old_path"), pair.first},
            {QStringLiteral("path"), pair.second}});
}

RelocateWorker::RelocateWorker(const QStringList& sources, const QString& target,
                               QObject* parent)
    : QObject(parent), sources_(sources), target_(target) {}

void RelocateWorker::reportProgress() {
    const int percent = totalBytes_ > 0
        ? static_cast<int>(completedBytes_ * 100 / totalBytes_)
        : static_cast<int>(doneItems_ * 100 / qMax(1, sources_.size()));
    const int bounded = qBound(0, percent, 100);
    if (bounded == lastPercent_) return;
    lastPercent_ = bounded;
    emit progressed(bounded);
}

bool RelocateWorker::relocate(const QString& source, qint64 bytes, QString* destination,
                              QString* message) {
    const QString name = QFileInfo(source).fileName();
    QString target = QDir(target_).filePath(name);
    if (QFileInfo::exists(target)) {
        // 新目录里已经有同名项目，改名而不是覆盖，两个都不能丢
        int suffix = 2;
        while (QFileInfo::exists(target = QDir(target_).filePath(
                   QStringLiteral("%1-%2").arg(name).arg(suffix))))
            ++suffix;
        *message = QStringLiteral("新目录已有同名项目，已改名为 %1")
                       .arg(QFileInfo(target).fileName());
    }
    if (QDir().rename(source, target)) {
        completedBytes_ += bytes;
        *destination = target;
        reportProgress();
        return true;
    }
    // 跨盘时改名会失败：改成 复制 -> 校验 -> 删源，任何一步出问题都保留原目录
    const qint64 before = completedBytes_;
    QString error;
    if (!copyTree(source, target, &completedBytes_, [this]() { reportProgress(); }, &error)) {
        QDir(target).removeRecursively();
        completedBytes_ = before;
        reportProgress();
        *message = error;
        return false;
    }
    if (!verifyTree(source, target, &error)) {
        QDir(target).removeRecursively();
        completedBytes_ = before;
        reportProgress();
        *message = error;
        return false;
    }
    *destination = target;
    if (!QDir(source).removeRecursively())
        *message = QStringLiteral("项目已复制到新位置，但原目录删除失败，请手动清理");
    return true;
}

void RelocateWorker::run() {
    QDir().mkpath(target_);
    QVector<qint64> sizes;
    sizes.reserve(sources_.size());
    for (const QString& source : sources_) {
        const qint64 bytes = treeBytes(source);
        sizes.append(bytes);
        totalBytes_ += bytes;
    }
    emit progressed(0);
    for (int i = 0; i < sources_.size(); ++i) {
        const QString source = sources_.at(i);
        emit itemStarted(source, i + 1, sources_.size());
        QString destination;
        QString message;
        const bool ok = relocate(source, sizes.at(i), &destination, &message);
        ++doneItems_;
        emit itemFinished(source, destination, ok, message);
        reportProgress();
    }
    emit progressed(100);
    emit done();
}
} // namespace fkw::projects
