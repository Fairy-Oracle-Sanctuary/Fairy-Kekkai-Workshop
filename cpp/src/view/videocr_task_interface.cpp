#include "view/videocr_task_interface.h"

#include <QDir>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/utils.h"
#include "service/ocr_service.h"

namespace fkw {
OcrTaskInterface::OcrTaskInterface(QWidget* parent)
    : BaseTaskInterface(PreviewTaskKind::Ocr, nullptr, parent) {
    setObjectName(QStringLiteral("videocrTaskInterface"));
}

void OcrTaskInterface::setCropRects(const QVector<QRect>& rects) {
    cropRects_ = rects;
}

std::shared_ptr<TaskBase> OcrTaskInterface::createTask(const QString& input,
                                                       const QString& output) {
    // 对齐 Python OcrInterface._get_args：任务创建时快照当前 OCR 配置
    auto task = std::make_shared<OcrTask>(input, output);
    const auto& cfg = AppConfig::instance();
    task->lang = cfg.value(ConfigKeys::ocr_lang).toString();
    task->timeStart = cfg.value(ConfigKeys::timeStart).toString();
    task->timeEnd = cfg.value(ConfigKeys::timeEnd).toString();
    task->simThreshold = cfg.value(ConfigKeys::simThreshold).toInt();
    task->maxMergeGapSec = cfg.value(ConfigKeys::maxMergeGap).toDouble();
    task->ssimThreshold = cfg.value(ConfigKeys::ssimThreshold).toDouble();
    task->framesToSkip = cfg.value(ConfigKeys::framesToSkip).toInt();
    task->ocrImageMaxWidth = cfg.value(ConfigKeys::ocrImageMaxWidth).toInt();
    task->minSubtitleDurationSec =
        cfg.value(ConfigKeys::minSubtitleDuration).toDouble();
    task->confidenceThreshold =
        cfg.value(ConfigKeys::confidenceThreshold).toInt();
    task->useGpu = cfg.value(ConfigKeys::useGpu).toBool();
    task->useDualZone = cfg.value(ConfigKeys::useDualZone).toBool();
    task->useAngleCls = cfg.value(ConfigKeys::useAngleCls).toBool();
    task->postProcessing = cfg.value(ConfigKeys::postProcessing).toBool();
    task->useServerModel = cfg.value(ConfigKeys::useServerModel).toBool();
    task->paddleocrPath = cfg.value(ConfigKeys::paddleocrPath).toString();
    task->supportFilesPath = cfg.value(ConfigKeys::supportFilesPath).toString();
    task->tempDir = cfg.value(ConfigKeys::tempDir).toString();
    // Python 侧硬编码项（_get_args：use_fullframe=False / subtitle_position="center"）
    task->useFullframe = false;
    task->subtitlePosition = QStringLiteral("center");
    task->cropRects = cropRects_;
    return task;
}

TaskWorker* OcrTaskInterface::createWorker(
    const std::shared_ptr<TaskBase>& task) {
    return new OcrWorker(std::static_pointer_cast<OcrTask>(task));
}

QString OcrTaskInterface::taskTypeText() const {
    return Text::instance().Extract;
}

QString OcrTaskInterface::logName() const {
    // 实时日志通道名（对齐 OCR Worker 的 videocr 通道）
    return QStringLiteral("videocr");
}

QStringList OcrTaskInterface::taskGeneratedFiles(
    const std::shared_ptr<TaskBase>& task) const {
    // 任务生成的文件：OCR 输出 + 激活为当前原文的 原文.srt
    QStringList files = BaseTaskInterface::taskGeneratedFiles(task);
    if (!task->outputPath.isEmpty()) {
        const QString parentDir = pathDirname(task->outputPath);
        files.append(QDir(parentDir).filePath(QStringLiteral("原文.srt")));
    }
    return files;
}

void OcrTaskInterface::emitLegacyFinished(bool success, TaskCard* card) {
    // 兼容旧功能通知通道（Python _emitLegacyFinished）
    emit GlobalEventBus::instance().ocr_finished_signal(
        success, success ? card->task()->outputPath : QString());
}
}
