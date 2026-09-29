#pragma once
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <qtfluentwidgets.h>

namespace fkw {
class VersionService;
class UpdateDialog : public qfw::MessageBoxBase {
public:
    explicit UpdateDialog(VersionService* version, QWidget* parent = nullptr);
    bool validate() override;
    void reject() override;
private:
    void startDownload();
    void finishDownload();
    VersionService* version_;
    QNetworkAccessManager network_;
    QPointer<QNetworkReply> reply_;
    QFile file_;
    QString filePath_;
    bool downloading_ = false;
    bool downloaded_ = false;
    qfw::PlainTextEdit* changelog_;
    qfw::ProgressBar* progress_;
};
} // namespace fkw
