#include "service/ocr_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPixmap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScreen>
#include <QThread>
#include <QVariant>
#include <algorithm>
#include <exception>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/task_status.h"
#include "common/utils.h"
#include "service/paddleocr.h"

namespace fkw {
namespace {

// 进度解析正则（对齐 Python 的四个模块级正则）
const QRegularExpression& step1CurrentRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Current:\s+(\d+:\d+:\d+)\s+/\s+(\d+:\d+:\d+))"));
    return re;
}
const QRegularExpression& step2DetectRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Performing Text-Detection on image\s+(\d+)\s+of\s+(\d+))"));
    return re;
}
const QRegularExpression& analyzeFrameRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Analyzing frame\s+(\d+)\s+of\s+(\d+))"));
    return re;
}
const QRegularExpression& step3OcrRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(Performing OCR on image\s+(\d+)\s+of\s+(\d+))"));
    return re;
}
// ppocr INFO: [[[x,y],...], ('text', score)] 行整体（对应 Python re.search）
const QRegularExpression& ppocrInfoRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(ppocr INFO:\s*(\[.*\])\s*$)"));
    return re;
}
// 元组中的单引号文本（含转义），对应 Python parsed[1][0]
const QRegularExpression& ppocrTextRe() {
    static const QRegularExpression re(
        QStringLiteral(R"(\(\s*'((?:[^'\\]|\\.)*)'\s*,)"));
    return re;
}

// 将 HH:MM:SS / MM:SS 时间字符串转换为秒（对齐 Python _time_to_seconds，整数秒）
int timeToSeconds(const QString& timeStr) {
    const QStringList parts = timeStr.trimmed().split(QLatin1Char(':'));
    bool ok = false;
    if (parts.size() == 3) {
        const int hours = parts.at(0).toInt(&ok);
        if (!ok) return 0;
        const int minutes = parts.at(1).toInt(&ok);
        if (!ok) return 0;
        const int seconds = parts.at(2).toInt(&ok);
        if (!ok) return 0;
        return hours * 3600 + minutes * 60 + seconds;
    }
    if (parts.size() == 2) {
        const int minutes = parts.at(0).toInt(&ok);
        if (!ok) return 0;
        const int seconds = parts.at(1).toInt(&ok);
        if (!ok) return 0;
        return minutes * 60 + seconds;
    }
    return 0;
}

QString boolText(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

// 解析 ppocr INFO 行中的文本：对应 Python
// ast.literal_eval(...)[1][0]，即坐标数组之后的 ('text', score) 元组。
QString extractPpocrText(const QString& line) {
    const auto info = ppocrInfoRe().match(line);
    if (!info.hasMatch()) return {};
    const QString payload = info.captured(1);
    const auto text = ppocrTextRe().match(payload);
    if (!text.hasMatch()) return {};

    // 反转义（Python repr 里的 \' 与 \\）
    const QString raw = text.captured(1);
    QString result;
    result.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < raw.size()) {
            result.append(raw.at(++i));
            continue;
        }
        result.append(ch);
    }
    return result.trimmed();
}

// 截取屏幕区域（对齐 Python QApplication.primaryScreen().grabWindow）：
// grabWindow 只在 GUI 线程安全，必要时阻塞切回主线程执行。
QPixmap grabScreenRegion(const QRect& rect) {
    QPixmap pixmap;
    const auto capture = [&rect, &pixmap]() {
        if (QScreen* screen = QGuiApplication::primaryScreen())
            pixmap = screen->grabWindow(0, rect.x(), rect.y(), rect.width(),
                                        rect.height());
    };
    QCoreApplication* app = QCoreApplication::instance();
    if (!app || QThread::currentThread() == app->thread()) {
        capture();
    } else {
        QMetaObject::invokeMethod(app, capture, Qt::BlockingQueuedConnection);
    }
    return pixmap;
}

// 临时目录 RAII 清理（对齐 Python finally 中的 shutil.rmtree）
struct TempDirGuard {
    QString path;
    ~TempDirGuard() {
        if (!path.isEmpty()) QDir(path).removeRecursively();
    }
};

}  // namespace

OcrTask::OcrTask(const QString& videoPath, const QString& outputPath)
    : TaskBase(videoPath, outputPath) {}

QStringList buildOcrCommand(const OcrTask& task) {
    QStringList cmd;

    cmd << QStringLiteral("--video_path") << task.inputPath;
    cmd << QStringLiteral("--output") << task.outputPath;
    cmd << QStringLiteral("--lang") << task.lang;
    cmd << QStringLiteral("--time_start") << task.timeStart;
    if (!task.timeEnd.isEmpty()) cmd << QStringLiteral("--time_end") << task.timeEnd;
    cmd << QStringLiteral("--sim_threshold") << QString::number(task.simThreshold);
    cmd << QStringLiteral("--max_merge_gap") << QString::number(task.maxMergeGapSec);
    cmd << QStringLiteral("--use_fullframe") << boolText(task.useFullframe);
    cmd << QStringLiteral("--use_gpu") << boolText(task.useGpu);
    cmd << QStringLiteral("--use_angle_cls") << boolText(task.useAngleCls);
    cmd << QStringLiteral("--use_server_model") << boolText(task.useServerModel);
    cmd << QStringLiteral("--ssim_threshold") << QString::number(task.ssimThreshold);
    cmd << QStringLiteral("--subtitle_position") << task.subtitlePosition;
    cmd << QStringLiteral("--frames_to_skip") << QString::number(task.framesToSkip);
    cmd << QStringLiteral("--ocr_image_max_width")
        << QString::number(task.ocrImageMaxWidth);
    cmd << QStringLiteral("--post_processing") << boolText(task.postProcessing);
    cmd << QStringLiteral("--min_subtitle_duration")
        << QString::number(task.minSubtitleDurationSec);
    cmd << QStringLiteral("--conf_threshold")
        << QString::number(task.confidenceThreshold);

    // 自定义路径参数（对齐 Python：非空才附加）
    if (!task.paddleocrPath.isEmpty())
        cmd << QStringLiteral("--paddleocr_path") << task.paddleocrPath;
    if (!task.supportFilesPath.isEmpty())
        cmd << QStringLiteral("--supportFilesPath") << task.supportFilesPath;
    if (!task.tempDir.isEmpty()) cmd << QStringLiteral("--tempDir") << task.tempDir;

    // 裁剪区域（无框选时为 0，保持与 Python 参数集合一致）
    const QRect zone1 = task.cropRects.value(0);
    cmd << QStringLiteral("--crop_x") << QString::number(zone1.x());
    cmd << QStringLiteral("--crop_y") << QString::number(zone1.y());
    cmd << QStringLiteral("--crop_width") << QString::number(zone1.width());
    cmd << QStringLiteral("--crop_height") << QString::number(zone1.height());
    if (task.useDualZone && task.cropRects.size() >= 2) {
        const QRect zone2 = task.cropRects.at(1);
        cmd << QStringLiteral("--crop_x2") << QString::number(zone2.x());
        cmd << QStringLiteral("--crop_y2") << QString::number(zone2.y());
        cmd << QStringLiteral("--crop_width2") << QString::number(zone2.width());
        cmd << QStringLiteral("--crop_height2") << QString::number(zone2.height());
    }

    return cmd;
}

OcrWorker::OcrWorker(std::shared_ptr<OcrTask> task)
    : TaskWorker(nullptr), task_(std::move(task)) {}

OcrWorker::~OcrWorker() {
    // 兜底：正常路径下 run() 结束时进程已回收
    QMutexLocker locker(&processMutex_);
    if (process_) {
        if (process_->state() != QProcess::NotRunning) process_->kill();
        process_->waitForFinished(1000);
        delete process_;
        process_ = nullptr;
    }
}

void OcrWorker::run() {
    const QString currentTime =
        task_->createTime.toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
    taskLogger_ = Logger::get(
        QStringLiteral("Tasks/") + currentTime + QStringLiteral("_taskID-") +
            QString::number(task_->taskId),
        QStringLiteral("videocr"));
    task_->logPath = taskLogger_->logFilePath();

    // 清理临时目录
    if (!task_->tempDir.isEmpty() && QDir(task_->tempDir).exists()) {
        if (!QDir(task_->tempDir).removeRecursively())
            taskLogger_->error(QStringLiteral("清理临时目录失败: ") + task_->tempDir);
    }

    // 构建命令并检查可执行文件
    const QString cmdPath =
        AppConfig::instance().value(ConfigKeys::videocrCliPath).toString();
    if (!QFileInfo::exists(cmdPath)) {
        taskLogger_->error(QStringLiteral("videocr-cli.exe 不存在: ") + cmdPath);
        finish(false);
        deleteLater();
        return;
    }

    // 确保输出目录存在
    const QString outputDir = pathDirname(task_->outputPath);
    if (!outputDir.isEmpty()) QDir().mkpath(outputDir);

    const QStringList cmdArgs = buildOcrCommand(*task_);
    taskLogger_->info(QStringLiteral("args: ") + cmdPath + QLatin1Char(' ') +
                      cmdArgs.join(QLatin1Char(' ')));

    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, 0, QVariant(int(TaskStatus::Processing)), QString(), 0.0,
        QString(), 0.0);

    // QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失
    {
        QMutexLocker locker(&processMutex_);
        process_ = new QProcess();  // 创建于 Worker 线程，信号在本线程分发
    }
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyReadStandardOutput, this,
            &OcrWorker::handleStdout, Qt::DirectConnection);
    QEventLoop loop;
    connect(process_, &QProcess::finished, &loop, &QEventLoop::quit,
            Qt::DirectConnection);
    process_->setProgram(cmdPath);
    process_->setArguments(cmdArgs);
    process_->setWorkingDirectory(pathDirname(cmdPath));
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
        // 自动复制到 原文.srt（设为当前活动原文）
        activateAsCurrent(task_->outputPath, task_->inputPath);
        taskLogger_->info(QStringLiteral("OCR处理完成: -%1- 输出: %2")
                              .arg(task_->inputPath, task_->outputPath));
    } else {
        QString errorMsg =
            QStringLiteral("OCR处理失败，错误码: %1").arg(exitCode);
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

void OcrWorker::cancel() {
    // 取消 OCR 处理：标记 + taskkill 强杀进程树（videocr 有子进程）
    cancelled_.store(true);
    // 全程持锁：与 destroyProcess 串行，避免 taskkill 期间进程被回收
    QMutexLocker locker(&processMutex_);
    if (!process_ || process_->state() == QProcess::NotRunning) return;
#ifdef Q_OS_WIN
    QProcess killer;
    killer.setProgram(QStringLiteral("taskkill"));
    killer.setArguments({QStringLiteral("/F"), QStringLiteral("/T"),
                         QStringLiteral("/PID"),
                         QString::number(process_->processId())});
    killer.start();
    if (!killer.waitForFinished(2000)) process_->kill();
#else
    process_->kill();
#endif
    process_->waitForFinished(2000);
}

int OcrWorker::destroyProcess() {
    QMutexLocker locker(&processMutex_);
    if (!process_) return -1;
    const int exitCode = process_->exitCode();
    if (process_->state() != QProcess::NotRunning) process_->kill();
    delete process_;
    process_ = nullptr;
    return exitCode;
}

void OcrWorker::handleStdout() {
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
        taskLogger_->info(line);

        if (cancelled_.load()) continue;
        if (!shouldEmitLine(line)) continue;

        // 进度行 → 刷新式日志（覆盖上一行）+ 进度上报
        const std::optional<int> progress = parseProgress(line);
        if (progress) {
            if (*progress != lastProgress_) {
                lastProgress_ = *progress;
                emit GlobalEventBus::instance().updateTaskStatusSig(
                    task_->taskId, *progress,
                    QVariant(int(TaskStatus::Processing)), QString(), 0.0,
                    QString(), 0.0);
            }
            emit GlobalEventBus::instance().taskLogSignal(
                QStringLiteral("videocr"), line, false, true);
            continue;
        }

        // 错误行 → 红色错误日志
        if (isErrorLine(line)) {
            emit GlobalEventBus::instance().taskLogSignal(
                QStringLiteral("videocr"), line, true, false);
            continue;
        }

        // 普通日志
        emit GlobalEventBus::instance().taskLogSignal(
            QStringLiteral("videocr"), line, false, false);
    }
}

bool OcrWorker::shouldEmitLine(const QString& line) {
    // Step 1/3 相同进度行去重
    if (line.contains(QStringLiteral("Step 1/3")) &&
        line.contains(QStringLiteral("Current:"))) {
        const QString currentPart = line.section(QStringLiteral("Current:"), 1, 1)
                                        .section(QLatin1Char('/'), 0, 0)
                                        .trimmed();
        if (currentPart == lastStep1Progress_) return false;
        lastStep1Progress_ = currentPart;
    }
    return true;
}

bool OcrWorker::isErrorLine(const QString& line) {
    return line.contains(QStringLiteral("找不到PaddleOCR路径")) ||
           line.contains(QStringLiteral("无法找到PaddleOCR可执行文件")) ||
           line.contains(QStringLiteral("Error: PaddleOCR failed"));
}

std::optional<int> OcrWorker::parseProgress(const QString& line) {
    // Step 1/3: Processing video... Current: HH:MM:SS / HH:MM:SS (0-33)
    if (line.contains(QStringLiteral("Step 1/3"))) {
        const auto match = step1CurrentRe().match(line);
        if (match.hasMatch()) {
            const int total = timeToSeconds(match.captured(2));
            if (total > 0) {
                const int current = timeToSeconds(match.captured(1));
                return std::min(int(double(current) / total * 33), 33);
            }
        }
        return std::nullopt;
    }
    // Step 2/3: Text-Detection (33-53)
    if (line.contains(QStringLiteral("Step 2/3")) &&
        line.contains(QStringLiteral("Text-Detection"))) {
        const auto match = step2DetectRe().match(line);
        if (match.hasMatch()) {
            const int total = match.captured(2).toInt();
            if (total > 0) {
                const int current = match.captured(1).toInt();
                return std::min(33 + int(double(current) / total * 20), 53);
            }
        }
        return std::nullopt;
    }
    // Analyzing frame (53-66)
    if (line.contains(QStringLiteral("Analyzing frame"))) {
        const auto match = analyzeFrameRe().match(line);
        if (match.hasMatch()) {
            const int total = match.captured(2).toInt();
            if (total > 0) {
                const int current = match.captured(1).toInt();
                return std::min(53 + int(double(current - 1) / total * 13), 66);
            }
        }
        return std::nullopt;
    }
    // Step 3/3: Performing OCR (66-100)
    if (line.contains(QStringLiteral("Step 3/3")) &&
        line.contains(QStringLiteral("Performing OCR"))) {
        const auto match = step3OcrRe().match(line);
        if (match.hasMatch()) {
            const int total = match.captured(2).toInt();
            if (total > 0) {
                const int current = match.captured(1).toInt();
                return std::min(66 + int(double(current) / total * 34), 100);
            }
        }
    }
    return std::nullopt;
}

void OcrWorker::activateAsCurrent(const QString& outputFile,
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

void OcrWorker::finish(bool success) {
    // 任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）
    if (taskLogger_) taskLogger_->close();
    if (cancelled_.load()) return;
    emit GlobalEventBus::instance().finishTaskSig(task_->taskId, success,
                                                  task_->logPath);
}

ScreenOcrRunner::ScreenOcrRunner(const QRect& rect, QObject* parent)
    : QObject(parent), rect_(rect) {
    // 关闭线程池自动删除，改由 run() 末尾 deleteLater() 投递到主线程回收
    setAutoDelete(false);
}

ScreenOcrRunner::~ScreenOcrRunner() = default;

void ScreenOcrRunner::run() {
    logger_ = Logger::get(QStringLiteral("ScreenOCRThread"),
                          QStringLiteral("screen_ocr"));
    try {
        execute();
    } catch (const std::exception& e) {
        const QString message =
            QStringLiteral("屏幕OCR失败: ") + QString::fromUtf8(e.what());
        if (logger_) logger_->error(message);
        emit GlobalEventBus::instance().screen_ocr_finished(false, message);
    }
    deleteLater();
}

void ScreenOcrRunner::execute() {
    const auto& cfg = AppConfig::instance();
    const QString lang = cfg.value(ConfigKeys::ocr_lang).toString();
    const bool useGpu = cfg.value(ConfigKeys::useGpu).toBool();
    const bool useAngleCls = cfg.value(ConfigKeys::useAngleCls).toBool();
    const bool useServerModel = cfg.value(ConfigKeys::useServerModel).toBool();

    emit GlobalEventBus::instance().screen_ocr_started();
    emit GlobalEventBus::instance().screen_ocr_log(
        QStringLiteral("正在截取屏幕区域..."));

    TempDirGuard temp;

    // 1. 截取屏幕区域
    const QPixmap pixmap = grabScreenRegion(rect_);
    if (pixmap.isNull()) {
        emit GlobalEventBus::instance().screen_ocr_finished(
            false, QStringLiteral("截取屏幕失败"));
        return;
    }

    // 2. 保存为临时图片
    temp.path = QDir(QDir::tempPath())
                    .filePath(QStringLiteral("screen_ocr_") +
                              QString::number(
                                  QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(temp.path);
    const QString imgPath =
        QDir(temp.path).filePath(QStringLiteral("capture.png"));
    pixmap.save(imgPath, "PNG");

    emit GlobalEventBus::instance().screen_ocr_log(QStringLiteral("已保存截图: ") +
                                                   imgPath);

    // 3. 构建 paddleocr 命令
    const QString paddleocrPath =
        cfg.value(ConfigKeys::paddleocrPath).toString();
    if (!QFileInfo::exists(paddleocrPath)) {
        emit GlobalEventBus::instance().screen_ocr_finished(
            false, QStringLiteral("paddleocr 不存在: ") + paddleocrPath);
        return;
    }

    QString supportFilesPath =
        cfg.value(ConfigKeys::supportFilesPath).toString();
    if (!supportFilesPath.isEmpty())
        supportFilesPath = QDir::cleanPath(supportFilesPath);

    // 解析模型目录
    const PaddleOcrModelDirs dirs =
        resolveModelDirs(lang, useServerModel, supportFilesPath);

    QStringList cmdArgs;
    cmdArgs << QStringLiteral("ocr") << QStringLiteral("--input") << temp.path
            << QStringLiteral("--device")
            << (useGpu ? QStringLiteral("gpu") : QStringLiteral("cpu"))
            << QStringLiteral("--use_textline_orientation")
            << boolText(useAngleCls)
            << QStringLiteral("--use_doc_orientation_classify")
            << QStringLiteral("false")
            << QStringLiteral("--use_doc_unwarping") << QStringLiteral("false")
            << QStringLiteral("--lang") << lang
            << QStringLiteral("--text_detection_model_dir") << dirs.detection
            << QStringLiteral("--text_detection_model_name")
            << pathBasename(dirs.detection)
            << QStringLiteral("--text_recognition_model_dir") << dirs.recognition
            << QStringLiteral("--text_recognition_model_name")
            << pathBasename(dirs.recognition);
    if (useAngleCls) {
        cmdArgs << QStringLiteral("--textline_orientation_model_dir")
                << dirs.classification
                << QStringLiteral("--textline_orientation_model_name")
                << pathBasename(dirs.classification);
    }

    emit GlobalEventBus::instance().screen_ocr_log(
        QStringLiteral("启动 PaddleOCR..."));

    // 4. 执行 CLI 进程
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram(paddleocrPath);
    process.setArguments(cmdArgs);
    process.setWorkingDirectory(pathDirname(paddleocrPath));

    // 逐行收集输出（保留跨读取的残行），ppocr INFO 行实时上报日志
    QStringList stdoutLines;
    QString pending;
    const auto consume = [&process, &stdoutLines, &pending]() {
        pending += QString::fromUtf8(process.readAllStandardOutput());
        pending.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        pending.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        int index = -1;
        while ((index = pending.indexOf(QLatin1Char('\n'))) >= 0) {
            const QString line = pending.left(index).trimmed();
            pending.remove(0, index + 1);
            if (line.isEmpty()) continue;
            stdoutLines.append(line);
            if (line.contains(QStringLiteral("ppocr INFO:")))
                emit GlobalEventBus::instance().screen_ocr_log(line);
        }
    };

    process.start();
    if (!process.waitForStarted(5000)) {
        emit GlobalEventBus::instance().screen_ocr_finished(
            false, QStringLiteral("PaddleOCR 启动失败: ") + paddleocrPath);
        return;
    }
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(100);
        consume();
        if (cancelled_.load()) {
            process.kill();
            process.waitForFinished(2000);
            break;
        }
    }
    consume();
    if (!pending.trimmed().isEmpty()) {
        const QString line = pending.trimmed();
        stdoutLines.append(line);
        if (line.contains(QStringLiteral("ppocr INFO:")))
            emit GlobalEventBus::instance().screen_ocr_log(line);
    }

    if (cancelled_.load()) {
        emit GlobalEventBus::instance().screen_ocr_finished(
            false, QStringLiteral("已取消"));
        return;
    }

    const int exitCode = process.exitCode();
    if (process.exitStatus() != QProcess::NormalExit || exitCode != 0) {
        const QString lastStdout =
            stdoutLines.mid(qMax(0, stdoutLines.size() - 10))
                .join(QLatin1Char('\n'));
        const QString diagnostic =
            QStringLiteral(
                "PaddleOCR 失败 (code=%1)\n命令: %2\nstdout(最后10行):\n%3")
                .arg(exitCode)
                .arg(cmdArgs.join(QLatin1Char(' ')), lastStdout);
        if (logger_) logger_->error(diagnostic);
        emit GlobalEventBus::instance().screen_ocr_finished(
            false, QStringLiteral("PaddleOCR 失败 (code=%1): %2")
                       .arg(exitCode)
                       .arg(lastStdout.left(500)));
        return;
    }

    // 5. 解析 ppocr INFO 输出，提取文本
    QStringList texts;
    for (const QString& line : stdoutLines) {
        const QString text = extractPpocrText(line);
        if (!text.isEmpty()) texts.append(text);
    }
    emit GlobalEventBus::instance().screen_ocr_log(
        QStringLiteral("识别完成，共 %1 行文本").arg(texts.size()));
    emit GlobalEventBus::instance().screen_ocr_finished(
        true, texts.join(QLatin1Char('\n')));
}

void ScreenOcrRunner::cancel() { cancelled_.store(true); }

}  // namespace fkw
