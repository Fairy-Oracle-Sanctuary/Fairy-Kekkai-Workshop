#include "service/ocr_migration.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include "common/app_data.h"
#include "common/config.h"
#include "common/config_keys.h"
#include "common/logger.h"
#include "common/setting.h"

namespace fkw::ocr {
namespace {

// ── 上个版本的资源名（口径来自 GitHub 工作流）──────────────────────────
// .github/workflows/deploy-windows.yml（release.yml 同源）中：
//   引擎：下载 timminator/PaddleOCR-Standalone v1.4.0 的资产
//        PaddleOCR-CPU-v1.4.0.7z / PaddleOCR-GPU-v1.4.0-CUDA-{11.8,12.9}.7z，
//        解压后重命名并落位为 tools\PaddleOCR-*-v1.5.1*；
//   模型：PaddleOCR.PP-OCRv5.support.files.VideOCR.7z 解压为
//        tools\PaddleOCR.PP-OCRv5.support.files。
// 本次已升级到 PaddleOCR-Standalone v3.7.0 + PP-OCRv6（见根目录 PADDLEOCR
// 标识与 setting.paddleOcrSupportFilesName()），所以上面这些 v1.5.1 / PP-OCRv5
// 的落位目录就是「上个版本」的残留。
const QStringList& legacyEngineDirs() {
    static const QStringList names = {
        QStringLiteral("PaddleOCR-CPU-v1.5.1"),
        QStringLiteral("PaddleOCR-GPU-v1.5.1-CUDA-11.8"),
        QStringLiteral("PaddleOCR-GPU-v1.5.1-CUDA-12.9"),
    };
    return names;
}

const QStringList& legacyModelDirs() {
    static const QStringList names = {
        QStringLiteral("PaddleOCR.PP-OCRv5.support.files"),
    };
    return names;
}

// 上个版本工作流下载的压缩包。正常只出现在构建机的 downloads/，
// 本地按老流程构建过的软件目录里可能留有，同样按上个版本残留处理。
const QStringList& legacyArchives() {
    static const QStringList names = {
        QStringLiteral("PaddleOCR-CPU-v1.4.0.7z"),
        QStringLiteral("PaddleOCR-GPU-v1.4.0-CUDA-11.8.7z"),
        QStringLiteral("PaddleOCR-GPU-v1.4.0-CUDA-12.9.7z"),
        QStringLiteral("PaddleOCR.PP-OCRv5.support.files.VideOCR.7z"),
    };
    return names;
}

// 更早历史版本的兜底：引擎目录形如 PaddleOCR-CPU-* / PaddleOCR-GPU-*
const QRegularExpression& versionDirPattern() {
    static const QRegularExpression pattern(QStringLiteral("^PaddleOCR-(?:CPU|GPU)-"),
                                            QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// 更早历史版本的兜底：模型目录形如 PaddleOCR.PP-OCRv5.support.files
const QRegularExpression& modelDirPattern() {
    static const QRegularExpression pattern(
        QStringLiteral("^PaddleOCR\\.PP-OCRv\\d+\\.support\\.files$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// 新版不再需要的字体支持目录（旧版 utils.py 会去 program_dir 下找它）
const QString kFontSupportDir = QStringLiteral("PaddleOCR.font.support.files");

// 是否属于旧版引擎目录：不等于当前版本，且命中工作流名单或兜底规则。
// 「不等于当前版本」这一条保证当前正在使用的目录永远不会被选中。
bool isLegacyEngine(const QString& name, const QString& currentVersion) {
    if (name.compare(currentVersion, Qt::CaseInsensitive) == 0) return false;
    return legacyEngineDirs().contains(name, Qt::CaseInsensitive)
        || versionDirPattern().match(name).hasMatch();
}

// 是否属于旧版识别模型目录，规则同上
bool isLegacyModel(const QString& name, const QString& currentSupportFiles) {
    if (name.compare(currentSupportFiles, Qt::CaseInsensitive) == 0) return false;
    return legacyModelDirs().contains(name, Qt::CaseInsensitive)
        || modelDirPattern().match(name).hasMatch();
}

// 统一成带正斜杠的路径，便于做「是否位于软件目录内」的前缀判断
QString normalizedPath(const QString& path) {
    return QDir::cleanPath(QDir::fromNativeSeparators(path))
        .replace(QLatin1Char('\\'), QLatin1Char('/'));
}

// 取路径最后一层名
QString leafName(const QString& path) {
    return QFileInfo(QDir::cleanPath(path)).fileName();
}

// 目录树的字节数，仅用于报告释放了多少空间
qint64 treeBytes(const QString& path) {
    qint64 bytes = 0;
    QDirIterator entries(path, QDir::Files | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        entries.next();
        bytes += entries.fileInfo().size();
    }
    return bytes;
}

// 条目的字节数：目录递归统计，文件直接取大小
qint64 entryBytes(const QFileInfo& info) {
    return info.isDir() ? treeBytes(info.absoluteFilePath()) : info.size();
}

// 命中则返回需要清理的原因，否则返回空串。
// 判据是「等于工作流里上个版本的名字（或更早版本的同族名字），但不等于当前版本」，
// 因此当前版本自身永远安全，与 OCR 无关的目录（whisper、ffmpeg 等）也不会被波及。
QString obsoleteReason(const QString& name, const QString& currentVersion,
                       const QString& currentSupportFiles) {
    if (isLegacyEngine(name, currentVersion))
        return legacyEngineDirs().contains(name, Qt::CaseInsensitive)
            ? QStringLiteral("上个版本的 PaddleOCR（v1.5.1）")
            : QStringLiteral("旧版 PaddleOCR");
    if (isLegacyModel(name, currentSupportFiles))
        return legacyModelDirs().contains(name, Qt::CaseInsensitive)
            ? QStringLiteral("上个版本的识别模型（PP-OCRv5）")
            : QStringLiteral("旧版识别模型");
    if (legacyArchives().contains(name, Qt::CaseInsensitive))
        return QStringLiteral("上个版本的 OCR 压缩包");
    if (name.compare(kFontSupportDir, Qt::CaseInsensitive) == 0)
        return QStringLiteral("已废弃的字体支持目录");
    return {};
}

// 软件目录里需要清理的条目：根目录、tools、downloads 各扫一层。
// 老版本把引擎放 tools\<tag>、模型放 tools\<name>，构建机还会留下 downloads\ 里的压缩包
QFileInfoList obsoleteEntries(const QString& currentVersion,
                              const QString& currentSupportFiles) {
    QFileInfoList hits;
    const QString root = normalizedPath(sourceRoot());
    const QStringList folders = {root, root + QStringLiteral("/tools"),
                                 root + QStringLiteral("/downloads")};
    QSet<QString> seen;
    for (const QString& folder : folders) {
        const QDir dir(folder);
        if (!dir.exists()) continue;
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& entry : entries) {
            const QString path = normalizedPath(entry.absoluteFilePath());
            // 只处理软件目录内部的条目，防止误删外部同名目录
            if (!path.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive)) continue;
            const QString key = path.toLower();
            if (seen.contains(key)) continue;
            seen.insert(key);
            if (obsoleteReason(entry.fileName(), currentVersion, currentSupportFiles).isEmpty())
                continue;
            hits.append(entry);
        }
    }
    return hits;
}

// 配置项是否指向旧版本：exe 看所在目录名，模型项看目录名本身；
// 两者都额外把「目标已不存在」视为需要纠正
bool isStalePath(const QString& storedPath, bool executable, const QString& currentVersion,
                 const QString& currentSupportFiles) {
    const QString cleaned = normalizedPath(storedPath);
    if (executable) {
        if (isLegacyEngine(leafName(QFileInfo(cleaned).absolutePath()), currentVersion))
            return true;
        return !QFileInfo::exists(cleaned);
    }
    const QString dirName = leafName(cleaned);
    if (isLegacyModel(dirName, currentSupportFiles)) return true;
    if (dirName.compare(kFontSupportDir, Qt::CaseInsensitive) == 0) return true;
    return !QDir(cleaned).exists();
}

// 待体检的配置项
struct PathTarget {
    QString label;     // 报告里显示的名字
    bool executable;   // true：指向 exe 文件；false：指向目录
};

}  // namespace

QVector<PathFix> fixOcrPaths() {
    QVector<PathFix> fixes;
    const QString currentVersion = paddleOcrVersion();
    // 认不出当前版本就不动配置，避免把正确路径改坏
    if (currentVersion.isEmpty()) return fixes;
    const QString currentSupportFiles = paddleOcrSupportFilesName();
    AppConfig& config = AppConfig::instance();

    const QVector<PathTarget> targets = {
        {QStringLiteral("PaddleocrPath"), true},
        {QStringLiteral("supportFilesPath"), false},
    };
    for (const PathTarget& target : targets) {
        const QJsonValue raw = target.executable
            ? config.value(ConfigKeys::paddleocrPath)
            : config.value(ConfigKeys::supportFilesPath);
        const QString stored = raw.toString().trimmed();
        if (stored.isEmpty()) continue;  // 从未写过，动态默认值本身就是当前的
        if (!isStalePath(stored, target.executable, currentVersion, currentSupportFiles))
            continue;
        const QString fallback = target.executable ? paddleOcrDefaultPath()
                                                   : paddleOcrSupportFilesDefaultPath();
        // 旧值与当前默认值一致（例如工具尚未安装）时不写盘，避免无意义的重写
        if (normalizedPath(stored).compare(normalizedPath(fallback), Qt::CaseInsensitive) == 0)
            continue;
        const ConfigKeys::Key key = target.executable ? ConfigKeys::paddleocrPath
                                                      : ConfigKeys::supportFilesPath;
        if (config.set(key, fallback))
            fixes.append({target.label, stored, fallback});
    }
    return fixes;
}

bool hasObsoleteResources() {
    const QString currentVersion = paddleOcrVersion();
    if (currentVersion.isEmpty()) return false;
    return !obsoleteEntries(currentVersion, paddleOcrSupportFilesName()).isEmpty();
}

QVector<ObsoleteResource> scanObsoleteResources() {
    QVector<ObsoleteResource> items;
    const QString currentVersion = paddleOcrVersion();
    // 认不出当前版本就不删任何东西
    if (currentVersion.isEmpty()) return items;
    const QString currentSupportFiles = paddleOcrSupportFilesName();
    const QFileInfoList entries = obsoleteEntries(currentVersion, currentSupportFiles);
    items.reserve(entries.size());
    for (const QFileInfo& entry : entries) {
        const QString path = normalizedPath(entry.absoluteFilePath());
        items.append({path, entry.fileName(),
                      obsoleteReason(entry.fileName(), currentVersion, currentSupportFiles),
                      entryBytes(entry), entry.isDir()});
    }
    return items;
}

ResourceCleaner::ResourceCleaner(QObject* parent) : QObject(parent) {}

void ResourceCleaner::run() {
    auto logger = Logger::get(QStringLiteral("OcrMigration"), QStringLiteral("ocr_migration"));
    const QVector<ObsoleteResource> items = scanObsoleteResources();
    const int total = items.size();
    if (total > 0) {
        qint64 totalBytes = 0;
        for (const ObsoleteResource& item : items) totalBytes += item.bytes;

        qint64 doneBytes = 0;
        int index = 0;
        for (const ObsoleteResource& item : items) {
            emit itemStarted(item.name, ++index, total);
            const QString detail = item.path + QStringLiteral("（") + item.reason
                + QStringLiteral("）");
            const bool ok = item.directory ? QDir(item.path).removeRecursively()
                                           : QFile::remove(item.path);
            if (ok) {
                result_.freedBytes += item.bytes;
                result_.removed.append(detail);
                emit itemFinished(item.path, true, item.reason);
                logger->info(QStringLiteral("已清理旧版 OCR 资源：") + detail);
            } else {
                result_.failed.append(detail + QStringLiteral("，被占用或权限不足"));
                emit itemFinished(item.path, false, item.reason);
                logger->warning(QStringLiteral("清理旧版 OCR 资源失败：") + detail);
            }
            doneBytes += item.bytes;
            const int percent = totalBytes > 0
                ? static_cast<int>(doneBytes * 100 / totalBytes)
                : index * 100 / total;
            emit progressed(qBound(0, percent, 100));
        }
        const double freedMB = static_cast<double>(result_.freedBytes) / (1024.0 * 1024.0);
        logger->info(QStringLiteral("OCR 资源清理完成：删除 %1 项，释放 %2 MB")
                         .arg(result_.removed.size())
                         .arg(freedMB, 0, 'f', 1));
    }
    emit done();
}

}  // namespace fkw::ocr
