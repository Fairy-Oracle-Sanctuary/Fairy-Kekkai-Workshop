#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace fkw::projects {
struct Episode {
    QString originalTitle;
    QString translatedTitle;
    QString videoUrl;
};
struct Document {
    QStringList indexLines;
    QVector<Episode> episodes;
    bool translated = false;
};
// 当前项目库根目录：优先使用 project.json 记住的位置，失效时按
// 软件目录的平行目录（D 盘 -> C 盘）-> 文档目录 -> 用户数据目录重新探测。
// 返回值保证不在软件目录内，且目录已存在。
QString root();
// 校验候选目录能否作为项目库：必须存在、不与软件目录重叠、且是空目录
bool checkRoot(const QString& path, QString* error = nullptr);
// 记住新的项目库根目录（调用方应先把已有项目搬迁过去）
bool rememberRoot(const QString& path, QString* error = nullptr);
QStringList links();
QStringList order();
bool setLinks(const QStringList& paths, QString* error = nullptr);
bool setOrder(const QStringList& paths, QString* error = nullptr);
// 纯数字分集目录号，升序排列（忽略其它杂项目录）
QVector<int> episodeNumbers(const QString& path);
// 严格识别：存在 标题.txt，且分集目录为连续的 1..N
bool isProject(const QString& path);
// 宽松识别：标题文件无需完好；标题缺失时以连续分集目录、可解析的改名标题，
// 或不连续分集目录加项目标记/图标/标准分集文件确认项目身份。迁移与项目列表共用。
bool looksLikeProject(const QString& path);
// 严格识别通过的项目路径；candidates() 额外保留待修复与失联的项目
QStringList paths();
QStringList candidates();
bool create(const QString& name, int count, const QString& title, QString* error = nullptr);
bool createPlaylist(const QString& name, const QString& title,
                    const QVector<Episode>& episodes, QString* error = nullptr);
bool importProject(const QString& path, bool copy, QString* error = nullptr);
bool update(const QString& path, const QString& name, const QString& title,
            const QString& icon, QString* error = nullptr);
bool remove(const QString& path, bool unlink, QString* error = nullptr);
bool read(const QString& path, Document* document, QString* error = nullptr);
// 解析任意路径下的 标题.txt（项目体检与修复复用）
bool readFile(const QString& filePath, Document* document, QString* error = nullptr);
// 按项目格式写回 标题.txt
bool writeDocument(const QString& path, const Document& document, QString* error = nullptr);
bool insertEpisode(const QString& path, int number, const Episode& episode,
                   QString* error = nullptr);
bool deleteEpisode(const QString& path, int number, QString* error = nullptr);
bool editEpisode(const QString& path, int number, const Episode& episode,
                 QString* error = nullptr);
} // namespace fkw::projects
