#include "service/version_service.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>
#include <QTextDocument>
#include <QXmlStreamReader>
#include "common/setting.h"

namespace fkw {
namespace {
const QUrl releaseApi(QStringLiteral(
    "https://api.github.com/repos/Fairy-Oracle-Sanctuary/"
    "Fairy-Kekkai-Workshop/releases/latest"));
QString releaseBody(const QJsonObject& release) {
    QString body = release.value(QStringLiteral("body")).toString();
    return body.replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
               .replace(QLatin1Char('\r'), QLatin1Char('\n'));
}

// 本地 OCR 标识（PADDLEOCR 文件第一行，形如 PaddleOCR-CPU-v3.7.0）里
// 去掉 OCR 版本号后的「机型 + 算力」维度，返回值是 CPU / GPU / CUDA-11.8 / CUDA-12.9。
// 换引擎时版本号会整体跳变（PaddleOCR-Standalone v1.5.1 -> v3.7.0 -> 下一代），
// 只有这一维度能跨代次稳定匹配。
QString ocrFlavor(const QString& identifier) {
    const QString upper = identifier.toUpper();
    if (upper.contains(QStringLiteral("CPU"))) return QStringLiteral("CPU");
    if (upper.contains(QStringLiteral("CUDA-11.8"))) return QStringLiteral("CUDA-11.8");
    if (upper.contains(QStringLiteral("CUDA-12.9"))) return QStringLiteral("CUDA-12.9");
    if (upper.contains(QStringLiteral("GPU"))) return QStringLiteral("GPU");
    return {};
}

// 安装包名/链接（Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe）
// 是否属于指定机型。按 -CPU- / -CUDA-11.8- 这类完整分段匹配，避免机型互相命中，
// 同时排除 Clear 增量包（它本身不含引擎，不能用来换引擎）。
bool isFlavorInstaller(const QString& labelOrUrl, const QString& flavor) {
    if (flavor.isEmpty()) return false;
    if (labelOrUrl.contains(QStringLiteral("Clear"), Qt::CaseInsensitive)) return false;
    const QString upper = labelOrUrl.toUpper();
    if (flavor == QLatin1String("CPU")) return upper.contains(QStringLiteral("-CPU-"));
    if (!upper.contains(QStringLiteral("-GPU-"))) return false;
    if (flavor == QLatin1String("GPU")) return true;
    return upper.contains(QStringLiteral("-") + flavor.toUpper() + QStringLiteral("-"));
}
}

VersionService::VersionService(QObject* parent) : QObject(parent),
    currentVersion_(settingData(QStringLiteral("VERSION")).toString()) {}

void VersionService::check() {
    if (checking_) return;
    checking_ = true;
    QNetworkRequest request(releaseApi);
    request.setRawHeader("User-Agent", "Fairy-Kekkai-Workshop");
    request.setTransferTimeout(5000);
    auto* reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError && status < 400
            ? QString() : reply->errorString();
        reply->deleteLater();
        if (!error.isEmpty()) {
            checkAtom(error);
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonObject release = document.object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        const QRegularExpression pattern(QStringLiteral("v(\\d+)\\.(\\d+)\\.(\\d+)"));
        const auto match = pattern.match(tag);
        if (release.isEmpty() || !match.hasMatch()) {
            checking_ = false;
            emit checked(false, QStringLiteral("Invalid GitHub release response"));
            return;
        }
        atomChangelog_.clear();
        atomDownloads_.clear();
        atomOcrUpdate_ = false;
        release_ = release;
        latestVersion_ = tag.mid(match.capturedStart() + 1, match.capturedLength() - 1);
        const auto latest = QVersionNumber::fromString(latestVersion_);
        const auto current = QVersionNumber::fromString(currentVersion_);
        checking_ = false;
        emit checked(QVersionNumber::compare(latest, current) > 0, {});
    });
}
void VersionService::checkAtom(const QString& apiError) {
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases.atom")));
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    request.setTransferTimeout(10000);
    auto* reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, apiError] {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QString atomError = reply->errorString();
        const bool networkOk = reply->error() == QNetworkReply::NoError && status < 400;
        reply->deleteLater();
        if (!networkOk) {
            checking_ = false;
            emit checked(false, apiError + QStringLiteral("; ") + atomError);
            return;
        }
        QXmlStreamReader xml(payload);
        QString releaseLink;
        QString html;
        while (!xml.atEnd() && (releaseLink.isEmpty() || html.isEmpty())) {
            xml.readNext();
            if (!xml.isStartElement() || xml.name() != QLatin1String("entry")) continue;
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("link")) {
                    releaseLink = xml.attributes().value(QStringLiteral("href")).toString();
                    xml.skipCurrentElement();
                } else if (xml.name() == QLatin1String("content"))
                    html = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                else xml.skipCurrentElement();
            }
        }
        const QRegularExpression versionPattern(QStringLiteral(
            "/tag/v(\\d+)\\.(\\d+)\\.(\\d+)"));
        const auto match = versionPattern.match(releaseLink);
        if (xml.hasError() || !match.hasMatch() || html.isEmpty()) {
            checking_ = false;
            emit checked(false, apiError);
            return;
        }
        latestVersion_ = match.captured(1) + QLatin1Char('.')
            + match.captured(2) + QLatin1Char('.') + match.captured(3);
        release_ = {};
        atomDownloads_.clear();
        QTextDocument document;
        document.setHtml(html);
        QString plain = document.toPlainText().trimmed();
        atomOcrUpdate_ = plain.endsWith(QStringLiteral("!OCRUPDATE!"));
        if (atomOcrUpdate_) plain.chop(QStringLiteral("!OCRUPDATE!").size());
        const int downloadsAt = plain.indexOf(QStringLiteral("下载提示"));
        atomChangelog_ = (downloadsAt < 0 ? plain : plain.left(downloadsAt)).trimmed();
        if (atomChangelog_.startsWith(QStringLiteral("更新日志")))
            atomChangelog_ = atomChangelog_.mid(QStringLiteral("更新日志").size()).trimmed();
        const QString downloadsHtml = html.mid(html.indexOf(QStringLiteral("下载提示")));
        const QRegularExpression anchor(
            QStringLiteral(R"re(<a[^>]+href="([^"]+)"[^>]*>([^<]+)</a>)re"));
        auto links = anchor.globalMatch(downloadsHtml);
        while (links.hasNext()) {
            const auto link = links.next();
            atomDownloads_.append({link.captured(2), link.captured(1)});
        }
        const auto latest = QVersionNumber::fromString(latestVersion_);
        const auto current = QVersionNumber::fromString(currentVersion_);
        checking_ = false;
        emit checked(QVersionNumber::compare(latest, current) > 0, {});
    });
}
QString VersionService::changelog() const {
    if (!atomChangelog_.isEmpty()) return atomChangelog_;
    QString body = releaseBody(release_).trimmed();
    if (body.endsWith(QStringLiteral("!OCRUPDATE!")))
        body.chop(QStringLiteral("!OCRUPDATE!").size());
    const QRegularExpression section(
        QStringLiteral("## 更新日志\\n(.*?)(?=\\n## |\\z)"),
        QRegularExpression::DotMatchesEverythingOption);
    const auto match = section.match(body);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QList<QPair<QString, QString>> VersionService::downloadLinks() const {
    if (!atomDownloads_.isEmpty()) return atomDownloads_;
    QList<QPair<QString, QString>> links;
    const QString body = releaseBody(release_);
    const QRegularExpression section(
        QStringLiteral("## 下载提示\\n(.*?)(?=\\n# |\\z)"),
        QRegularExpression::DotMatchesEverythingOption);
    const auto match = section.match(body);
    if (!match.hasMatch()) return links;
    const QRegularExpression markdown(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));
    auto iterator = markdown.globalMatch(match.captured(1));
    while (iterator.hasNext()) {
        const auto link = iterator.next();
        links.append({link.captured(1), link.captured(2)});
    }
    return links;
}
QString VersionService::defaultDownloadUrl() const {
    const auto links = downloadLinks();
    QString body = releaseBody(release_).trimmed();
    const bool updateOcr = atomOcrUpdate_ || body.endsWith(QStringLiteral("!OCRUPDATE!"));
    if (updateOcr) {
        // 本次更新包含 OCR 引擎换代，必须下载与本地机型匹配的整包，
        // 否则会退回 Clear 增量包：那种包不带 PADDLEOCR 标识与新模型，
        // 结果是旧引擎目录被保留、旧模型目录被清理，OCR 直接失效。
        const QString version = paddleOcrVersion();
        const QRegularExpression marker(QStringLiteral("PaddleOCR-(.+)"));
        const auto match = marker.match(version);
        const QString identifier = match.hasMatch() ? match.captured(1).trimmed() : QString();
        // 1) 引擎代次未变时优先取与本地标识完全一致的安装包（等价于旧行为）
        if (!identifier.isEmpty()) {
            for (const auto& link : links)
                if (link.first.contains(identifier) || link.second.contains(identifier))
                    return link.second;
        }
        // 2) 跨代次更新：OCR 版本号已经跳变，改按「机型 + 算力」匹配同规格整包
        const QString flavor = ocrFlavor(identifier);
        if (!flavor.isEmpty()) {
            for (const auto& link : links)
                if (isFlavorInstaller(link.first, flavor)
                    || isFlavorInstaller(link.second, flavor))
                    return link.second;
        }
        // 3) 读不到本地标识（只装过 Clear 包的机器）时无从判断机型，
        //    退回通用的 CPU 整包：任何 Windows x64 机器都能跑，比给不带引擎的包安全
        for (const auto& link : links)
            if (isFlavorInstaller(link.first, QStringLiteral("CPU"))
                || isFlavorInstaller(link.second, QStringLiteral("CPU")))
                return link.second;
    }
    for (const auto& link : links)
        if (link.first.contains(QStringLiteral("Clear"))) return link.second;
    return QStringLiteral("https://github.com/Fairy-Oracle-Sanctuary/"
        "Fairy-Kekkai-Workshop/releases/download/v%1/"
        "Fairy-Kekkai-Workshop-v%1-Clear-Windows-x86_64-Setup.exe")
        .arg(latestVersion_);
}
} // namespace fkw
