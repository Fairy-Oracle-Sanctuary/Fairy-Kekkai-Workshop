#include "service/translate_service.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <atomic>
#include <functional>

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/setting.h"
#include "common/task_status.h"
#include "common/text.h"
#include "common/text_format.h"

namespace fkw {
namespace {

constexpr int kBatchSize = 50;    // 对齐 Python TranslateWorker.BATCH_SIZE
constexpr int kMaxRetries = 2;    // 对齐 Python TranslateWorker.MAX_RETRIES
constexpr int kStreamTimeoutMs = 120000;

QString stringValue(ConfigKeys::Key key) {
    return AppConfig::instance().value(key).toString();
}

// 去掉尾部的 '/'（对齐 Python url.rstrip("/")）
QString rstripSlash(QString url) {
    while (url.endsWith(QLatin1Char('/'))) url.chop(1);
    return url;
}

// 对齐 Python CustomModelService._normalize_base_url：无 path 时补 /v1。
QString normalizeBaseUrl(const QString& url) {
    const QUrl parsed(url.trimmed());
    if (parsed.path().isEmpty()) return url.trimmed() + QStringLiteral("/v1");
    return rstripSlash(url.trimmed());
}

// ---------- 流式 chat/completions ----------
struct StreamOutcome {
    bool ok = false;
    QString error;
};

// 在调用线程内同步执行流式请求（QEventLoop 阻塞，规避 Worker 线程内信号投递问题）。
// 每收到一段 content 调用 onChunk；返回 ok=false 时 error 为服务商/网络错误串。
StreamOutcome postStreamingChat(const TranslateProvider& provider,
                                const QJsonArray& messages, double temperature,
                                std::atomic<bool>* cancelled,
                                const std::function<void(const QString&)>& onChunk) {
    StreamOutcome outcome;
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(provider.baseUrl + QStringLiteral("/chat/completions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + provider.apiKey.toUtf8());
    request.setTransferTimeout(kStreamTimeoutMs);

    QJsonObject payload{
        {QStringLiteral("model"), provider.model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("stream"), true},
        {QStringLiteral("temperature"), temperature},
    };
    if (provider.reasoning) {
        payload.insert(QStringLiteral("reasoning_effort"), QStringLiteral("high"));
        payload.insert(QStringLiteral("extra_body"),
                       QJsonObject{{QStringLiteral("thinking"),
                                    QJsonObject{{QStringLiteral("type"),
                                                 QStringLiteral("enabled")}}}});
    }

    QNetworkReply* reply =
        manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QString lineBuffer;
    QString rawBody;
    QString streamError;
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setInterval(100);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]() {
        if (cancelled && cancelled->load()) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
        const QString chunk = QString::fromUtf8(reply->readAll());
        rawBody += chunk;
        lineBuffer += chunk;
        int newline = -1;
        while ((newline = lineBuffer.indexOf(QLatin1Char('\n'))) >= 0) {
            const QString line = lineBuffer.left(newline).trimmed();
            lineBuffer.remove(0, newline + 1);
            if (!line.startsWith(QLatin1String("data:"))) continue;
            const QString data = line.mid(5).trimmed();
            if (data.isEmpty() || data == QLatin1String("[DONE]")) continue;
            const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
            if (!doc.isObject()) continue;
            const QJsonObject object = doc.object();
            if (object.contains(QStringLiteral("error")))
                streamError = QString::fromUtf8(
                    QJsonDocument(object.value(QStringLiteral("error")).toObject())
                        .toJson(QJsonDocument::Compact));
            const QJsonArray choices = object.value(QStringLiteral("choices")).toArray();
            if (choices.isEmpty()) continue;
            const QString piece = choices.first().toObject()
                                      .value(QStringLiteral("delta"))
                                      .toObject()
                                      .value(QStringLiteral("content"))
                                      .toString();
            if (!piece.isEmpty() && onChunk) onChunk(piece);
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    watchdog.start();
    loop.exec();
    watchdog.stop();

    if (cancelled && cancelled->load()) {
        outcome.error = QStringLiteral("cancelled");
        return outcome;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    if (networkError != QNetworkReply::NoError || status >= 400) {
        outcome.error = !streamError.isEmpty() ? streamError
                        : !rawBody.isEmpty()   ? rawBody
                                               : reply->errorString();
        if (outcome.error.isEmpty()) outcome.error = reply->errorString();
        return outcome;
    }
    outcome.ok = true;
    return outcome;
}

// ---------- 响应解析（对齐 Python 多策略解析） ----------
QString jsonValueToText(const QJsonValue& value) {
    if (value.isString()) return value.toString();
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (number == static_cast<double>(static_cast<qint64>(number)))
            return QString::number(static_cast<qint64>(number));
        return QString::number(number);
    }
    if (value.isBool()) return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    if (value.isNull()) return QStringLiteral("None");
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

// 对齐 Python _extract_translations_from_json：dict 取常见键，list 直接返回。
QStringList translationsFromDocument(const QJsonDocument& doc) {
    QStringList result;
    if (doc.isArray()) {
        for (const QJsonValue& value : doc.array()) result << jsonValueToText(value);
        return result;
    }
    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        for (const QString& key : {QStringLiteral("translations"), QStringLiteral("result"),
                                   QStringLiteral("data"), QStringLiteral("items")}) {
            if (object.contains(key) && object.value(key).isArray()) {
                for (const QJsonValue& value : object.value(key).toArray())
                    result << jsonValueToText(value);
                return result;
            }
        }
    }
    return {};
}

QStringList parseJsonResponse(const QString& response, int expectedCount) {
    const auto tryDocument = [expectedCount](const QString& text) -> QStringList {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) return {};
        const QStringList result = translationsFromDocument(doc);
        return result.size() == expectedCount ? result : QStringList{};
    };

    // 尝试1: 直接解析整个响应
    QStringList result = tryDocument(response);
    if (!result.isEmpty()) return result;

    // 尝试2: 代码块 ```json ... ```
    static const QRegularExpression codeBlock(
        QStringLiteral("```(?:json)?\\s*(.*?)\\s*```"),
        QRegularExpression::DotMatchesEverythingOption);
    const auto block = codeBlock.match(response);
    if (block.hasMatch()) {
        result = tryDocument(block.captured(1));
        if (!result.isEmpty()) return result;
    }

    // 尝试3: 第一个 { 到最后一个 }
    const int braceStart = response.indexOf(QLatin1Char('{'));
    const int braceEnd = response.lastIndexOf(QLatin1Char('}'));
    if (braceStart != -1 && braceEnd > braceStart) {
        result = tryDocument(response.mid(braceStart, braceEnd - braceStart + 1));
        if (!result.isEmpty()) return result;
    }

    // 尝试4: JSON 数组 [ ... ]
    const int bracketStart = response.indexOf(QLatin1Char('['));
    const int bracketEnd = response.lastIndexOf(QLatin1Char(']'));
    if (bracketStart != -1 && bracketEnd > bracketStart) {
        result = tryDocument(response.mid(bracketStart, bracketEnd - bracketStart + 1));
        if (!result.isEmpty()) return result;
    }
    return {};
}

QStringList parseXmlResponse(const QString& response, int expectedCount) {
    static const QRegularExpression tag(QStringLiteral("<t>(.*?)</t>"),
                                        QRegularExpression::DotMatchesEverythingOption);
    QStringList result;
    auto matches = tag.globalMatch(response);
    while (matches.hasNext()) result << matches.next().captured(1).trimmed();
    return result.size() == expectedCount ? result : QStringList{};
}

QStringList parseNumberedResponse(const QString& response, int expectedCount) {
    static const QRegularExpression numbered(QStringLiteral("^\\d+\\.\\s*(.+)$"));
    QStringList result;
    const QStringList lines = response.trimmed().split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        const auto match = numbered.match(line);
        if (match.hasMatch()) result << match.captured(1);
    }
    return result.size() == expectedCount ? result : QStringList{};
}

// 多策略解析，空列表表示解析失败（对齐 Python _parse_translation_response）
QStringList parseTranslationResponse(const QString& response, int expectedCount) {
    const QString cleaned = removeThinkingContent(response);
    QStringList result = parseJsonResponse(cleaned, expectedCount);
    if (!result.isEmpty()) return result;
    result = parseXmlResponse(cleaned, expectedCount);
    if (!result.isEmpty()) return result;
    result = parseNumberedResponse(cleaned, expectedCount);
    if (!result.isEmpty()) return result;
    return {};
}

// 对齐 Python _sanitize_text：清洗翻译文本，防止破坏 SRT 结构。
QString sanitizeText(const QString& text) {
    if (text.isEmpty()) return text;
    QString result = text;
    result.remove(QLatin1Char('\r'));
    result.replace(QStringLiteral("\n\n"), QStringLiteral("\n"));
    return result.trimmed();
}

}  // namespace

QList<SrtItem> parseSrt(const QString& content) {
    QList<SrtItem> items;
    static const QRegularExpression separator(QStringLiteral("\\n\\s*\\n"));
    const QStringList blocks =
        content.trimmed().split(separator, Qt::SkipEmptyParts);
    for (const QString& rawBlock : blocks) {
        const QStringList lines = rawBlock.trimmed().split(QLatin1Char('\n'));
        if (lines.size() < 3) continue;
        SrtItem item;
        item.index = lines.at(0).trimmed();
        item.time = lines.at(1).trimmed();
        item.text = lines.mid(2).join(QLatin1Char('\n')).trimmed();
        items.append(item);
    }
    return items;
}

QString assembleSrt(const QList<SrtItem>& items) {
    QStringList blocks;
    blocks.reserve(items.size());
    for (const SrtItem& item : items)
        blocks << item.index + QLatin1Char('\n') + item.time + QLatin1Char('\n') + item.text;
    return blocks.join(QStringLiteral("\n\n"));
}

QString removeThinkingContent(const QString& text) {
    static const QRegularExpression thinking(
        QStringLiteral("<thinking>.*?</thinking>"),
        QRegularExpression::DotMatchesEverythingOption);
    QString result = text;
    result.remove(thinking);
    return result.trimmed();
}

bool resolveProvider(const QString& ai, QString* error, TranslateProvider* provider) {
    const auto& cfg = AppConfig::instance();
    TranslateProvider result;
    if (ai == QStringLiteral("deepseek")) {
        result.baseUrl = QStringLiteral("https://api.deepseek.com");
        result.apiKey = cfg.value(ConfigKeys::deepseekApiKey).toString();
    } else if (ai == QStringLiteral("glm-4.5-flash")) {
        result.baseUrl = QStringLiteral("https://open.bigmodel.cn/api/paas/v4/");
        result.apiKey = cfg.value(ConfigKeys::glmApiKey).toString();
        result.model = QStringLiteral("glm-4.5-flash");
    } else if (ai == QStringLiteral("spark-lite")) {
        result.baseUrl = QStringLiteral("https://spark-api-open.xf-yun.com/v1");
        result.apiKey = cfg.value(ConfigKeys::sparkApiKey).toString();
        result.model = QStringLiteral("generalv3.5");
    } else if (ai == QStringLiteral("hunyuan-turbos-latest")) {
        result.baseUrl = QStringLiteral("https://api.hunyuan.cloud.tencent.com/v1");
        result.apiKey = cfg.value(ConfigKeys::hunyuanApiKey).toString();
        result.model = QStringLiteral("hunyuan-turbos-latest");
    } else if (ai == QStringLiteral("intern-latest")) {
        result.baseUrl = QStringLiteral("https://chat.intern-ai.org.cn/api/v1");
        result.apiKey = cfg.value(ConfigKeys::internApiKey).toString();
        result.model = QStringLiteral("intern-latest");
    } else if (ai == QStringLiteral("ernie-speed-128k")) {
        result.baseUrl = QStringLiteral("https://qianfan.baidubce.com/v2/");
        result.apiKey = cfg.value(ConfigKeys::ernieSpeedApiKey).toString();
        result.model = QStringLiteral("ernie-speed-128k");
    } else if (ai == QStringLiteral("gemini-3.5-flash")) {
        result.baseUrl =
            QStringLiteral("https://generativelanguage.googleapis.com/v1beta/openai/");
        result.apiKey = cfg.value(ConfigKeys::geminiApiKey).toString();
        result.model = QStringLiteral("gemini-3.5-flash");
    } else if (ai == QStringLiteral("custom-model")) {
        if (!cfg.value(ConfigKeys::customModelEnabled).toBool()) {
            if (error) *error = QStringLiteral("自定义模型未启用");
            return false;
        }
        result.apiKey = cfg.value(ConfigKeys::customModelApiKey).toString();
        if (result.apiKey.isEmpty()) {
            if (error) *error = QStringLiteral("请填写自定义模型的API密钥");
            return false;
        }
        const QString baseUrl = cfg.value(ConfigKeys::customModelBaseUrl).toString();
        if (baseUrl.isEmpty()) {
            if (error) *error = QStringLiteral("请填写自定义模型的API基础URL");
            return false;
        }
        result.baseUrl = normalizeBaseUrl(baseUrl);
        const QString modelName = cfg.value(ConfigKeys::customModelName).toString();
        if (modelName.isEmpty()) {
            if (error) *error = QStringLiteral("请填写自定义模型名称");
            return false;
        }
        const QString endpoint = cfg.value(ConfigKeys::customModelEndpoint).toString();
        result.model = endpoint.isEmpty() ? modelName : endpoint;
    } else {
        if (error) *error = QStringLiteral("不支持的AI模型: ") + ai;
        return false;
    }
    result.baseUrl = rstripSlash(result.baseUrl);
    *provider = result;
    return true;
}

TranslateTask::TranslateTask(const QString& srtPath, const QString& outputPath)
    : TaskBase(srtPath, outputPath) {}

TranslateWorker::TranslateWorker(std::shared_ptr<TranslateTask> task)
    : TaskWorker(nullptr), task_(std::move(task)) {}

TranslateWorker::~TranslateWorker() = default;

void TranslateWorker::cancel() { cancelled_.store(true); }

void TranslateWorker::run() {
    const QString currentTime =
        task_->createTime.toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
    taskLogger_ = Logger::get(
        QStringLiteral("Tasks/") + currentTime + QStringLiteral("_taskID-") +
            QString::number(task_->taskId),
        QStringLiteral("translate"));
    task_->logPath = taskLogger_->logFilePath();

    const bool success = translate();

    if (taskLogger_) taskLogger_->close();
    // 取消的任务不 emit 完成信号（状态已由任务界面设为 Cancelled）
    if (!cancelled_.load())
        emit GlobalEventBus::instance().finishTaskSig(task_->taskId, success,
                                                      task_->logPath);
    // QObject 归属主线程，投递到主线程事件循环回收
    deleteLater();
}

bool TranslateWorker::translate() {
    const auto& t = Text::instance();

    TranslateProvider provider;
    QString providerError;
    if (!resolveProvider(task_->ai, &providerError, &provider)) {
        // 未知模型回退到通用文案，已知模型的配置缺失直接给出具体提示
        const bool unknownModel =
            providerError.startsWith(QStringLiteral("不支持的AI模型"));
        taskLogger_->error(unknownModel ? formatText(t.TextAuto059, {task_->ai})
                                        : providerError);
        return false;
    }
    // Deepseek 的 model 来自任务快照（对齐 Python DeepseekService.get_model_name
    // 优先取 task.deepseek_model），其余服务商在 resolveProvider 内已固定。
    if (task_->ai == QStringLiteral("deepseek") && !task_->deepseekModel.isEmpty())
        provider.model = task_->deepseekModel;
    provider.reasoning = task_->deepseekReasoning;

    emit GlobalEventBus::instance().updateTaskStatusSig(
        task_->taskId, 0, QVariant(int(TaskStatus::Processing)), QString(), 0.0,
        QString(), 0.0);

    // 解析 SRT 文件
    QList<SrtItem> items = parseSrt(task_->rawContent);

    // 构建系统提示（content 占位防止用户自定义模板含 {content} 时报错）
    const QString systemPrompt = formatText(
        stringValue(ConfigKeys::promptTemplate), {},
        {{QStringLiteral("origin_lang"), task_->originLang},
         {QStringLiteral("target_lang"), task_->targetLang},
         {QStringLiteral("content"), QString()}});
    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), systemPrompt}});

    const bool useContext =
        AppConfig::instance().value(ConfigKeys::useTranslateContext).toBool();
    const int totalBatches = (items.size() + kBatchSize - 1) / kBatchSize;

    for (int offset = 0, batchIdx = 0; offset < items.size();
         offset += kBatchSize, ++batchIdx) {
        if (cancelled_.load()) return false;

        const QList<SrtItem> batch = items.mid(offset, kBatchSize);
        QStringList batchTexts;
        batchTexts.reserve(batch.size());
        for (int j = 0; j < batch.size(); ++j) {
            QString text = batch.at(j).text;
            text.replace(QLatin1Char('\n'), QLatin1Char(' '));
            batchTexts << QString::number(j + 1) + QStringLiteral(". ") + text;
        }
        // JSON 格式要求已在 promptTemplate 中配置
        const QString userContent =
            QStringLiteral("请翻译以下%1句文本：\n").arg(batch.size()) +
            batchTexts.join(QLatin1Char('\n'));

        if (useContext) {
            // 限制上下文：只保留最近两轮历史对话（1 轮为 1 对 user+assistant，共 4 条消息）
            if (messages.size() > 5) {
                QJsonArray trimmed;
                trimmed.append(messages.at(0));
                const int keep = qMin(4, messages.size() - 1);
                for (int i = messages.size() - keep; i < messages.size(); ++i)
                    trimmed.append(messages.at(i));
                messages = trimmed;
            }
            messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                        {QStringLiteral("content"), userContent}});
        } else {
            messages = QJsonArray{
                messages.at(0),
                QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                            {QStringLiteral("content"), userContent}}};
        }

        // 调用翻译（带重试机制，解析失败自动重试）
        QStringList translated;
        QString fullResponse;
        for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
            if (cancelled_.load()) return false;

            fullResponse.clear();
            const StreamOutcome outcome = postStreamingChat(
                provider, messages, task_->temperature, &cancelled_,
                [&fullResponse](const QString& piece) { fullResponse += piece; });
            if (!outcome.ok) {
                if (cancelled_.load()) return false;
                const QString message = analysisAiError(outcome.error);
                taskLogger_->error(QStringLiteral("翻译任务失败: ") + task_->inputPath +
                                   QStringLiteral(" - ") + message);
                return false;
            }

            translated = parseTranslationResponse(fullResponse, batch.size());
            // 严格校验：数量必须完全匹配
            if (translated.size() == batch.size()) break;

            const int actual = translated.size();
            if (attempt < kMaxRetries) {
                const QString warning =
                    QStringLiteral("批次 %1/%2 第 %3 次解析失败 (期望 %4 条，实际 %5 "
                                   "条)，重试中...")
                        .arg(batchIdx + 1)
                        .arg(totalBatches)
                        .arg(attempt + 1)
                        .arg(batch.size())
                        .arg(actual);
                taskLogger_->warning(warning);
                emit GlobalEventBus::instance().taskLogSignal(
                    QStringLiteral("translate"), warning, false, false);
            } else {
                const QString message =
                    QStringLiteral("批次 %1/%2 重试 %3 次仍失败 (期望 %4 条，实际 %5 "
                                   "条)，该批保留原文")
                        .arg(batchIdx + 1)
                        .arg(totalBatches)
                        .arg(kMaxRetries)
                        .arg(batch.size())
                        .arg(actual);
                taskLogger_->error(message);
                emit GlobalEventBus::instance().taskLogSignal(
                    QStringLiteral("translate"), message, true, false);
            }
        }

        // 添加助手回复到对话历史（维持上下文连贯，无论成功与否）
        if (!fullResponse.isEmpty())
            messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                        {QStringLiteral("content"), fullResponse}});

        // 更新 SRT 项目（仅在解析成功时）
        if (translated.size() == batch.size()) {
            for (int j = 0; j < batch.size(); ++j)
                items[offset + j].text = sanitizeText(translated.at(j));
        }

        // 更新进度
        const int progress =
            qMin(100, int((offset + kBatchSize) / double(items.size()) * 100));
        emit GlobalEventBus::instance().updateTaskStatusSig(
            task_->taskId, progress, QVariant(int(TaskStatus::Processing)), QString(),
            0.0, QString(), 0.0);
        emit GlobalEventBus::instance().taskLogSignal(
            QStringLiteral("translate"),
            formatText(t.TextAuto058, {QString::number(progress)}), false, true);
    }

    if (cancelled_.load()) return false;

    // 组装最终的 SRT 并写入文件
    QFile output(task_->outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        taskLogger_->error(QStringLiteral("翻译任务失败: ") + task_->inputPath +
                           QStringLiteral(" - ") + analysisAiError(output.errorString()));
        return false;
    }
    output.write(assembleSrt(items).toUtf8());
    output.close();

    // 翻译完成后进行后处理：去除思考内容
    if (!postProcess()) return false;

    taskLogger_->info(QStringLiteral("翻译任务已完成: ") + task_->inputPath);
    return true;
}

bool TranslateWorker::postProcess() {
    QFile file(task_->outputPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString message = formatText(Text::instance().TextAuto061, {file.errorString()});
        taskLogger_->error(QStringLiteral("翻译后处理失败: ") + task_->inputPath +
                           QStringLiteral(" - ") + message);
        return false;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QString cleaned = removeThinkingContent(content);
    if (cleaned == content) {
        taskLogger_->info(QStringLiteral("未检测到思考内容，文件保持不变: ") +
                          task_->outputPath);
        return true;
    }
    QFile output(task_->outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString message =
            formatText(Text::instance().TextAuto061, {output.errorString()});
        taskLogger_->error(QStringLiteral("翻译后处理失败: ") + task_->inputPath +
                           QStringLiteral(" - ") + message);
        return false;
    }
    output.write(cleaned.toUtf8());
    output.close();
    taskLogger_->info(QStringLiteral("已去除思考内容，文件已更新: ") + task_->outputPath);
    return true;
}

// ---------- 屏幕翻译（对应 Python ScreenTranslateThread） ----------
ScreenTranslateRunner::ScreenTranslateRunner(const QString& text, QObject* parent)
    : QObject(parent), text_(text) {
    // 与 ScreenOcrRunner 一致：交给调用方的 QThreadPool 调度，run() 末尾自回收
    setAutoDelete(false);
}

ScreenTranslateRunner::~ScreenTranslateRunner() = default;

void ScreenTranslateRunner::cancel() { cancelled_.store(true); }

void ScreenTranslateRunner::run() {
    const auto& cfg = AppConfig::instance();
    const QString ai = cfg.value(ConfigKeys::ai_model).toString();
    const QString originLang = cfg.value(ConfigKeys::origin_lang).toString();
    const QString targetLang = cfg.value(ConfigKeys::target_lang).toString();
    const double temperature = cfg.value(ConfigKeys::aiTemperature).toDouble();

    TranslateProvider provider;
    QString providerError;
    if (!resolveProvider(ai, &providerError, &provider)) {
        emit GlobalEventBus::instance().screen_translate_finished(false, providerError);
        deleteLater();
        return;
    }
    // Deepseek 的 model 与深度思考开关来自配置（对齐 Python DeepseekService.get_model_name）
    if (ai == QStringLiteral("deepseek")) {
        const QString deepseekModel = cfg.value(ConfigKeys::deepseekModel).toString();
        if (!deepseekModel.isEmpty()) provider.model = deepseekModel;
        provider.reasoning = cfg.value(ConfigKeys::deepseekReasoning).toBool();
    }

    // prompt 与 Python ScreenTranslateThread 逐字对齐（单轮 user 消息，无系统提示）
    const QString prompt =
        QStringLiteral("你是一个专业的%1翻译助手。\n请将以下%2文本翻译为%3，"
                       "保持原意流畅自然，直接输出翻译结果：\n\n%4")
            .arg(targetLang)
            .arg(originLang)
            .arg(targetLang)
            .arg(text_);
    const QJsonArray messages{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                          {QStringLiteral("content"), prompt}}};

    QString fullResponse;
    const StreamOutcome outcome = postStreamingChat(
        provider, messages, temperature, &cancelled_,
        [&fullResponse](const QString& piece) { fullResponse += piece; });

    if (cancelled_.load()) {
        emit GlobalEventBus::instance().screen_translate_finished(false,
                                                                  QStringLiteral("已取消"));
    } else if (!outcome.ok) {
        emit GlobalEventBus::instance().screen_translate_finished(
            false, analysisAiError(outcome.error));
    } else {
        emit GlobalEventBus::instance().screen_translate_finished(
            true, removeThinkingContent(fullResponse));
    }
    deleteLater();
}

}  // namespace fkw