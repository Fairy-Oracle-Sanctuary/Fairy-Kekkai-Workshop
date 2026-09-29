#include "common/logger.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QWeakPointer>
#include <cstdio>
#include "common/app_data.h"
#include "common/event_bus.h"

namespace fkw {
namespace {
QString logFolder() {
    return QDir(userDataFolder()).absoluteFilePath(QStringLiteral("Log"));
}
QMutex& cacheMutex() {
    static QMutex mutex;
    return mutex;
}
QMutex& logIoMutex() {
    static QMutex mutex;
    return mutex;
}
QHash<QString, QWeakPointer<Logger>>& loggerCache() {
    static QHash<QString, QWeakPointer<Logger>> cache;
    return cache;
}
}
QSharedPointer<Logger> Logger::get(const QString& fileName,
                                   const QString& logName, bool printConsole) {
    QMutexLocker locker(&cacheMutex());
    auto logger = loggerCache().value(fileName).toStrongRef();
    if (!logger) {
        logger = QSharedPointer<Logger>::create(fileName, logName, printConsole);
        loggerCache().insert(fileName, logger.toWeakRef());
    }
    return logger;
}
Logger::Logger(const QString& fileName, const QString& logName, bool printConsole)
    : file_(QDir(logFolder()).filePath(fileName + QStringLiteral(".log"))),
      logName_(logName), printConsole_(printConsole) {
    QDir().mkpath(QFileInfo(file_).absolutePath());
    file_.open(QIODevice::WriteOnly | QIODevice::Append);
}
Logger::~Logger() { close(); }
void Logger::info(const QString& message) { log(QStringLiteral("INFO"), message); }
void Logger::error(const QString& message) { log(QStringLiteral("ERROR"), message); }
void Logger::debug(const QString& message) { log(QStringLiteral("DEBUG"), message); }
void Logger::warning(const QString& message) { log(QStringLiteral("WARNING"), message); }
void Logger::critical(const QString& message) { log(QStringLiteral("CRITICAL"), message); }
void Logger::log(const QString& level, const QString& message) {
    const QString prefix = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QStringLiteral(" - ") + level
        + QStringLiteral(" - ");
    const QString line = prefix + message + QLatin1Char('\n');
    {
        QMutexLocker ioLocker(&logIoMutex());
        QMutexLocker locker(&mutex_);
        if (!file_.isOpen()) file_.open(QIODevice::WriteOnly | QIODevice::Append);
        if (printConsole_) {
            const QByteArray bytes = line.toUtf8();
            std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
            std::fflush(stdout);
        }
        if (file_.isOpen()) {
            static const QRegularExpression ansi(
                QStringLiteral("\\x1B(?:[@-Z\\\\-_]|\\[[0-?]*[ -/]*[@-~])"));
            const QByteArray bytes = (prefix + QString(message).replace(ansi, QString()) +
                                      QLatin1Char('\n')).toUtf8();
            file_.write(bytes);
            file_.flush();
        }
    }
    emit GlobalEventBus::instance().log_message(logName_, line);
}
void Logger::close() {
    QMutexLocker locker(&mutex_);
    if (file_.isOpen()) file_.close();
}
int Logger::clearAllLogs() {
    QMutexLocker ioLocker(&logIoMutex());
    QList<QSharedPointer<Logger>> active;
    {
        QMutexLocker cacheLocker(&cacheMutex());
        for (auto it = loggerCache().cbegin(); it != loggerCache().cend(); ++it) {
            if (auto logger = it.value().toStrongRef()) active.append(logger);
        }
    }
    for (const auto& logger : active) logger->close();
    const QDir folder(logFolder());
    int removed = 0;
    for (const QFileInfo& info : folder.entryInfoList({QStringLiteral("*.log")}, QDir::Files)) {
        if (QFile::remove(info.absoluteFilePath())) ++removed;
        else qWarning() << "Failed to delete log file:" << info.absoluteFilePath();
    }
    return removed;
}
int Logger::cleanOldLogs(int retentionDays) {
    if (!QDir(logFolder()).exists()) return 0;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-retentionDays);
    QDirIterator files(logFolder(), {QStringLiteral("*.log")}, QDir::Files,
                       QDirIterator::Subdirectories);
    int removed = 0;
    while (files.hasNext()) {
        const QString path = files.next();
        if (QFileInfo(path).lastModified() < cutoff && QFile::remove(path))
            ++removed;
    }
    return removed;
}
}  // namespace fkw
