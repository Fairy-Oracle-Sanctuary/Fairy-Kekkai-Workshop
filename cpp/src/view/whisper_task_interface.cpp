#include "view/whisper_task_interface.h"

#include <QDir>

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
    auto task = std::make_shared<WhisperTask>(input, output);
    const auto& cfg = AppConfig::instance();
    task->modelFile = cfg.value(ConfigKeys::whisperModelPath).toString();
    task->language = cfg.value(ConfigKeys::whisperLanguage).toString();
    task->format = cfg.value(ConfigKeys::whisperOutputFormat).toString();
    // GPU 开关关闭时不传 -gpu（对齐 Python："" 时跳过）
    task->gpu = cfg.value(ConfigKeys::whisperUseGpu).toBool()
                    ? cfg.value(ConfigKeys::whisperGpu).toString()
                    : QString();
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
    if (!task->outputPath.isEmpty()) {
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
