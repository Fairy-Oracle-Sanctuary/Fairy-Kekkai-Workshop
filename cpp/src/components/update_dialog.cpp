#include "components/update_dialog.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include "common/text.h"
#include "common/text_format.h"
#include "common/utils.h"
#include "service/version_service.h"

namespace fkw {
UpdateDialog::UpdateDialog(VersionService* version, QWidget* parent)
    : qfw::MessageBoxBase(parent), version_(version),
      changelog_(new qfw::PlainTextEdit(this)),
      progress_(new qfw::ProgressBar(this)) {
    viewLayout->addWidget(new qfw::SubtitleLabel(
        formatText(Text::instance().UpdateAvailable, {version_->latestVersion()}), this));
    changelog_->setReadOnly(true);
    changelog_->setPlainText(version_->changelog().isEmpty()
        ? Text::instance().NoChangelog : version_->changelog());
    viewLayout->addWidget(changelog_);
    progress_->setVisible(false);
    viewLayout->addWidget(progress_);
    yesButton->setText(Text::instance().DownloadInstaller);
    cancelButton->setText(Text::instance().Close);
    widget->setMinimumSize(500, 350);
}
bool UpdateDialog::validate() {
    if (downloading_) return false;
    if (downloaded_) {
        showInFolder(filePath_);
        return true;
    }
    startDownload();
    return false;
}

void UpdateDialog::reject() {
    if (!downloading_) qfw::MessageBoxBase::reject();
}

void UpdateDialog::startDownload() {
    const QUrl url(version_->defaultDownloadUrl());
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        changelog_->appendPlainText(formatText(Text::instance().DownloadFailed3,
                                                {QStringLiteral("Invalid download URL")}));
        return;
    }
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty() || !QDir(directory).exists())
        directory = QDir::tempPath();
    directory = QDir(directory).filePath(QStringLiteral("Fairy-Kekkai-Workshop"));
    const QString filename = QFileInfo(url.fileName()).fileName();
    filePath_ = QDir(directory).filePath(filename.isEmpty()
        ? QStringLiteral("Fairy-Kekkai-Workshop-Setup.exe") : filename);
    if (!QDir().mkpath(directory)) {
        changelog_->appendPlainText(formatText(Text::instance().DownloadFailed3,
                                                {directory}));
        return;
    }
    file_.setFileName(filePath_);
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        changelog_->appendPlainText(formatText(Text::instance().DownloadFailed3,
                                                {file_.errorString()}));
        return;
    }
    downloading_ = true;
    yesButton->setEnabled(false);
    yesButton->setText(Text::instance().Downloading3);
    cancelButton->setEnabled(false);
    progress_->setValue(0);
    progress_->setVisible(true);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply_ = network_.get(request);
    connect(reply_, &QNetworkReply::readyRead, this, [this] {
        if (!reply_) return;
        const QByteArray chunk = reply_->readAll();
        if (file_.write(chunk) != chunk.size()) reply_->abort();
    });
    connect(reply_, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) progress_->setValue(static_cast<int>(received * 100 / total));
    });
    connect(reply_, &QNetworkReply::finished, this, &UpdateDialog::finishDownload);
}
void UpdateDialog::finishDownload() {
    if (!reply_) return;
    auto* reply = reply_.data();
    const QByteArray remainder = reply->readAll();
    const bool written = file_.write(remainder) == remainder.size();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString error = reply->errorString();
    const bool success = written && file_.error() == QFile::NoError
        && reply->error() == QNetworkReply::NoError && status < 400;
    file_.close();
    reply->deleteLater();
    reply_.clear();
    downloading_ = false;
    yesButton->setEnabled(true);
    cancelButton->setEnabled(true);
    if (!success) {
        QFile::remove(filePath_);
        progress_->setVisible(false);
        yesButton->setText(Text::instance().DownloadInstaller);
        changelog_->appendPlainText(formatText(Text::instance().DownloadFailed3,
                                                {error}));
        return;
    }
    downloaded_ = true;
    progress_->setValue(100);
    yesButton->setText(Text::instance().OpenFolder);
    changelog_->appendPlainText(formatText(Text::instance().DownloadedTo,
                                            {filePath_}));
    showInFolder(filePath_);
}
} // namespace fkw
