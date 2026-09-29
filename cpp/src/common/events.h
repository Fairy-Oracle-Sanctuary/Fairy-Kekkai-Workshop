#pragma once
#include <QJsonObject>
#include <QString>

namespace fkw {
enum class DownloadType { Video, Image, Audio };
enum class NotificationType { Success, Error, Warning, Info };

struct DownloadRequest {
    DownloadType type = DownloadType::Video;
    QString url;
    QString savePath;
    QString quality = QStringLiteral("best");
    QString projectName;
    int episodeNum = 0;
    QJsonObject metadata;
};
struct Notification {
    NotificationType type = NotificationType::Info;
    QString title;
    QString message;
    int duration = 3000;
};
struct EventBuilder {
    static QJsonObject downloadVideo(const QString& url, const QString& savePath,
                                     const QString& quality = QStringLiteral("best"),
                                     const QJsonObject& extras = {}) {
        QJsonObject event{{QStringLiteral("type"), QStringLiteral("video")},
                          {QStringLiteral("url"), url},
                          {QStringLiteral("save_path"), savePath},
                          {QStringLiteral("quality"), quality}};
        for (auto it = extras.begin(); it != extras.end(); ++it)
            event.insert(it.key(), it.value());
        return event;
    }
    static QJsonObject downloadImage(const QString& url, const QString& savePath) {
        return {{QStringLiteral("type"), QStringLiteral("image")},
                {QStringLiteral("url"), url},
                {QStringLiteral("save_path"), savePath}};
    }
    static QJsonObject notificationSuccess(const QString& title, const QString& message) {
        return {{QStringLiteral("type"), QStringLiteral("success")},
                {QStringLiteral("title"), title}, {QStringLiteral("message"), message}};
    }
    static QJsonObject navigationToDownload() {
        return {{QStringLiteral("target"), QStringLiteral("download")},
                {QStringLiteral("data"), QJsonObject{}}};
    }
};
}  // namespace fkw
