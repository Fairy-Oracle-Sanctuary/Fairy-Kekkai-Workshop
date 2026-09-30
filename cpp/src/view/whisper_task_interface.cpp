#include "view/whisper_task_interface.h"

#include <QDir>
#include <QFileInfo>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/utils.h"
#include "service/whisper_service.h"

namespace fkw {
WhisperTaskInterface::WhisperTaskInterface(QWidget* parent)
    : BaseTaskInterface(PreviewTaskKind::Whisper, nullptr, parent) {
    setObjectName(QStringLiteral("whisperTaskInterface"));
}

std::shared_ptr<TaskBase> WhisperTaskInterface::createTask(const QString& input,
                                                           const QString& output) {
    // 对齐 Python WhisperInterface._get_args/addWhisperTaskFromProject：快照当前配置
    const auto& cfg = AppConfig::instance();
    const QString format = cfg.value(ConfigKeys::whisperOutputFormat).toString();
    const QFileInfo outputInfo(output);
    const QString outputPath = QDir(outputInfo.absolutePath()).filePath(
        outputInfo.completeBaseName() + QLatin1Char('.') + format);
    auto task = std::make_shared<WhisperTask>(input, outputPath);
    task->modelFile = cfg.value(ConfigKeys::whisperModelPath).toString();
    task->language = cfg.value(ConfigKeys::whisperLanguage).toString();
    task->format = format;
    task->useGpu = cfg.value(ConfigKeys::whisperUseGpu).toBool();
    task->useVad = cfg.value(ConfigKeys::whisperUseVad).toBool();
    task->vadModelFile = cfg.value(ConfigKeys::whisperVadModelPath).toString();
    task->vadThreshold = cfg.value(ConfigKeys::whisperVadThreshold).toDouble();
    task->vadMinSilenceMs = cfg.value(ConfigKeys::whisperVadMinSilenceMs).toInt();
    task->vadMaxSpeechSeconds = cfg.value(ConfigKeys::whisperVadMaxSpeechSeconds).toInt();
    task->resetContext = cfg.value(ConfigKeys::whisperResetContext).toBool();
    task->ffmpegFile = cfg.value(ConfigKeys::ffmpegPath).toString();
    return task;
}

TaskWorker* WhisperTaskInterface::createWorker(
    const std::shared_ptr<TaskBase>& task) {
    return new WhisperWorker(std::static_pointer_cast<WhisperTask>(task));
}

QString WhisperTaskInterface::taskTypeText() const {
    return Text::instance().Recognize;
}

QStringList WhisperTaskInterface::taskGeneratedFiles(
    const std::shared_ptr<TaskBase>& task) const {
    // 任务生成的文件：Whisper 输出 + 激活为当前原文的 原文.srt
    QStringList files = BaseTaskInterface::taskGeneratedFiles(task);
    if (!task->outputPath.isEmpty() && task->outputPath.endsWith(QStringLiteral(".srt"), Qt::CaseInsensitive)) {
        const QString parentDir = pathDirname(task->outputPath);
        files.append(QDir(parentDir).filePath(QStringLiteral("原文.srt")));
    }
    return files;
}

void WhisperTaskInterface::emitLegacyFinished(bool success, TaskCard* card) {
    // 兼容旧托盘通知通道（Python _emitLegacyFinished）
    emit GlobalEventBus::instance().whisper_finished_signal(
        success, success ? card->task()->outputPath : QString());
}
}
