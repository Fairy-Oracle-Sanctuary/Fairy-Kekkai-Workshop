#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "service/project_service.h"

namespace fkw::projects {
// 项目体检等级：正常 / 可修复 / 失联（项目目录已不存在）
enum class HealthLevel { Ok, Repairable, Broken };

enum class IssueCode {
    ProjectDirMissing,     // 项目目录不存在
    TitleFileMissing,      // 缺少 标题.txt
    TitleFileRenamed,      // 标题.txt 被改名
    TitleFileBroken,       // 标题.txt 内容已损坏
    EpisodeDirMissing,     // 缺少分集目录
    EpisodeRecordMissing,  // 分集目录没有对应的标题记录
    MarkerFileMissing,     // 缺少原标题标记文件
    IconFileMissing,       // 缺少项目图标
    TempDirLeftover,       // 残留的删除暂存目录
    NoEpisodeContent,      // 既没有分集目录也没有标题记录，无法自动修复
};

struct Issue {
    IssueCode code;
    QString detail;
    QVector<int> numbers;
};

struct Health {
    HealthLevel level = HealthLevel::Ok;
    QVector<Issue> issues;
    bool blocking = false;         // 是否属于结构性损坏：修复前不允许打开
    bool recordsKnown = false;     // 标题.txt 是否可读
    int recordCount = 0;           // 标题.txt 中的分集记录数
    int dirCount = 0;              // 数字分集目录数
    int targetCount = 0;           // 修复后的集数
    QVector<int> missingNumbers;   // 缺少的分集目录号
    QVector<int> extraNumbers;     // 有目录但没有标题记录的分集号
    bool ok() const { return level == HealthLevel::Ok; }
};

// 只读体检，不修改任何文件
Health check(const QString& path);

struct RepairReport {
    QStringList actions;   // 已执行的修复动作，供界面展示
    QString backupPath;    // 标题.txt 备份路径
    bool changed = false;  // 是否实际改动了磁盘
};

// 自动修复：先备份 标题.txt，再补齐分集目录与标题记录，最后回读校验，失败自动回滚
bool repair(const QString& path, RepairReport* report, QString* error = nullptr);
} // namespace fkw::projects
