#include "service/legacy_cleanup.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "common/app_data.h"
#include "common/logger.h"
#include "service/project_service.h"

// 上一代（Python + Nuitka/PySide6）版本的残留清理。
//
// Python 版由 Nuitka 打包，整棵依赖树被平铺在安装目录顶层：PIL/numpy/cv2/PySide6…
// 以及 python39.dll、大量 *.pyd。C++ 版只需要 Qt/OpenCV/VC 运行库和 tools，
// 两代共用同一个 AppId 与安装目录，覆盖升级后这些文件就留在了原地。
//
// 这里的判据是「名字精确命中 Python 生态的已知产物」，且只扫软件目录顶层一层：
// 既不会碰到 C++ 版自己的文件，也不会深入 tools/ 或用户的项目目录。
namespace fkw::legacy {
namespace {

// ── Python 版铺在顶层的依赖目录 ──────────────────────────────────────
const QStringList& legacyDirs() {
    static const QStringList names = {
        QStringLiteral("PIL"),                // Pillow
        QStringLiteral("PySide6"),            // Qt for Python（含内层 qt-plugins）
        QStringLiteral("certifi"),
        QStringLiteral("charset_normalizer"),
        QStringLiteral("cv2"),                // opencv-python（C++ 版用 opencv_world4120.dll）
        QStringLiteral("jiter"),
        QStringLiteral("numpy"),
        QStringLiteral("numpy.libs"),
        QStringLiteral("pydantic_core"),
        QStringLiteral("shiboken6"),
    };
    return names;
}

// ── Python 版铺在顶层的文件（通配符）─────────────────────────────────
// caseSensitive 只给 Qt 模块用：Python 版留下的这几个是**小写**名，
// 而 windeployqt 给 C++ 版铺的是 Qt6Pdf/Qt6Sql/Qt6Xml 这种大写名。
// 大小写敏感才能只命中 Python 版那一份，不会把 C++ 版自己的 Qt 库删掉。
struct FilePattern {
    const char* pattern;
    const char* reason;
    bool caseSensitive = false;
};

const QVector<FilePattern>& legacyFilePatterns() {
    static const QVector<FilePattern> patterns = {
        {"*.pyd", "Python 扩展模块", false},
        {"python*.dll", "Python 运行时", false},
        {"pythoncom*.dll", "Python 运行时", false},
        {"pywintypes*.dll", "Python 运行时", false},
        {"pyside6*.dll", "PySide6 运行时", false},
        {"shiboken6*.dll", "PySide6 运行时", false},
        {"libcrypto-1_1*.dll", "Python 版附带的 OpenSSL 1.1", false},
        {"libssl-1_1*.dll", "Python 版附带的 OpenSSL 1.1", false},
        {"libffi-*.dll", "Python 依赖的 libffi", false},
        {"qt6pdf.dll", "Python 版多余的 Qt 模块", true},
        {"qt6sql.dll", "Python 版多余的 Qt 模块", true},
        {"qt6xml.dll", "Python 版多余的 Qt 模块", true},
        // 改名前的 C++ 主程序：产物名已统一为 Fairy-Kekkai-Workshop.exe，
        // 手工替换 exe 时旧的 -Cpp 版会留在原地
        {"Fairy-Kekkai-Workshop-Cpp.exe", "改名前的旧主程序", false},
    };
    return patterns;
}

struct FileRule {
    QRegularExpression regex;
    QString reason;
};

// 把通配符规则编译成正则，只做一次
const QVector<FileRule>& legacyFileRules() {
    static const QVector<FileRule> rules = [] {
        QVector<FileRule> result;
        for (const FilePattern& pattern : legacyFilePatterns()) {
            const QString wildcard = QString::fromLatin1(pattern.pattern);
            const auto options = pattern.caseSensitive
                ? QRegularExpression::NoPatternOption
                : QRegularExpression::CaseInsensitiveOption;
            result.append({QRegularExpression(
                               QRegularExpression::wildcardToRegularExpression(wildcard),
                               options),
                           QString::fromUtf8(pattern.reason)});
        }
        return result;
    }();
    return rules;
}

// 统一成带正斜杠的路径，便于做「是否位于软件目录内」的前缀判断
QString normalizedPath(const QString& path) {
    return QDir::cleanPath(QDir::fromNativeSeparators(path))
        .replace(QLatin1Char('\\'), QLatin1Char('/'));
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

// 命中则返回需要清理的原因，否则返回空串
QString residueReason(const QFileInfo& entry) {
    const QString name = entry.fileName();
    if (entry.isDir()) {
        // 用户项目优先：使用迁移的宽松识别，损坏项目或搬迁失败的项目也不能被清理，
        // 这里绝不能当成残留删除（项目目录恰好叫 cv2/numpy 这类名字时尤其危险）
        if (projects::looksLikeProject(entry.absoluteFilePath()) || QFileInfo::exists(
                QDir(entry.absoluteFilePath()).filePath(QStringLiteral("标题.txt"))))
            return {};
        for (const QString& dir : legacyDirs())
            if (name.compare(dir, Qt::CaseInsensitive) == 0)
                return QStringLiteral("上一代 Python 版本的依赖目录");
        return {};
    }
    // 正在运行的程序自身永远不删（手工把新 exe 放进旧目录时可能命中同名规则）
    if (normalizedPath(entry.absoluteFilePath())
            .compare(normalizedPath(QCoreApplication::applicationFilePath()),
                     Qt::CaseInsensitive) == 0)
        return {};
    for (const FileRule& rule : legacyFileRules())
        if (rule.regex.match(name).hasMatch()) return rule.reason;
    return {};
}

// 软件目录顶层的残留条目。软件目录可能不止一个：发行版就是 exe 所在目录，
// 开发构建的 exe 落在 build 输出目录里，两个目录都要看。
QFileInfoList residueEntries() {
    QFileInfoList hits;
    QSet<QString> seen;
    for (const QString& rawRoot : softwareRoots()) {
        const QString root = normalizedPath(rawRoot);
        const QDir dir(root);
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
            if (residueReason(entry).isEmpty()) continue;
            hits.append(entry);
        }
    }
    return hits;
}

}  // namespace

bool hasLegacyResidue() { return !residueEntries().isEmpty(); }

QVector<ResidueItem> scanLegacyResidue() {
    QVector<ResidueItem> items;
    const QFileInfoList entries = residueEntries();
    items.reserve(entries.size());
    for (const QFileInfo& entry : entries) {
        items.append({normalizedPath(entry.absoluteFilePath()), entry.fileName(),
                      residueReason(entry), entryBytes(entry), entry.isDir()});
    }
    return items;
}

ResidueCleaner::ResidueCleaner(QObject* parent) : QObject(parent) {}

void ResidueCleaner::run() {
    auto logger = Logger::get(QStringLiteral("LegacyCleanup"), QStringLiteral("legacy_cleanup"));
    const QVector<ResidueItem> items = scanLegacyResidue();
    const int total = items.size();
    if (total > 0) {
        qint64 totalBytes = 0;
        for (const ResidueItem& item : items) totalBytes += item.bytes;

        qint64 doneBytes = 0;
        int index = 0;
        for (const ResidueItem& item : items) {
            emit itemStarted(item.name, ++index, total);
            const QString detail = item.path + QStringLiteral("（") + item.reason
                + QStringLiteral("）");
            const bool ok = item.directory ? QDir(item.path).removeRecursively()
                                           : QFile::remove(item.path);
            if (ok) {
                result_.freedBytes += item.bytes;
                result_.removed.append(detail);
                emit itemFinished(item.path, true, item.reason);
                logger->info(QStringLiteral("已清理上一代残留：") + detail);
            } else {
                result_.failed.append(detail + QStringLiteral("，被占用或权限不足"));
                emit itemFinished(item.path, false, item.reason);
                logger->warning(QStringLiteral("清理上一代残留失败：") + detail);
            }
            doneBytes += item.bytes;
            const int percent = totalBytes > 0
                ? static_cast<int>(doneBytes * 100 / totalBytes)
                : index * 100 / total;
            emit progressed(qBound(0, percent, 100));
        }
        const double freedMB = static_cast<double>(result_.freedBytes) / (1024.0 * 1024.0);
        logger->info(QStringLiteral("上一代残留清理完成：删除 %1 项，释放 %2 MB")
                         .arg(result_.removed.size())
                         .arg(freedMB, 0, 'f', 1));
    }
    emit done();
}

}  // namespace fkw::legacy
