#pragma once

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fkw::projects {
// 单个项目的搬迁结果
struct RelocateRecord {
    QString source;        // 原路径
    QString destination;   // 成功后的实际路径（目标重名时会带 -2、-3 后缀）
    bool ok = false;       // 项目是否已经安全落在新位置
    QString message;       // 失败原因，或成功时仍需提醒的事项
};
struct RelocateReport {
    QString target;
    QVector<RelocateRecord> records;
    bool allOk() const;
    // 成功项的「旧路径 -> 新路径」对照表，用于改写链接表与排序表
    QVector<QPair<QString, QString>> moved() const;
    QStringList failedSources() const;
};
// folder 顶层看起来像项目的子目录（损坏的项目也算，搬过去后还能一键修复）
QStringList projectsIn(const QString& folder);
// 把搬迁结果写回链接表与排序表：成功项改写路径，失败项登记为链接以保证依旧可见
void applyRelocation(const RelocateReport& report);

// 后台搬迁任务：同盘直接改名，跨盘则复制、逐文件校验后再删除原目录
class RelocateWorker : public QObject {
    Q_OBJECT
public:
    RelocateWorker(const QStringList& sources, const QString& target,
                   QObject* parent = nullptr);
public slots:
    void run();
signals:
    void itemStarted(const QString& source, int index, int total);
    void progressed(int percent);
    void itemFinished(const QString& source, const QString& destination, bool ok,
                      const QString& message);
    void done();
private:
    bool relocate(const QString& source, qint64 bytes, QString* destination, QString* message);
    void reportProgress();
    QStringList sources_;
    QString target_;
    qint64 totalBytes_ = 0;
    qint64 completedBytes_ = 0;
    int doneItems_ = 0;
    int lastPercent_ = -1;
};
} // namespace fkw::projects
