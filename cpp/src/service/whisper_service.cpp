#include "service/whisper_service.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
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
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/whisper/whisper-cli.exe"));
#else
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/whisper/whisper-cli"));
#endif
}

QStringList buildWhisperCommand(const WhisperTask& task) {
    QStringList cmd{QStringLiteral("-f"), task.inputPath,
                    QStringLiteral("-m"), task.modelFile,
                    QStringLiteral("-l"), task.language.isEmpty() ? QStringLiteral("auto") : task.language,
                    QStringLiteral("--print-progress"), QStringLiteral("-of"),
                    QDir(QFileInfo(task.outputPath).absolutePath()).filePath(
                        QFileInfo(task.outputPath).completeBaseName())};
    if (task.format == QStringLiteral("txt")) cmd << QStringLiteral("-otxt");
    else if (task.format == QStringLiteral("vtt")) cmd << QStringLiteral("-ovtt");
    else cmd << QStringLiteral("-osrt");
    // 官方 CLI 默认启用已编译的 GPU 后端，禁用时明确使用 CPU。
    if (!task.useGpu) cmd << QStringLiteral("--no-gpu");
    if (task.resetContext) cmd << QStringLiteral("--max-context") << QStringLiteral("0");
    if (task.useVad) {
        cmd << QStringLiteral("--vad") << QStringLiteral("--vad-model") << task.vadModelFile
            << QStringLiteral("--vad-threshold") << QString::number(task.vadThreshold)
            << QStringLiteral("--vad-min-silence-duration-ms") << QString::number(task.vadMinSilenceMs)
            << QStringLiteral("--vad-max-speech-duration-s") << QString::number(task.vadMaxSpeechSeconds)
            << QStringLiteral("--vad-speech-pad-ms") << QStringLiteral("200")
            << QStringLiteral("--vad-samples-overlap") << QStringLiteral("0.1");
    }
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
    QString error;
    if (!QFileInfo::exists(cliPath)) error = QStringLiteral("whisper-cli 不存在: ") + cliPath;
    else if (task_->modelFile.isEmpty() || !QFileInfo::exists(task_->modelFile))
        error = QStringLiteral("Whisper 模型不存在: ") + task_->modelFile;
    else if (task_->useVad && !QFileInfo::exists(task_->vadModelFile))
        error = QStringLiteral("VAD 模型不存在: ") + task_->vadModelFile;
    else if (!QFileInfo::exists(task_->ffmpegFile))
        error = QStringLiteral("FFmpeg 不存在: ") + task_->ffmpegFile;
    else if (task_->format != QStringLiteral("srt") && task_->format != QStringLiteral("txt") &&
             task_->format != QStringLiteral("vtt"))
        error = QStringLiteral("不支持的 Whisper 输出格式: ") + task_->format;
    if (!error.isEmpty()) {
        taskLogger_->error(error);
        finish(false);
        deleteLater();
        return;
    }
    QTemporaryDir temporary(QDir::tempPath() + QStringLiteral("/fkw-whisper-XXXXXX"));
    if (!temporary.isValid()) {
        taskLogger_->error(QStringLiteral("无法创建 Whisper 临时目录"));
        finish(false);
        deleteLater();
        return;
    }
    duration_ = probeVideoDuration();
    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, 0, QVariant(int(TaskStatus::Processing)),
        QStringLiteral("0KiB"), 0.0, QStringLiteral("0kbits/s"), 0.0);
    const QString audioPath = temporary.filePath(QStringLiteral("audio.wav"));
    bool success = prepareAudio(audioPath);
    if (success && !cancelled_.load()) {
        // 只在任务独占的临时目录生成结果，失败不覆盖用户已有字幕。
        WhisperTask invocation = *task_;
        invocation.inputPath = audioPath;
        invocation.outputPath = temporary.filePath(QStringLiteral("result.") + task_->format);
        const int exitCode = runProcess(cliPath, buildWhisperCommand(invocation));
        success = exitCode == 0 && !cancelled_.load();
        if (success) success = activateOutput(invocation.outputPath);
        else if (!cancelled_.load())
            taskLogger_->error(QStringLiteral("Whisper 转录失败，错误码: %1").arg(exitCode));
    }
    if (success) taskLogger_->info(QStringLiteral("Whisper 转录完成: ") + task_->outputPath);
    finish(success);
    deleteLater();
}

int WhisperWorker::runProcess(const QString& program, const QStringList& arguments) {
    QEventLoop loop;
    {
        QMutexLocker locker(&processMutex_);
        if (cancelled_.load()) return -1;
        process_ = new QProcess();
        process_->setProcessChannelMode(QProcess::MergedChannels);
        connect(process_, &QProcess::readyReadStandardOutput, this,
                &WhisperWorker::handleStdout, Qt::DirectConnection);
        connect(process_, &QProcess::finished, &loop, &QEventLoop::quit, Qt::DirectConnection);
        process_->setWorkingDirectory(QFileInfo(program).absolutePath());
        process_->setProgram(program);
        process_->setArguments(arguments);
        pendingOutput_.clear();
        process_->start();
    }
    taskLogger_->info(QStringLiteral("args: ") + program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
    if (!process_->waitForStarted()) {
        taskLogger_->error(process_->errorString());
        destroyProcess();
        return -1;
    }
    if (process_->state() != QProcess::NotRunning) loop.exec();
    // 处理结尾没有换行的日志；按字节缓存，避免 UTF-8 字符被读块切断。
    pendingOutput_.append(process_->readAllStandardOutput());
    pendingOutput_.append('\n');
    handleStdout();
    return destroyProcess();
}

bool WhisperWorker::prepareAudio(const QString& audioPath) {
    preparingAudio_ = true;
    taskLogger_->info(QStringLiteral("正在转换为 16kHz 单声道 PCM 音频"));
    const int code = runProcess(task_->ffmpegFile,
        {QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"), QStringLiteral("-y"),
         QStringLiteral("-i"), task_->inputPath, QStringLiteral("-map"), QStringLiteral("0:a:0"),
         QStringLiteral("-vn"), QStringLiteral("-ar"), QStringLiteral("16000"),
         QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"),
         audioPath});
    preparingAudio_ = false;
    if (code != 0 && !cancelled_.load()) taskLogger_->error(QStringLiteral("音频转换失败"));
    return code == 0 && !cancelled_.load();
}

void WhisperWorker::cancel() {
    cancelled_.store(true);
    QMutexLocker locker(&processMutex_);
    if (process_) process_->kill();
}

int WhisperWorker::destroyProcess() {
    QMutexLocker locker(&processMutex_);
    if (!process_) return -1;
    const int exitCode = process_->exitStatus() == QProcess::NormalExit ? process_->exitCode() : -1;
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
    pendingOutput_.append(process_->readAllStandardOutput());
    pendingOutput_.replace("\r", "\n");
    int end = -1;
    while ((end = pendingOutput_.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(pendingOutput_.left(end)).trimmed();
        pendingOutput_.remove(0, end + 1);
        if (line.isEmpty()) continue;
        outputLines_.append(line);
        if (outputLines_.size() > 100) outputLines_.removeFirst();
        taskLogger_->info(QStringLiteral("Whisper: ") + line);

        if (cancelled_.load()) continue;

        // 进度行 → 刷新式日志（覆盖上一行）+ 进度上报
        const std::optional<int> progress = preparingAudio_ ? std::nullopt : parseProgress(line);
        if (progress) {
            if (*progress != lastProgress_) {
                lastProgress_ = std::max(lastProgress_, *progress);
                emit GlobalEventBus::instance().updateTaskStatusSig(
                    task_->taskId, lastProgress_, QVariant(int(TaskStatus::Processing)),
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
    static const QRegularExpression progressRe(QStringLiteral(R"(progress\s*=\s*(\d+)%)"));
    const auto reported = progressRe.match(line);
    if (reported.hasMatch()) return std::min(99, reported.captured(1).toInt());
    // 解析时间戳行计算进度（0-100）
    const auto match = timestampRe().match(line);
    if (match.hasMatch()) {
        const double currentSeconds = timestampToSeconds(match.captured(2));
        if (duration_ > 0 && currentSeconds > 0)
            return std::min(99, int((currentSeconds / duration_) * 100));
    }
    return std::nullopt;
}

bool WhisperWorker::activateOutput(const QString& generatedPath) {
    QFile source(generatedPath);
    if (!source.open(QIODevice::ReadOnly)) {
        taskLogger_->error(QStringLiteral("Whisper 未生成结果文件: ") + generatedPath);
        return false;
    }
    if (!QDir().mkpath(QFileInfo(task_->outputPath).absolutePath())) return false;
    QSaveFile destination(task_->outputPath);
    if (!destination.open(QIODevice::WriteOnly)) return false;
    while (!source.atEnd()) {
        if (cancelled_.load()) return false;
        const QByteArray data = source.read(64 * 1024);
        if (source.error() != QFileDevice::NoError || destination.write(data) != data.size()) return false;
    }
    if (cancelled_.load() || !destination.commit()) return false;
    if (task_->format == QStringLiteral("srt")) activateAsCurrent(task_->outputPath, task_->inputPath);
    return true;
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
