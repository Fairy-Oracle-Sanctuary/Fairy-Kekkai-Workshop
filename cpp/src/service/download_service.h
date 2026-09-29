#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace fkw {
struct DownloadRequest {
    QString url;
    QString directory;
    QString fileName;
    bool thumbnailOnly = false;
    QString title;
};

class DownloadProcess : public QObject {
    Q_OBJECT
public:
    explicit DownloadProcess(const DownloadRequest& request, QObject* parent = nullptr);
    ~DownloadProcess() override;
    void start();
    void cancel();
    bool isRunning() const;
    static QString executablePath();
    static QString configuredProxyUrl();
signals:
    void progress(int percent, const QString& speed, const QString& filename);
    void status(const QString& stage);
    void titleResolved(const QString& title);
    void outputLine(const QString& line);
    void finished(bool success, bool cancelled, const QString& message);
private:
    QStringList arguments() const;
    void consume(const QByteArray& bytes, QByteArray& pending);
    void parseLine(const QString& line);
    void complete(bool success, const QString& message);
    DownloadRequest request_;
    QProcess process_;
    QByteArray stdoutPending_;
    QByteArray stderrPending_;
    QStringList lastLines_;
    QString filename_;
    bool cancelling_ = false;
    bool completed_ = false;
};
} // namespace fkw
