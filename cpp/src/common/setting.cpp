#include "common/setting.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "common/app_data.h"

namespace fkw {
QJsonValue settingData(const QString& name) {
    static const QJsonObject data = [] {
        QFile file(QStringLiteral(":/app/setting_data.json"));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return data.value(name);
}
QString configFolder() {
    // Keep every config writer on the same canonical user-data directory.
    return QDir(userDataFolder()).absolutePath();
}
QString configFile() {
    return QDir(configFolder()).filePath(QStringLiteral("config.json"));
}
QString databasePath() {
    return QDir(configFolder()).filePath(QStringLiteral("database.db"));
}
QString coverFolder() {
    const QString folder = QDir(configFolder()).filePath(QStringLiteral("Cover"));
    QDir().mkpath(folder);
    return folder;
}
QString executableSuffix() {
#ifdef Q_OS_WIN
    return QStringLiteral(".exe");
#else
    return {};
#endif
}
QString copyleftSymbol() {
#ifdef Q_OS_WIN
    return QStringLiteral("🄯 ");
#else
    return QStringLiteral("©️ ");
#endif
}
QString paddleOcrVersion() {
#ifdef Q_OS_WIN
    QFile file(QDir(sourceRoot()).filePath(QStringLiteral("PADDLEOCR")));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QString line = QString::fromUtf8(file.readLine()).trimmed();
    return line.startsWith(QLatin1Char('#')) ? QString() : line;
#else
    return {};
#endif
}
bool isPaddleOcrCpu() {
    return paddleOcrVersion().contains(QStringLiteral("CPU"), Qt::CaseInsensitive);
}
QString paddleOcrDefaultPath() {
    const QString version = paddleOcrVersion();
    const QString subdir = version.isEmpty() ? QString() : version + QLatin1Char('/');
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/") + subdir
                                       + QStringLiteral("paddleocr") + executableSuffix());
}
QString paddleOcrSupportFilesName() {
    // 与 PaddleOCR-Standalone v3.7.0 的模型代次（PP-OCRv6）对齐
    return QStringLiteral("PaddleOCR.PP-OCRv6.support.files");
}
QString paddleOcrSupportFilesDefaultPath() {
    return QDir(sourceRoot()).filePath(QStringLiteral("tools/") + paddleOcrSupportFilesName());
}
QString analysisAiError(const QString& error) {
    // 顺序与 Python setting.AI_ERROR_MAP 一致：先命中的键先返回，
    // 因此不能用无序容器（QHash）承载。
    struct Entry { const char* key; const char* message; };
    static const Entry entries[] = {
        // --- 身份验证与权限 ---
        {"invalid_api_key", "API密钥无效，请检查设置"},
        {"authentication", "认证失败，请检查API Key是否正确"},
        {"unauthorized", "未授权访问，密钥可能已过期"},
        {"permission", "账号权限不足，请确认模型访问权限"},
        // --- 余额与额度 ---
        {"insufficient_quota", "账户额度不足，请及时充值"},
        {"quota", "额度已耗尽或账号已欠费"},
        {"balance", "账户余额不足"},
        {"credit_limit", "已达到信用额度限制"},
        // --- 请求频率与并发 ---
        {"rate_limit", "请求频率过快（RPM），请稍后重试"},
        {"too_many_requests", "并发请求数过多（TPM），请稍后重试"},
        {"concurrency", "已达到最大并行任务数限制"},
        {"429", "请求过于频繁，触发流量控制"},
        // --- 内容审核 (安全限制) ---
        {"policy", "内容触发安全审核政策，无法处理"},
        {"sensitive", "包含敏感词汇，请求被拦截"},
        {"safety", "内容因安全风险被过滤器拦截"},
        {"filtered", "输出内容因合规性被过滤"},
        // --- 服务器状态 ---
        {"overloaded", "服务器负载过高，请稍后重试"},
        {"busy", "服务器繁忙，请稍后重试"},
        {"internal", "服务器内部错误，请联系厂商支持"},
        {"server", "后端服务异常"},
        {"upstream", "上游服务报错"},
        {"500", "服务器崩溃，请稍后再试"},
        {"503", "服务暂时不可用，可能正在维护"},
        // --- 网络与超时 ---
        {"timeout", "请求超时，网络不稳定或响应过慢"},
        {"connection", "网络连接失败，请检查代理或网络设置"},
        {"connect", "无法连接到 API 服务器"},
        {"proxy", "代理服务器配置错误或连接断开"},
        // --- 参数与模型 ---
        {"invalid_request", "请求参数有误，请检查设置"},
        {"model_not_found", "指定的模型不存在或已被下线"},
        {"context_length", "内容超出模型最大上下文长度限制"},
        {"bad_request", "无效请求，请检查输入格式"},
    };
    const QString lowered = error.toLower();
    for (const Entry& entry : entries) {
        if (lowered.contains(QLatin1String(entry.key)))
            return QString::fromUtf8(entry.message);
    }
    return QStringLiteral("未知错误: ") + lowered;
}
QList<TranslateOption> translateLanguageOptions() {
    // 顺序对齐 Python setting.translate_language_dict（首个为默认选中项前的候选顺序）
    return {
        {QStringLiteral("en"), trText("英语")},
        {QStringLiteral("zh"), trText("中文")},
        {QStringLiteral("ja"), trText("日语")},
        {QStringLiteral("ko"), trText("韩语")},
        {QStringLiteral("fr"), trText("法语")},
        {QStringLiteral("de"), trText("德语")},
        {QStringLiteral("es"), trText("西班牙语")},
        {QStringLiteral("pt"), trText("葡萄牙语")},
        {QStringLiteral("ru"), trText("俄语")},
        {QStringLiteral("ar"), trText("阿拉伯语")},
        {QStringLiteral("it"), trText("意大利语")},
        {QStringLiteral("nl"), trText("荷兰语")},
        {QStringLiteral("hi"), trText("印地语")},
        {QStringLiteral("tr"), trText("土耳其语")},
        {QStringLiteral("vi"), trText("越南语")},
        {QStringLiteral("th"), trText("泰语")},
        {QStringLiteral("id"), trText("印尼语")},
        {QStringLiteral("sv"), trText("瑞典语")},
        {QStringLiteral("pl"), trText("波兰语")},
        {QStringLiteral("el"), trText("希腊语")},
        {QStringLiteral("cs"), trText("捷克语")},
        {QStringLiteral("da"), trText("丹麦语")},
        {QStringLiteral("fi"), trText("芬兰语")},
        {QStringLiteral("no"), trText("挪威语")},
        {QStringLiteral("hu"), trText("匈牙利语")},
        {QStringLiteral("ro"), trText("罗马尼亚语")},
        {QStringLiteral("uk"), trText("乌克兰语")},
        {QStringLiteral("fa"), trText("波斯语")},
        {QStringLiteral("he"), trText("希伯来语")},
    };
}
QString translateLanguageLabel(const QString& code) {
    for (const TranslateOption& option : translateLanguageOptions()) {
        if (option.value == code) return option.label;
    }
    // 对齐 Python translate_language_dict.get(x, x)：未命中时原样返回
    return code;
}
QList<TranslateOption> translateAiModelOptions() {
    return {
        {QStringLiteral("hunyuan-turbos-latest"), trText("腾讯混元")},
        {QStringLiteral("deepseek"), QStringLiteral("Deepseek")},
        {QStringLiteral("gemini-3.5-flash"), trText("Gemini 3.5 Flash")},
        {QStringLiteral("intern-latest"), trText("书生")},
        {QStringLiteral("glm-4.5-flash"), QStringLiteral("GLM-4.5-FLASH")},
        {QStringLiteral("spark-lite"), QStringLiteral("Spark-Lite")},
        {QStringLiteral("ernie-speed-128k"), trText("百度ERNIE-Speed-128K")},
        {QStringLiteral("custom-model"), trText("自定义模型")},
    };
}
QString translateAiModelIcon(const QString& value) {
    // 返回模型标识本身（不带 .svg）：TaskCard 按约定拼
    // ":/app/images/icons/<iconName>.svg"（对齐 Python 直接用 task.AI）
    return value;
}
}
