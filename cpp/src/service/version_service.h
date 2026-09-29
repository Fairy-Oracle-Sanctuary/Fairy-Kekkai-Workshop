#pragma once
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPair>
#include <QString>

namespace fkw {
class VersionService : public QObject {
    Q_OBJECT
public:
    explicit VersionService(QObject* parent = nullptr);
    void check();
    QString latestVersion() const { return latestVersion_; }
    QString changelog() const;
    QString defaultDownloadUrl() const;
signals:
    void checked(bool hasNewVersion, const QString& error);
private:
    QNetworkAccessManager network_;
    QJsonObject release_;
    QString currentVersion_;
    QString latestVersion_;
    QString atomChangelog_;
    QList<QPair<QString, QString>> atomDownloads_;
    bool atomOcrUpdate_ = false;
    bool checking_ = false;
    void checkAtom(const QString& apiError);
    QList<QPair<QString, QString>> downloadLinks() const;
};
} // namespace fkw
