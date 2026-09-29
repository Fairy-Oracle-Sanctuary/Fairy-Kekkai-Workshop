#pragma once
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QSharedPointer>
#include <QString>

namespace fkw {
class Logger {
public:
    static QSharedPointer<Logger> get(const QString& fileName,
                                      const QString& logName,
                                      bool printConsole = true);
    Logger(const QString& fileName, const QString& logName, bool printConsole = true);
    ~Logger();
    void info(const QString& message);
    void error(const QString& message);
    void debug(const QString& message);
    void warning(const QString& message);
    void critical(const QString& message);
    void close();
    void closeLogger() { close(); }
    // 对应 Python Logger.logFile.absolute()，供任务记录日志路径。
    QString logFilePath() const { return QFileInfo(file_).absoluteFilePath(); }
    static int cleanOldLogs(int retentionDays = 30);
    static int clearAllLogs();
private:
    void log(const QString& level, const QString& message);
    QFile file_;
    QString logName_;
    bool printConsole_ = true;
    QMutex mutex_;
};
}  // namespace fkw
