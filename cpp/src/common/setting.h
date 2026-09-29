#pragma once
#include <QJsonValue>
#include <QList>
#include <QString>
namespace fkw {
// 下拉选项：value 为写入 config 的原始值，label 为界面显示文案。
struct TranslateOption {
    QString value;
    QString label;
};
QJsonValue settingData(const QString& name);
QString configFolder();
QString configFile();
QString databasePath();
QString coverFolder();
QString executableSuffix();
QString copyleftSymbol();
QString paddleOcrVersion();
bool isPaddleOcrCpu();
// 当前版本 PaddleOCR 可执行文件的默认位置（tools/<版本>/paddleocr[.exe]）；
// 根目录 PADDLEOCR 缺失时退化为 tools/paddleocr[.exe]。
QString paddleOcrDefaultPath();
// 当前版本识别模型 support.files 的目录名，供默认值与启动体检共用。
QString paddleOcrSupportFilesName();
// 当前版本模型目录的默认位置（tools/<supportFilesName>）。
QString paddleOcrSupportFilesDefaultPath();
// 对齐 Python setting.AI_ERROR_MAP：把服务商返回的英文错误串映射为中文提示。
// 按子串匹配，首个命中的键生效。
QString analysisAiError(const QString& error);
// 对齐 Python setting.translate_language_dict / AI_model_dict。
// 语言/模型的显示顺序与 Python 字典保持一致，config 存语言代码与模型标识。
QList<TranslateOption> translateLanguageOptions();
QString translateLanguageLabel(const QString& code);
QList<TranslateOption> translateAiModelOptions();
// 任务卡片图标文件名（对应 :/app/images/icons/<name>），值为空时返回空串。
QString translateAiModelIcon(const QString& value);
}
