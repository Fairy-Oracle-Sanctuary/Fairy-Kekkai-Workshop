#include "view/translate_task_interface.h"

#include <QFile>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/setting.h"
#include "common/text.h"
#include "service/translate_service.h"

namespace fkw {
TranslateTaskInterface::TranslateTaskInterface(QWidget* parent)
    : BaseTaskInterface(PreviewTaskKind::Translate, nullptr, parent) {
    setObjectName(QStringLiteral("translateTaskInterface"));
}

std::shared_ptr<TaskBase> TranslateTaskInterface::createTask(const QString& input,
                                                             const QString& output) {
    // 对齐 Python TranslationInterface._get_args：读取 SRT 原文并快照当前配置。
    // 读取失败时返回 nullptr，任务不会被添加。
    QFile file(input);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return nullptr;
    auto task = std::make_shared<TranslateTask>(input, output);
    task->rawContent = QString::fromUtf8(file.readAll());

    const auto& cfg = AppConfig::instance();
    task->originLang =
        translateLanguageLabel(cfg.value(ConfigKeys::origin_lang).toString());
    task->targetLang =
        translateLanguageLabel(cfg.value(ConfigKeys::target_lang).toString());
    task->ai = cfg.value(ConfigKeys::ai_model).toString();
    // aiTemperature 在 config 中为字符串（对齐 Python ConfigItem 默认 "0.7"）
    const QString temperature = cfg.value(ConfigKeys::aiTemperature).toString();
    task->temperature = temperature.isEmpty() ? 0.7 : temperature.toDouble();
    if (task->ai == QStringLiteral("deepseek")) {
        const QString model = cfg.value(ConfigKeys::deepseekModel).toString();
        if (!model.isEmpty()) task->deepseekModel = model;
        task->deepseekReasoning =
            cfg.value(ConfigKeys::deepseekReasoning).toBool();
    }
    // 任务卡片图标与 Python 一致：:/app/images/icons/<AI>.svg
    task->iconName = translateAiModelIcon(task->ai);
    return task;
}

TaskWorker* TranslateTaskInterface::createWorker(const std::shared_ptr<TaskBase>& task) {
    return new TranslateWorker(std::static_pointer_cast<TranslateTask>(task));
}

QString TranslateTaskInterface::taskTypeText() const {
    return Text::instance().Translate;
}

QStringList TranslateTaskInterface::taskGeneratedFiles(
    const std::shared_ptr<TaskBase>& task) const {
    return task->outputPath.isEmpty() ? QStringList{} : QStringList{task->outputPath};
}

void TranslateTaskInterface::emitLegacyFinished(bool success, TaskCard* card) {
    // 兼容旧托盘通知通道（Python _emitLegacyFinished）
    emit GlobalEventBus::instance().translate_finished_signal(
        success, QStringList{success ? card->task()->outputPath : QString()});
}
}