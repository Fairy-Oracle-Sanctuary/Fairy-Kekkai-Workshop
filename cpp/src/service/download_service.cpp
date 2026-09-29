#include "service/download_service.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <string>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifdef Q_OS_WIN
#include <QSettings>
#endif
#include "common/config.h"
#include "common/logger.h"

namespace fkw {
namespace {
QString decodeProcessOutput(const QByteArray& bytes) {
#ifdef Q_OS_WIN
    const int length = MultiByteToWideChar(CP_ACP, 0, bytes.constData(),
                                           int(bytes.size()), nullptr, 0);
    if (length > 0) {
        std::wstring wide(size_t(length), L'\0');
        if (MultiByteToWideChar(CP_ACP, 0, bytes.constData(), int(bytes.size()),
                                wide.data(), length) > 0)
            return QString::fromWCharArray(wide.data(), length);
    }
#endif
    return QString::fromUtf8(bytes);
}
QJsonValue option(const char* group, const char* key) {
    return AppConfig::instance().value(QLatin1String(group), QLatin1String(key));
}
bool enabled(const char* key) { return option("YTDLP", key).toBool(); }
void flag(QStringList& args, const char* key, const QString& value) {
    if (enabled(key)) args << value;
}
QString powerShellQuote(QString value) {
    return QStringLiteral("'") + value.replace(QLatin1Char('\''), QStringLiteral("''"))
        + QStringLiteral("'");
}
QString systemProxyUrl() {
#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),
        QSettings::NativeFormat);
    if (!settings.value(QStringLiteral("ProxyEnable")).toBool()) return {};
    QString raw = settings.value(QStringLiteral("ProxyServer")).toString().trimmed();
    if (raw.contains(QLatin1Char(';'))) {
        QString http;
        QString https;
        for (const QString& part : raw.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            if (part.startsWith(QStringLiteral("https="), Qt::CaseInsensitive))
                https = part.mid(6);
            else if (part.startsWith(QStringLiteral("http="), Qt::CaseInsensitive))
                http = part.mid(5);
        }
        raw = https.isEmpty() ? http : https;
    } else if (raw.startsWith(QStringLiteral("https="), Qt::CaseInsensitive)) {
        raw = raw.mid(6);
    } else if (raw.startsWith(QStringLiteral("http="), Qt::CaseInsensitive)) {
        raw = raw.mid(5);
    }
    if (!raw.contains(QStringLiteral("://"))) raw.prepend(QStringLiteral("http://"));
    const QUrl url(raw);
    return url.isValid() && !url.host().isEmpty() ? raw : QString();
#else
    return {};
#endif
}
}

DownloadProcess::DownloadProcess(const DownloadRequest& request, QObject* parent)
    : QObject(parent), request_(request) {
    QObject::connect(&process_, &QProcess::readyReadStandardOutput, this, [this]() {
        consume(process_.readAllStandardOutput(), stdoutPending_);
    });
    QObject::connect(&process_, &QProcess::readyReadStandardError, this, [this]() {
        consume(process_.readAllStandardError(), stderrPending_);
    });
    QObject::connect(&process_, &QProcess::started, this, [this]() {
        Logger::get(QStringLiteral("DownloadProcess"), QStringLiteral("download"))->info(
            QStringLiteral("yt-dlp 已启动，PID：%1").arg(process_.processId()));
    });
    QObject::connect(&process_, &QProcess::finished, this,
                     [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Logger::get(QStringLiteral("DownloadProcess"), QStringLiteral("download"))->info(
            QStringLiteral("yt-dlp 已退出，退出码：%1，状态：%2")
                .arg(exitCode).arg(int(exitStatus)));
        consume(process_.readAllStandardOutput(), stdoutPending_);
        consume(process_.readAllStandardError(), stderrPending_);
        if (!stdoutPending_.isEmpty()) parseLine(decodeProcessOutput(stdoutPending_));
        if (!stderrPending_.isEmpty()) parseLine(decodeProcessOutput(stderrPending_));
        stdoutPending_.clear();
        stderrPending_.clear();
        if (cancelling_) { complete(false, QStringLiteral("下载已取消")); return; }
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            if (request_.thumbnailOnly && !QFileInfo::exists(
                    QDir(request_.directory).filePath(QStringLiteral("封面.jpg")))) {
                complete(false, QStringLiteral("yt-dlp 未生成封面.jpg"));
                return;
            }
            complete(true, QStringLiteral("下载完成"));
            return;
        }
        complete(false, QStringLiteral("yt-dlp 退出码：%1\n%2")
                 .arg(exitCode).arg(lastLines_.join(QLatin1Char('\n'))));
    });
    QObject::connect(&process_, &QProcess::errorOccurred, this,
                     [this](QProcess::ProcessError error) {
        if (error == QProcess::UnknownError) return;
        const QString message = process_.errorString();
        Logger::get(QStringLiteral("DownloadProcess"), QStringLiteral("download"))->error(
            QStringLiteral("yt-dlp 进程错误：%1").arg(message));
        complete(false, message);
        if (process_.state() != QProcess::NotRunning) process_.kill();
    });
}
DownloadProcess::~DownloadProcess() {
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(1000);
    }
}
QString DownloadProcess::executablePath() {
    return option("Download", "YTDLPPath").toString();
}
QString DownloadProcess::configuredProxyUrl() {
    return enabled("SystemProxy")
        ? systemProxyUrl() : option("YTDLP", "ProxyUrl").toString();
}
bool DownloadProcess::isRunning() const { return process_.state() != QProcess::NotRunning; }
QStringList DownloadProcess::arguments() const {
    QStringList args;
    const QString proxy = configuredProxyUrl();
    if (!proxy.isEmpty()) args << QStringLiteral("--proxy") << proxy;
    args << QStringLiteral("--print")
         << QStringLiteral("before_dl:FKW_TITLE:%(title)s")
         << QStringLiteral("--no-quiet") << QStringLiteral("--no-simulate");
    if (request_.thumbnailOnly) {
        args << QStringLiteral("--skip-download")
             << QStringLiteral("--write-thumbnail")
             << QStringLiteral("--convert-thumbnails") << QStringLiteral("jpg")
             << QStringLiteral("-o")
             << QDir(request_.directory).filePath(QStringLiteral("封面.%(ext)s"));
        const QString ffmpeg = option("FFmpeg", "FFmpegPath").toString();
        if (!ffmpeg.isEmpty()) args << QStringLiteral("--ffmpeg-location") << ffmpeg;
        args << request_.url;
        return args;
    }
    const QString name = request_.fileName.isEmpty()
        ? option("YTDLP", "OutputTemplate").toString()
        : request_.fileName + QStringLiteral(".%(ext)s");
    args << QStringLiteral("-o") << QDir(request_.directory).filePath(name);
    args << QStringLiteral("-f")
         << QStringLiteral("bestvideo[ext=mp4]+bestaudio[ext=m4a]/bestvideo+bestaudio/best");
    const QString quality = option("YTDLP", "DownloadQuality").toString();
    if (!quality.isEmpty() && quality != QStringLiteral("best") &&
        quality != QStringLiteral("worst"))
        args << QStringLiteral("-S") << QStringLiteral("res:") + quality;
    if (enabled("DownloadSubtitles")) {
        args << QStringLiteral("--write-sub");
        const QString languages = option("YTDLP", "SubtitleLanguages").toString();
        if (!languages.isEmpty()) args << QStringLiteral("--sub-langs") << languages;
    }
    flag(args, "EmbedSubtitles", QStringLiteral("--embed-subs"));
    flag(args, "DownloadThumbnail", QStringLiteral("--write-thumbnail"));
    flag(args, "EmbedThumbnail", QStringLiteral("--embed-thumbnail"));
    flag(args, "DownloadMetadata", QStringLiteral("--write-info-json"));
    flag(args, "WriteDescription", QStringLiteral("--write-description"));
    flag(args, "WriteAnnotations", QStringLiteral("--write-annotations"));
    args << QStringLiteral("-N") << QString::number(option("YTDLP", "ConcurrentDownloads").toInt());
    args << QStringLiteral("-R") << QString::number(option("YTDLP", "RetryAttempts").toInt());
    args << QStringLiteral("--socket-timeout")
         << QString::number(option("YTDLP", "DownloadTimeout").toInt());
    if (enabled("LimitDownloadRate")) {
        const QString maxRate = option("YTDLP", "MaxDownloadRate").toString();
        if (!maxRate.isEmpty()) args << QStringLiteral("--limit-rate") << maxRate;
    }
    flag(args, "SkipExistingFiles", QStringLiteral("--no-overwrites"));
    if (enabled("UseCookies")) {
        const QString cookies = option("YTDLP", "CookiesFile").toString();
        if (!cookies.isEmpty()) args << QStringLiteral("--cookies") << cookies;
    }
    const QString ffmpeg = option("FFmpeg", "FFmpegPath").toString();
    if (!ffmpeg.isEmpty()) args << QStringLiteral("--ffmpeg-location") << ffmpeg;
    args << QStringLiteral("--newline") << QStringLiteral("--no-part")
         << QStringLiteral("--no-mtime") << QStringLiteral("--ignore-errors")
         << request_.url;
    return args;
}
void DownloadProcess::start() {
    if (completed_ || isRunning()) return;
    const QString executable = executablePath();
    const QStringList args = arguments();
    QStringList command{QStringLiteral("&"), powerShellQuote(executable)};
    for (const QString& arg : args) command << powerShellQuote(arg);
    Logger::get(QStringLiteral("DownloadProcess"), QStringLiteral("download"))->info(
        QStringLiteral("yt-dlp 命令（PowerShell）：%1").arg(command.join(QLatin1Char(' '))));
    if (!QFileInfo::exists(executable)) {
        complete(false, QStringLiteral("yt-dlp 不存在：%1").arg(executable));
        return;
    }
    if (!QDir().mkpath(request_.directory)) {
        complete(false, QStringLiteral("无法创建下载目录：%1").arg(request_.directory));
        return;
    }
    process_.setProgram(executable);
    process_.setArguments(args);
    process_.start();
    if (request_.thumbnailOnly) QTimer::singleShot(120000, this, [this]() {
        if (completed_) return;
        complete(false, QStringLiteral("封面下载超时"));
        if (isRunning()) process_.kill();
    });
}
void DownloadProcess::cancel() {
    if (completed_ || cancelling_) return;
    cancelling_ = true;
    if (!isRunning()) { complete(false, QStringLiteral("下载已取消")); return; }
    process_.terminate();
    QTimer::singleShot(5000, this, [this]() {
        if (!completed_ && isRunning()) process_.kill();
    });
}
void DownloadProcess::consume(const QByteArray& bytes, QByteArray& pending) {
    pending += bytes;
    int newline = 0;
    while ((newline = pending.indexOf('\n')) >= 0) {
        const QByteArray line = pending.left(newline).trimmed();
        pending.remove(0, newline + 1);
        if (!line.isEmpty()) parseLine(decodeProcessOutput(line));
    }
}
void DownloadProcess::parseLine(const QString& line) {
    if (line.isEmpty() || completed_) return;
    if (line.startsWith(QStringLiteral("FKW_TITLE:"))) {
        const QString title = line.mid(10).trimmed();
        if (!title.isEmpty()) emit titleResolved(title);
        return;
    }
    lastLines_ << line;
    if (!line.startsWith(QStringLiteral("[download] ")) ||
        line.contains(QStringLiteral("Destination:"))) {
        emit outputLine(line);
        auto logger = Logger::get(QStringLiteral("DownloadProcess"), QStringLiteral("download"));
        if (line.startsWith(QStringLiteral("ERROR:"))) logger->error(line);
        else logger->info(line);
    }
    while (lastLines_.size() > 8) lastLines_.removeFirst();
    if (cancelling_) return;
    static const QRegularExpression destination(
        QStringLiteral(R"(^\[download\] Destination: (.+)$)"));
    static const QRegularExpression merged(
        QStringLiteral(R"regex(^\[Merger\] Merging formats into "(.+)"$)regex"));
    auto match = destination.match(line);
    if (!match.hasMatch()) match = merged.match(line);
    if (match.hasMatch()) {
        filename_ = QFileInfo(match.captured(1)).fileName();
        if (line.startsWith(QStringLiteral("[download] Destination:"))) {
            static const QRegularExpression formatSuffix(QStringLiteral(R"(\.f\d+(?=\.[^.]+$))"));
            filename_.remove(formatSuffix);
        }
        if (line.startsWith(QStringLiteral("[Merger]")))
            emit status(QStringLiteral("正在合并音视频"));
        else emit status(QStringLiteral("准备下载文件"));
    } else if (line.startsWith(QStringLiteral("[youtube]")) ||
               line.startsWith(QStringLiteral("[generic]")) ||
               line.startsWith(QStringLiteral("[info]"))) {
        emit status(QStringLiteral("正在解析视频信息"));
    } else if (line.startsWith(QStringLiteral("[download]"))) {
        emit status(QStringLiteral("正在下载文件"));
    } else if (line.startsWith(QStringLiteral("[ThumbnailConvertor]")) ||
               line.startsWith(QStringLiteral("[ExtractAudio]")) ||
               line.startsWith(QStringLiteral("[VideoRemuxer]"))) {
        emit status(QStringLiteral("正在处理文件"));
    }
    static const QRegularExpression percentage(
        QStringLiteral(R"(\[download\]\s+(\d+(?:\.\d+)?)%)"));
    static const QRegularExpression speedPattern(
        QStringLiteral(R"(\bat\s+([\d.]+\s*[KMGT]?i?B/s))"));
    const auto percent = percentage.match(line);
    if (percent.hasMatch()) {
        const auto speed = speedPattern.match(line);
        emit progress(qBound(0, int(percent.captured(1).toDouble()), 100),
                      speed.hasMatch() ? speed.captured(1) : QString(), filename_);
    } else if (match.hasMatch())
        emit progress(line.startsWith(QStringLiteral("[Merger]")) ? 100 : 0,
                      QString(), filename_);
}
void DownloadProcess::complete(bool success, const QString& message) {
    if (completed_) return;
    completed_ = true;
    emit finished(success, cancelling_, message);
}
} // namespace fkw
