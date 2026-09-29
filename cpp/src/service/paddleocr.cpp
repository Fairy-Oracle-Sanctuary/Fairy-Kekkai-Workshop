#include "service/paddleocr.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <stdexcept>

#include "common/setting.h"

namespace fkw {
PaddleOcrModelDirs resolveModelDirs(const QString& language, bool useServerModel,
                                    const QString& supportFilesPath) {
    static const QJsonObject languages = [] {
        QFile file(QStringLiteral(":/app/paddleocr_langs.json"));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    const QString base = supportFilesPath.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(paddleOcrSupportFilesName())
        : supportFilesPath;
    const QString v6Mode = useServerModel ? QStringLiteral("medium")
                                          : QStringLiteral("small");
    const QString legacyMode = useServerModel ? QStringLiteral("server")
                                              : QStringLiteral("mobile");
    const bool isV6Supported =
        QStringList{QStringLiteral("ch"), QStringLiteral("chinese_cht"),
                    QStringLiteral("en"), QStringLiteral("japan")}.contains(language)
        || (languages.value(QStringLiteral("latin")).toArray().contains(language)
            && language != QStringLiteral("pi"));
    QString detection;
    QString recognition;
    if (isV6Supported) {
        detection = QStringLiteral("PP-OCRv6_") + v6Mode + QStringLiteral("_det");
        recognition = QStringLiteral("PP-OCRv6_") + v6Mode + QStringLiteral("_rec");
    } else {
        detection = language == QStringLiteral("ka")
            ? QStringLiteral("PP-OCRv3_mobile_det")
            : QStringLiteral("PP-OCRv5_") + legacyMode + QStringLiteral("_det");
        for (const QString& group : {QStringLiteral("latin"), QStringLiteral("arabic"),
                                     QStringLiteral("eslav"), QStringLiteral("cyrillic"),
                                     QStringLiteral("devanagari")}) {
            if (languages.value(group).toArray().contains(language)) {
                recognition = group + QStringLiteral("_PP-OCRv5_mobile_rec");
                break;
            }
        }
        if (recognition.isEmpty() &&
            QStringList{QStringLiteral("korean"), QStringLiteral("th"),
                        QStringLiteral("el"), QStringLiteral("te"),
                        QStringLiteral("ta")}.contains(language))
            recognition = language + QStringLiteral("_PP-OCRv5_mobile_rec");
        if (language == QStringLiteral("ka"))
            recognition = QStringLiteral("ka_PP-OCRv3_mobile_rec");
    }
    if (recognition.isEmpty())
        throw std::invalid_argument("Unsupported PaddleOCR language");
    const QDir directory(base);
    return {directory.filePath(QStringLiteral("det/") + detection),
            directory.filePath(QStringLiteral("rec/") + recognition),
            directory.filePath(QStringLiteral("cls/PP-LCNet_x1_0_textline_ori"))};
}
}  // namespace fkw
