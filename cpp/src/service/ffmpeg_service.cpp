#include "service/ffmpeg_service.h"

#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QVariant>
#include <cmath>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/task_status.h"
#include "common/utils.h"

namespace fkw {
namespace {

// 解析 ffmpeg 输出（对齐 Easy-FFmpeg）
const QRegularExpression& durationRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Duration: (\d{2}):(\d{2}):(\d{2}\.\d+))"));
    return re;
}
const QRegularExpression& timeRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(time=(\d{2}):(\d{2}):(\d{2}\.\d+))"));
    return re;
}
const QRegularExpression& sizeRe() {
    static const QRegularExpression re(QStringLiteral(R"(size=\s*(\S+))"));
    return re;
}
const QRegularExpression& bitrateRe() {
    static const QRegularExpression re(QStringLiteral(R"(bitrate=\s*(\S+))"));
    return re;
}
const QRegularExpression& speedRe() {
    static const QRegularExpression re(QStringLiteral(R"(speed=\s*([\d.]+))"));
    return re;
}

QString ffmpegPath() {
    return AppConfig::instance().value(ConfigKeys::ffmpegPath).toString();
}
QString stringValue(ConfigKeys::Key key) {
    return AppConfig::instance().value(key).toString();
}
bool boolValue(ConfigKeys::Key key) {
    return AppConfig::instance().value(key).toBool();
}
int intValue(ConfigKeys::Key key) {
    return AppConfig::instance().value(key).toInt();
}
double doubleValue(ConfigKeys::Key key) {
    return AppConfig::instance().value(key).toDouble();
}

// os.path.dirname/basename 语义见 common/utils.h（与 task_base 共用）
double round2(double value) {
    return std::round(value * 100.0) / 100.0;
}

}  // namespace

FFmpegTask::FFmpegTask(const QString& videoPath, const QString& outputPath)
    : TaskBase(videoPath, outputPath), saveFolder(pathDirname(outputPath)) {}

bool probeHasAudio(const QString& videoPath) {
    QProcess process;
    process.start(ffmpegPath(), {QStringLiteral("-i"), videoPath});
    process.waitForFinished(5000);
    const QString output = QString::fromUtf8(process.readAllStandardError());
    return output.contains(QStringLiteral("Audio:"));
}

QString adjustOutputFormat(const QString& outputFile) {
    const QString outputFormat = stringValue(ConfigKeys::ffmpegOutputFormat);
    if (outputFormat.isEmpty()) return outputFile;
    // os.path.splitext 语义：在最后一个路径组件内取最后一个非前导的点
    const qsizetype sep = lastSeparator(outputFile);
    const qsizetype dot = outputFile.lastIndexOf(QLatin1Char('.'));
    bool hasExtension = false;
    if (dot > sep) {
        for (qsizetype i = sep + 1; i < dot; ++i) {
            if (outputFile.at(i) != QLatin1Char('.')) {
                hasExtension = true;
                break;
            }
        }
    }
    const QString base = hasExtension ? outputFile.left(dot) : outputFile;
    return base + QLatin1Char('.') + outputFormat;
}

QPair<QStringList, QString> buildFfmpegCommand(const QString& videoPath,
                                               const QString& outputFile,
                                               bool hasAudio) {
    QStringList cmd;

    // 硬件加速
    if (boolValue(ConfigKeys::ffmpegUseHardwareAcceleration)) {
        const QString accelerator = stringValue(ConfigKeys::ffmpegHardwareAccelerator);
        if (accelerator != QStringLiteral("auto"))
            cmd << QStringLiteral("-hwaccel") << accelerator;
    }

    // 输入视频
    cmd << QStringLiteral("-i") << videoPath;

    // 视频编码参数
    cmd << QStringLiteral("-c:v") << stringValue(ConfigKeys::ffmpegVideoCodec)
        << QStringLiteral("-crf") << QString::number(intValue(ConfigKeys::ffmpegCrf))
        << QStringLiteral("-preset") << stringValue(ConfigKeys::ffmpegPreset);

    // x264高级参数（如果启用）
    if (boolValue(ConfigKeys::ffmpegUseAdvanced)) {
        const QStringList x264Params{
            QStringLiteral("ref=") + QString::number(intValue(ConfigKeys::ffmpegRefFrames)),
            QStringLiteral("bframes=") + QString::number(intValue(ConfigKeys::ffmpegBFrames)),
            QStringLiteral("keyint=") + QString::number(intValue(ConfigKeys::ffmpegKeyint)),
            QStringLiteral("minkeyint=") + QString::number(intValue(ConfigKeys::ffmpegMinkeyint)),
            QStringLiteral("scenecut=") + QString::number(intValue(ConfigKeys::ffmpegScenecut)),
            QStringLiteral("qcomp=") + QString::number(doubleValue(ConfigKeys::ffmpegQcomp)),
            QStringLiteral("psy-rd=") + stringValue(ConfigKeys::ffmpegPsyRd),
            QStringLiteral("aq-mode=") + QString::number(intValue(ConfigKeys::ffmpegAqMode)),
            QStringLiteral("aq-strength=") + QString::number(doubleValue(ConfigKeys::ffmpegAqStrength)),
        };
        cmd << QStringLiteral("-x264-params") << x264Params.join(QLatin1Char(':'));
    }

    // 音频处理
    const QString audioMode = stringValue(ConfigKeys::ffmpegAudioMode);
    if (audioMode == QStringLiteral("none")) {
        cmd << QStringLiteral("-an");  // 无音频
    } else if (audioMode == QStringLiteral("copy")) {
        cmd << QStringLiteral("-c:a") << QStringLiteral("copy");  // 直接复制
    } else if (audioMode == QStringLiteral("encode") ||
               audioMode == QStringLiteral("auto")) {
        // 依据调用方传入的音频流探测结果决定是否编码音频
        if (hasAudio) {
            cmd << QStringLiteral("-c:a") << stringValue(ConfigKeys::ffmpegAudioCodec)
                << QStringLiteral("-b:a") << stringValue(ConfigKeys::ffmpegAudioBitrate);
        } else {
            cmd << QStringLiteral("-an");
        }
    }

    // 视频缩放
    const QString scaleOption = stringValue(ConfigKeys::ffmpegScale);
    if (scaleOption != QStringLiteral("none")) {
        if (scaleOption == QStringLiteral("custom")) {
            const QString customScale = stringValue(ConfigKeys::ffmpegCustomScale);
            if (!customScale.isEmpty())
                cmd << QStringLiteral("-vf") << QStringLiteral("scale=") + customScale;
        } else {
            static const QHash<QString, QString> resolutionMap{
                {QStringLiteral("720p"), QStringLiteral("1280:720")},
                {QStringLiteral("1080p"), QStringLiteral("1920:1080")},
                {QStringLiteral("1440p"), QStringLiteral("2560:1440")},
                {QStringLiteral("2160p"), QStringLiteral("3840:2160")},
            };
            const QString resolution = resolutionMap.value(scaleOption);
            if (!resolution.isEmpty())
                cmd << QStringLiteral("-vf") << QStringLiteral("scale=") + resolution;
        }
    }

    // 帧率设置
    const QString fpsOption = stringValue(ConfigKeys::ffmpegFps);
    if (fpsOption != QStringLiteral("source"))
        cmd << QStringLiteral("-r") << fpsOption;

    // 视频码率限制
    const QString videoBitrate = stringValue(ConfigKeys::ffmpegVideoBitrate);
    if (!videoBitrate.isEmpty())
        cmd << QStringLiteral("-b:v") << videoBitrate;

    // 输出格式（修正输出文件扩展名）
    const QString output = adjustOutputFormat(outputFile);

    // 覆盖输出文件
    cmd << (boolValue(ConfigKeys::ffmpegOverwriteOutput) ? QStringLiteral("-y")
                                                         : QStringLiteral("-n"));

    // 输出文件
    cmd << output;

    return {cmd, output};
}

FFmpegWorker::FFmpegWorker(std::shared_ptr<FFmpegTask> task)
    : TaskWorker(nullptr), task_(std::move(task)) {
    // autoDelete 关闭与 QObject 归属由 TaskWorker 基类统一处理
}

FFmpegWorker::~FFmpegWorker() {
    // 兜底：正常路径下 run() 结束时进程已回收
    QMutexLocker locker(&processMutex_);
    if (process_) {
        if (process_->state() != QProcess::NotRunning) process_->kill();
        process_->waitForFinished(1000);
        delete process_;
        process_ = nullptr;
    }
}

void FFmpegWorker::run() {
    const QString currentTime =
        task_->createTime.toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
    taskLogger_ = Logger::get(
        QStringLiteral("Tasks/") + currentTime + QStringLiteral("_taskID-") +
            QString::number(task_->taskId),
        QStringLiteral("ffmpeg"));
    task_->logPath = taskLogger_->logFilePath();

    // 在 Worker 线程内探测音频流并构建命令（不阻塞 UI 主线程）
    const bool hasAudio = probeHasAudio(task_->inputPath);
    const QStringList cmd =
        buildFfmpegCommand(task_->inputPath, task_->outputPath, hasAudio).first;
    taskLogger_->info(QStringLiteral("args: ") + ffmpegPath() + QLatin1Char(' ') +
                      cmd.join(QLatin1Char(' ')));

    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, 0, QVariant(int(TaskStatus::Processing)),
        QStringLiteral("0KiB"), 0.0, QStringLiteral("0kbits/s"), 0.0);

    if (!runStage(cmd)) {
        destroyProcess();
        finish(false);
    } else {
        const int exitCode = destroyProcess();
        const bool success = exitCode == 0 && !cancelled_.load();
        finish(success);
    }
    // QObject 归属主线程，投递到主线程事件循环回收
    deleteLater();
}

bool FFmpegWorker::runStage(const QStringList& args) {
    // 执行单阶段 ffmpeg，返回是否成功启动。
    // QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失；
    // process_ 始终指向当前进程，便于取消时 kill。
    duration_ = 0.0;
    lastEmit_ = 0.0;
    stderrBuffer_.clear();
    durationFrozen_ = false;

    {
        QMutexLocker locker(&processMutex_);
        process_ = new QProcess();  // 创建于 Worker 线程，信号在本线程分发
    }
    // DirectConnection 与 Python 的 QRunnable 用法一致：回调在 Worker 线程同步执行
    connect(process_, &QProcess::readyReadStandardError, this,
            &FFmpegWorker::handleStderr, Qt::DirectConnection);
    QEventLoop loop;
    connect(process_, &QProcess::finished, &loop, &QEventLoop::quit,
            Qt::DirectConnection);
    process_->start(ffmpegPath(), args);
    if (!process_->waitForStarted()) return false;
    loop.exec();
    return true;
}

void FFmpegWorker::cancel() {
    cancelled_.store(true);
    QMutexLocker locker(&processMutex_);
    if (process_) process_->kill();
}

int FFmpegWorker::destroyProcess() {
    QMutexLocker locker(&processMutex_);
    if (!process_) return -1;
    const int exitCode = process_->exitCode();
    if (process_->state() != QProcess::NotRunning) process_->kill();
    delete process_;
    process_ = nullptr;
    return exitCode;
}

void FFmpegWorker::tryParseDuration(const QString& data) {
    // 解析视频总时长：多输入时累加所有 Duration 求和，time= 出现后冻结
    double total = 0.0;
    bool matched = false;
    auto it = durationRe().globalMatch(data);
    while (it.hasNext()) {
        const auto match = it.next();
        matched = true;
        total += match.captured(1).toInt() * 3600 + match.captured(2).toInt() * 60 +
                 match.captured(3).toDouble();
    }
    if (matched) duration_ = total;
    if (timeRe().match(data).hasMatch()) durationFrozen_ = true;
}

void FFmpegWorker::parseProgress(const QString& data) {
    // 解析当前压制进度，节流到每秒最多 4 次
    if (duration_ <= 0) return;
    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    if (now - lastEmit_ < 0.25) return;
    const auto match = timeRe().match(data);
    if (!match.hasMatch()) return;

    lastEmit_ = now;
    const double current = round2(match.captured(1).toInt() * 3600 +
                                  match.captured(2).toInt() * 60 +
                                  match.captured(3).toDouble());
    const int progress = qMin(100, int(current / duration_ * 100));

    QString size;
    const auto sizeMatch = sizeRe().match(data);
    if (sizeMatch.hasMatch()) size = sizeMatch.captured(1);

    QString bitrate;
    const auto bitrateMatch = bitrateRe().match(data);
    if (bitrateMatch.hasMatch()) bitrate = bitrateMatch.captured(1);

    double speed = 0.0;
    const auto speedMatch = speedRe().match(data);
    if (speedMatch.hasMatch()) speed = round2(speedMatch.captured(1).toDouble());

    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, progress, QVariant(int(TaskStatus::Processing)), size,
        current, bitrate, speed);
}

void FFmpegWorker::handleStderr() {
    // ffmpeg 全部输出到 stderr
    const QString data = QString::fromUtf8(process_->readAllStandardError());
    // 编码开始前累积 stderr 用于时长解析；time= 出现后停止累积防内存增长
    if (!durationFrozen_) {
        stderrBuffer_ += data;
        tryParseDuration(stderrBuffer_);
    }
    parseProgress(data);
    taskLogger_->info(data);
}

void FFmpegWorker::finish(bool success) {
    // 任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）
    if (taskLogger_) taskLogger_->close();
    if (cancelled_.load()) return;
    emit GlobalEventBus::instance().finishTaskSig(task_->taskId, success,
                                                  task_->logPath);
}

}  // namespace fkw
