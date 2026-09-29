#include "common/config.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QOperatingSystemVersion>
#include <QSaveFile>
#include "common/app_data.h"
#include "common/setting.h"

namespace fkw {
namespace {
QJsonObject schemaFor(const QString& group, const QString& key) {
    static const QJsonObject schema = [] {
        QFile file(QStringLiteral(":/app/config_schema.json"));
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object();
    }();
    return schema.value(group).toObject().value(key).toObject();
}
QJsonValue dynamicDefault(const QString& group, const QString& key) {
    if (group == QStringLiteral("MainWindow")) {
        if (key == QStringLiteral("MicaEnabled")) {
#ifdef Q_OS_WIN
            return QOperatingSystemVersion::current().microVersion() >= 22000;
#else
            return false;
#endif
        }
        if (key == QStringLiteral("Language")) return QStringLiteral("Auto");
        if (key == QStringLiteral("BackgroundPath"))
            return QDir(sourceRoot()).filePath(QStringLiteral("background.jpg"));
    }
    if (group == QStringLiteral("Project") && key == QStringLiteral("LastOpenPath"))
        return QDir::homePath();
    const QString executable = executableSuffix();
    if (group == QStringLiteral("Download") && key == QStringLiteral("YTDLPPath"))
        return QDir(sourceRoot()).filePath(QStringLiteral("tools/yt-dlp") + executable);
    if (group == QStringLiteral("FFmpeg") && key == QStringLiteral("FFmpegPath"))
        return QDir(sourceRoot()).filePath(QStringLiteral("tools/ffmpeg") + executable);
    if (group == QStringLiteral("OCR")) {
        if (key == QStringLiteral("VideocrCliPath"))
            return QDir(sourceRoot()).filePath(QStringLiteral("tools/videocr-cli") + executable);
        if (key == QStringLiteral("PaddleocrPath")) return paddleOcrDefaultPath();
        if (key == QStringLiteral("supportFilesPath"))
            return paddleOcrSupportFilesDefaultPath();
        if (key == QStringLiteral("TempDir"))
            return QDir(sourceRoot()).filePath(QStringLiteral("temp"));
    }
    if (group == QStringLiteral("Whisper")) {
        if (key == QStringLiteral("ModelPath"))
            return QDir(sourceRoot()).filePath(
                QStringLiteral("tools/Whisper.model/ggml-model-whisper-small.bin"));
        if (key == QStringLiteral("CliPath"))
            return QDir(sourceRoot()).filePath(QStringLiteral("tools/whisper/main") + executable);
    }
    if (group == QStringLiteral("Bilibili") && key == QStringLiteral("ApiPath"))
        return QDir(sourceRoot()).filePath(QStringLiteral("tools/upload-video") + executable);
    return {};
}
}
AppConfig& AppConfig::instance() {
    static AppConfig config;
    return config;
}
QJsonObject AppConfig::snapshot() const {
    QFile file(configFile());
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QJsonValue AppConfig::value(const QString& group, const QString& key,
                           const QJsonValue& fallback) const {
    const QJsonValue entry = snapshot().value(group).toObject().value(key);
    if (!entry.isUndefined()) return entry;
    const QJsonObject spec = schemaFor(group, key);
    if (spec.value(QStringLiteral("dynamicDefault")).toBool()) {
        const QJsonValue resolved = dynamicDefault(group, key);
        return resolved.isUndefined() ? fallback : resolved;
    }
    const QJsonValue declared = spec.value(QStringLiteral("default"));
    return declared.isUndefined() ? fallback : declared;
}
bool AppConfig::set(const QString& group, const QString& key,
                    const QJsonValue& value, bool restart) {
    const QString path = configFile();
    QJsonObject root;
    if (QFile::exists(path)) {
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly)) return false;
        QJsonParseError error;
        const QJsonDocument parsed = QJsonDocument::fromJson(input.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !parsed.isObject()) return false;
        root = parsed.object();
    }
    QJsonObject section = root.value(group).toObject();
    const QJsonObject spec = schemaFor(group, key);
    QJsonValue accepted = value;
    if (spec.value(QStringLiteral("boolean")).toBool() && !accepted.isBool())
        return false;
    if (spec.contains(QStringLiteral("range"))) {
        if (!accepted.isDouble()) return false;
        const QJsonArray range = spec.value(QStringLiteral("range")).toArray();
        if (range.size() == 2)
            accepted = qBound(range.at(0).toDouble(), accepted.toDouble(),
                              range.at(1).toDouble());
    }
    if (spec.contains(QStringLiteral("options")) &&
        !spec.value(QStringLiteral("options")).toArray().contains(accepted))
        return false;
    if (section.value(key) == accepted) return true;
    section.insert(key, accepted);
    root.insert(group, section);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) return false;
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (output.write(json) != json.size() || !output.commit()) return false;
    emit valueChanged(group, key, accepted);
    if (restart || spec.value(QStringLiteral("restart")).toBool())
        emit restartRequired();
    return true;
}
}  // namespace fkw
