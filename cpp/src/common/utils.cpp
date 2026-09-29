#include "common/utils.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

namespace fkw {
qsizetype lastSeparator(const QString& path) {
    return qMax(path.lastIndexOf(QLatin1Char('/')),
                path.lastIndexOf(QLatin1Char('\\')));
}
QString pathDirname(const QString& path) {
    const qsizetype sep = lastSeparator(path);
    return sep < 0 ? QString() : path.left(sep);
}
QString pathBasename(const QString& path) {
    const qsizetype sep = lastSeparator(path);
    return sep < 0 ? path : path.mid(sep + 1);
}
bool openUrl(const QString& url) {
    if (url.isEmpty()) return false;
    if (!url.startsWith(QStringLiteral("http"))) {
        if (!QFileInfo::exists(url)) return false;
        QDesktopServices::openUrl(QUrl::fromLocalFile(url));
    } else {
        QDesktopServices::openUrl(QUrl(url));
    }
    return true;
}
bool showInFolder(const QString& path) {
    if (path.isEmpty() || path.startsWith(QStringLiteral("http"), Qt::CaseInsensitive))
        return false;
    const QFileInfo info(path);
    if (!info.exists()) return false;
#ifdef Q_OS_WIN
    QStringList args;
    if (!info.isDir()) args << QStringLiteral("/select,");
    args << QDir::toNativeSeparators(info.absoluteFilePath());
    QProcess::startDetached(QStringLiteral("explorer"), args);
#elif defined(Q_OS_MACOS)
    const QString script = QStringLiteral("tell application \"Finder\" to reveal POSIX file \"%1\"")
        .arg(info.absoluteFilePath());
    QProcess::execute(QStringLiteral("/usr/bin/osascript"),
                      {QStringLiteral("-e"), script});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir()
        ? info.absoluteFilePath() : info.absolutePath()));
#endif
    return true;
}
}  // namespace fkw
