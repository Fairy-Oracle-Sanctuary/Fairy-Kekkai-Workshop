#include "view/ffmpeg_task_interface.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"

namespace fkw {
FFmpegTaskInterface::FFmpegTaskInterface(QWidget* parent)
    : BaseTaskInterface(PreviewTaskKind::FFmpeg, &ConfigKeys::concurrentEncodes, parent) {
    setObjectName(QStringLiteral("ffmpegTaskInterface"));
}

std::shared_ptr<TaskBase> FFmpegTaskInterface::createTask(const QString& input,
                                                          const QString& output) {
    // 修正输出扩展名；音频流探测在 Worker 线程内完成
    return std::make_shared<FFmpegTask>(input, adjustOutputFormat(output));
}

TaskWorker* FFmpegTaskInterface::createWorker(const std::shared_ptr<TaskBase>& task) {
    return new FFmpegWorker(std::static_pointer_cast<FFmpegTask>(task));
}

QString FFmpegTaskInterface::taskTypeText() const { return Text::instance().Encode; }

void FFmpegTaskInterface::emitLegacyFinished(bool success, TaskCard* card) {
    // 兼容旧托盘通知通道
    emit GlobalEventBus::instance().ffmpeg_finished_signal(
        success, success ? card->task()->outputPath : QString());
}
}
