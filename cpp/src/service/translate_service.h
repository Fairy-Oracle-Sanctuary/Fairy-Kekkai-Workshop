#pragma once

#include <QList>
#include <QRunnable>
#include <QSharedPointer>
#include <QString>
#include <atomic>
#include <memory>

#include "common/logger.h"
#include "service/task_base.h"

namespace fkw {

// ---------- SRT 解析 / 组装（对齐 Python translate_service.parse_srt / assemble_srt） ----------
struct SrtItem {
    QString index;
    QString time;
    QString text;
};

QList<SrtItem> parseSrt(const QString& content);
QString assembleSrt(const QList<SrtItem>& items);
// 去除 <think>...</think> 思考内容（对齐 Python remove_thinking_content）。
QString removeThinkingContent(const QString& text);

// ---------- AI 服务商解析（对齐 Python SERVICES 及各 *Service.get_client/get_model_name） ----------
struct TranslateProvider {
    QString baseUrl;  // OpenAI 兼容 base_url（不含 /chat/completions）
    QString apiKey;
    QString model;    // 请求体中的 model 字段
    // Deepseek 深度思考：额外附带 reasoning_effort / thinking 参数
    bool reasoning = false;
};

// 依据 config 解析所选模型的服务商参数。失败时返回 false 并写入 error（中文提示）。
bool resolveProvider(const QString& ai, QString* error, TranslateProvider* provider);

// ---------- 翻译任务 ----------
// 对应 Python translate_service.TranslateTask：公共字段（taskId/fileName/outputName/
// logPath/createTime/iconName）在 TaskBase 中，这里只补充翻译专有参数。
struct TranslateTask : TaskBase {
    TranslateTask(const QString& srtPath, const QString& outputPath);

    QString originLang;  // 已本地化的语言名（直接写入 prompt，对齐 Python）
    QString targetLang;
    QString rawContent;  // SRT 原始内容
    QString ai;          // 对应 Python task.AI（模型标识）
    double temperature = 0.7;
    QString deepseekModel = QStringLiteral("deepseek-v4-flash");
    bool deepseekReasoning = false;
};

// 翻译执行引擎（对齐 Python TranslateWorker：分批 + 多策略解析 + 重试 + 去思考内容后处理）。
// 由 QThreadPool 调度并在 run() 结束后 deleteLater() 回收。
class TranslateWorker : public TaskWorker {
    Q_OBJECT
public:
    explicit TranslateWorker(std::shared_ptr<TranslateTask> task);
    ~TranslateWorker() override;

    void run() override;

    // 取消翻译：标记取消并中断当前流式请求。取消的任务不 emit finishTaskSig
    // （状态已由任务界面设为 Cancelled）。
    void cancel() override;

private:
    // 执行翻译主流程，返回是否成功（取消返回 false）。
    bool translate();
    // 翻译后处理：去除思考内容。
    bool postProcess();

    std::shared_ptr<TranslateTask> task_;
    QSharedPointer<Logger> taskLogger_;
};

// 屏幕翻译一次性任务（对应 Python translate_service.ScreenTranslateThread）：把悬浮窗
// 当前的 OCR 文本整体交给所选 AI 模型流式翻译，结果经 event_bus 上报。由 QThreadPool
// 调度，run() 结束自动 deleteLater()（外部持有引用请用 QPointer）。
class ScreenTranslateRunner : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit ScreenTranslateRunner(const QString& text, QObject* parent = nullptr);
    ~ScreenTranslateRunner() override;

    void run() override;
    void cancel();

private:
    QString text_;
    std::atomic<bool> cancelled_{false};
};

}  // namespace fkw