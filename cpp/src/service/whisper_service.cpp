#include "service/whisper_service.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QVariant>
#include <algorithm>

#include "common/app_data.h"
#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/task_status.h"
#include "common/utils.h"

namespace fkw {
namespace {

// 进度解析正则：时间戳行，如 [00:00:01.480 --> 00:00:03.200]
const QRegularExpression& timestampRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(\[(\d+:\d+:\d+\.\d+)\s*-->\s*(\d+:\d+:\d+\.\d+)\])"));
    return re;
}
const QRegularExpression& durationRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Duration:\s*(\d+):(\d+):(\d+\.\d+))"));
    return re;
}

// 时间戳转秒（对齐 Python _timestamp_to_seconds）：
// 00:00:01.480 或 00:01:30，解析失败返回 0
double timestampToSeconds(const QString& timestamp) {
    const QStringList parts = timestamp.trimmed().split(QLatin1Char(':'));
    bool ok = false;
    if (parts.size() == 3) {
        const int hours = parts.at(0).toInt(&ok);
        if (!ok) return 0;
        const int minutes = parts.at(1).toInt(&ok);
        if (!ok) return 0;
        const double seconds = parts.at(2).toDouble(&ok);
        if (!ok) return 0;
        return hours * 3600 + minutes * 60 + seconds;
    }
    if (parts.size() == 2) {
        const int minutes = parts.at(0).toInt(&ok);
        if (!ok) return 0;
        const double seconds = parts.at(1).toDouble(&ok);
        if (!ok) return 0;
        return minutes * 60 + seconds;
    }
    return 0;
}

bool isErrorLine(const QString& line) {
    const QString lower = line.toLower();
    return lower.contains(QStringLiteral("error")) ||
           lower.contains(QStringLiteral("failed")) ||
           line.contains(QStringLiteral("错误"));
}

}  // namespace

WhisperTask::WhisperTask(const QString& videoPath, const QString& outputPath)
    : TaskBase(videoPath, outputPath) {}

QString getWhisperCliPath() {
    const QString custom =
        AppConfig::instance().value(ConfigKeys::whisperCliPath).toString();
    if (!custom.isEmpty() && QFileInfo::exists(custom)) return custom;
#ifdef Q_OS_WIN
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/whisper/main.exe"));
#else
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/whisper/main"));
#endif
}

QStringList buildWhisperCommand(const WhisperTask& task) {
    QStringList cmd;

    cmd << QStringLiteral("-f") << task.inputPath;

    // 仅在指定了有效语言时传递 -l 参数，否则让 whisper 自动检测
    if (!task.language.isEmpty() && task.language != QStringLiteral("auto"))
        cmd << QStringLiteral("-l") << task.language;

    // 输出格式
    if (task.format == QStringLiteral("srt"))
        cmd << QStringLiteral("-osrt");
    else if (task.format == QStringLiteral("txt"))
        cmd << QStringLiteral("-otxt");
    else if (task.format == QStringLiteral("vtt"))
        cmd << QStringLiteral("-ovtt");

    // 当 GPU 不为空时添加 GPU 参数（包括"自动检测"）
    if (!task.gpu.isEmpty()) cmd << QStringLiteral("-gpu");

    // 模型路径
    if (!task.modelFile.isEmpty())
        cmd << QStringLiteral("-m") << task.modelFile;

    return cmd;
}

WhisperWorker::WhisperWorker(std::shared_ptr<WhisperTask> task)
    : TaskWorker(nullptr), task_(std::move(task)) {}

WhisperWorker::~WhisperWorker() {
    // 兜底：正常路径下 run() 结束时进程已回收
    QMutexLocker locker(&processMutex_);
    if (process_) {
        if (process_->state() != QProcess::NotRunning) process_->kill();
        process_->waitForFinished(1000);
        delete process_;
        process_ = nullptr;
    }
}

void WhisperWorker::run() {
    const QString currentTime =
        task_->createTime.toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
    taskLogger_ = Logger::get(
        QStringLiteral("Tasks/") + currentTime + QStringLiteral("_taskID-") +
            QString::number(task_->taskId),
        QStringLiteral("whisper"));
    task_->logPath = taskLogger_->logFilePath();

    const QString cliPath = getWhisperCliPath();
    if (!QFileInfo::exists(cliPath)) {
        taskLogger_->error(QStringLiteral("main.exe 不存在: ") + cliPath);
        finish(false);
        deleteLater();
        return;
    }
    if (!task_->modelFile.isEmpty() && !QFileInfo::exists(task_->modelFile)) {
        taskLogger_->error(QStringLiteral("模型文件不存在: ") + task_->modelFile);
        finish(false);
        deleteLater();
        return;
    }

    const QString outputDir = pathDirname(task_->outputPath);
    if (!outputDir.isEmpty()) QDir().mkpath(outputDir);

    // 获取视频总时长用于进度计算
    duration_ = probeVideoDuration();

    const QStringList cmd = buildWhisperCommand(*task_);
    taskLogger_->info(QStringLiteral("args: ") + cliPath + QLatin1Char(' ') +
                      cmd.join(QLatin1Char(' ')));

    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, 0, QVariant(int(TaskStatus::Processing)),
        QStringLiteral("0KiB"), 0.0, QStringLiteral("0kbits/s"), 0.0);

    // QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失
    {
        QMutexLocker locker(&processMutex_);
        process_ = new QProcess();  // 创建于 Worker 线程，信号在本线程分发
    }
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyReadStandardOutput, this,
            &WhisperWorker::handleStdout, Qt::DirectConnection);
    QEventLoop loop;
    connect(process_, &QProcess::finished, &loop, &QEventLoop::quit,
            Qt::DirectConnection);
    process_->setProgram(cliPath);
    process_->setArguments(cmd);
    // 设置工作目录为 whisper 目录，方便加载相对路径的模型
    process_->setWorkingDirectory(pathDirname(cliPath));
    process_->start();
    if (!process_->waitForStarted()) {
        destroyProcess();
        finish(false);
        deleteLater();
        return;
    }
    loop.exec();

    if (cancelled_.load()) {
        destroyProcess();
        // 取消的任务不 emit 完成信号
        deleteLater();
        return;
    }

    const int exitCode = destroyProcess();
    const bool success = exitCode == 0;
    if (success) {
        activateOutput();
        taskLogger_->info(QStringLiteral("Whisper转录完成: -%1- 输出: %2")
                              .arg(task_->inputPath, task_->outputPath));
    } else {
        QString errorMsg =
            QStringLiteral("Whisper转录失败，错误码: %1").arg(exitCode);
        if (!outputLines_.isEmpty()) {
            errorMsg += QStringLiteral("\n最后输出:\n") +
                        outputLines_.mid(qMax(0, outputLines_.size() - 5))
                            .join(QLatin1Char('\n'));
        }
        taskLogger_->error(errorMsg);
    }
    finish(success);
    // QObject 归属主线程，投递到主线程事件循环回收
    deleteLater();
}

void WhisperWorker::cancel() {
    cancelled_.store(true);
    QMutexLocker locker(&processMutex_);
    if (process_) process_->kill();
}

int WhisperWorker::destroyProcess() {
    QMutexLocker locker(&processMutex_);
    if (!process_) return -1;
    const int exitCode = process_->exitCode();
    if (process_->state() != QProcess::NotRunning) process_->kill();
    delete process_;
    process_ = nullptr;
    return exitCode;
}

double WhisperWorker::probeVideoDuration() {
    const QString ffmpeg =
        AppConfig::instance().value(ConfigKeys::ffmpegPath).toString();
    if (ffmpeg.isEmpty() || !QFileInfo::exists(ffmpeg)) return 0;

    QProcess process;
    process.start(ffmpeg, {QStringLiteral("-i"), task_->inputPath});
    process.waitForFinished(5000);
    const QString output = QString::fromUtf8(process.readAllStandardError());

    // 从 FFmpeg 输出中解析时长信息: Duration: 00:00:05.03
    const auto match = durationRe().match(output);
    if (match.hasMatch()) {
        const double totalSeconds = match.captured(1).toInt() * 3600 +
                                    match.captured(2).toInt() * 60 +
                                    match.captured(3).toDouble();
        taskLogger_->info(
            QStringLiteral("获取视频时长成功: %1秒").arg(totalSeconds));
        return totalSeconds;
    }
    return 0;
}

void WhisperWorker::handleStdout() {
    // 进程指针仅在 destroyProcess（loop.exec 返回后）置空，此处无需加锁
    if (!process_) return;
    QString data = QString::fromUtf8(process_->readAllStandardOutput());
    data.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    data.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = data.split(QLatin1Char('\n'));

    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        outputLines_.append(line);
        taskLogger_->info(QStringLiteral("Whisper: ") + line);

        if (cancelled_.load()) continue;

        // 进度行 → 刷新式日志（覆盖上一行）+ 进度上报
        const std::optional<int> progress = parseProgress(line);
        if (progress) {
            if (*progress != lastProgress_) {
                lastProgress_ = *progress;
                emit GlobalEventBus::instance().updateTaskStatusSig(
                    task_->taskId, *progress, QVariant(int(TaskStatus::Processing)),
                    QStringLiteral("0KiB"), 0.0, QStringLiteral("0kbits/s"), 0.0);
            }
            emit GlobalEventBus::instance().taskLogSignal(
                QStringLiteral("whisper"), line, false, true);
            continue;
        }

        // 错误行 → 红色错误日志
        if (isErrorLine(line)) {
            emit GlobalEventBus::instance().taskLogSignal(
                QStringLiteral("whisper"), line, true, false);
            continue;
        }

        // 普通日志
        emit GlobalEventBus::instance().taskLogSignal(
            QStringLiteral("whisper"), line, false, false);
    }
}

std::optional<int> WhisperWorker::parseProgress(const QString& line) {
    // 解析时间戳行计算进度（0-100）
    const auto match = timestampRe().match(line);
    if (match.hasMatch()) {
        const double currentSeconds = timestampToSeconds(match.captured(2));
        if (duration_ > 0 && currentSeconds > 0)
            return std::min(100, int((currentSeconds / duration_) * 100));
    }
    // 检测完成标志
    if (line.contains(QStringLiteral("LoadModel")) &&
        line.contains(QStringLiteral("RunComplete")))
        return 100;
    return std::nullopt;
}

void WhisperWorker::activateOutput() {
    // Whisper CLI 无 -o 参数：输出自动生成在输入文件旁
    // （<输入名>.mp4.<格式> 或 <输入名>.<格式>），重命名为任务输出
    const QString inputDir = pathDirname(task_->inputPath);
    const QString inputStem = QFileInfo(task_->inputPath).completeBaseName();
    const QStringList possibleOutputs{
        QDir(inputDir).filePath(inputStem + QStringLiteral(".mp4.") + task_->format),
        QDir(inputDir).filePath(inputStem + QLatin1Char('.') + task_->format),
    };
    for (const QString& candidate : possibleOutputs) {
        if (!QFileInfo::exists(candidate)) continue;
        if (QFileInfo(candidate).absoluteFilePath() ==
            QFileInfo(task_->outputPath).absoluteFilePath())
            continue;
        // os.replace：目标存在则先移除再改名
        if (QFile::exists(task_->outputPath)) QFile::remove(task_->outputPath);
        if (QFile::rename(candidate, task_->outputPath)) break;
    }
    // 自动复制为 原文.srt（设为当前活动原文）
    activateAsCurrent(task_->outputPath, task_->inputPath);
}

void WhisperWorker::activateAsCurrent(const QString& outputFile,
                                      const QString& inputFile) {
    const QString parentDir = pathDirname(outputFile);
    const QString currentFile =
        QDir(parentDir).filePath(QStringLiteral("原文.srt"));

    // 如果输出文件已经是 原文.srt，则直接返回
    if (QFileInfo(outputFile).absoluteFilePath() ==
        QFileInfo(currentFile).absoluteFilePath())
        return;

    // 只有当视频名为 生肉.mp4 且 原文.srt 不存在时才复制
    if (!inputFile.isEmpty() &&
        pathBasename(inputFile) != QStringLiteral("生肉.mp4"))
        return;
    if (QFileInfo::exists(currentFile)) return;

    const QFileInfo info(outputFile);
    if (info.exists() && info.size() > 0) QFile::copy(outputFile, currentFile);
}

void WhisperWorker::finish(bool success) {
    // 任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）
    if (taskLogger_) taskLogger_->close();
    if (cancelled_.load()) return;
    emit GlobalEventBus::instance().finishTaskSig(task_->taskId, success,
                                                  task_->logPath);
}

}  // namespace fkw
