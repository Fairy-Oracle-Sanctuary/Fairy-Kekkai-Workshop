#pragma once

#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QStackedWidget>
#include <QStringList>
#include <QThreadPool>
#include <QVBoxLayout>
#include <memory>
#include <qtfluentwidgets.h>
#include "common/config_keys.h"
#include "components/task_card.h"
#include "service/task_base.h"

namespace fkw {
class EmptyStatusWidget;

// 通用任务界面基类（对齐 Python TaskInterface：QThreadPool 调度 + event_bus 收口
// + CommandBar 批量操作）。子类通过 createTask/createWorker 扩展点接入具体 service。
class BaseTaskInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit BaseTaskInterface(PreviewTaskKind kind,
                               const ConfigKeys::Key* concurrentItem = nullptr,
                               QWidget* parent = nullptr);
    ~BaseTaskInterface() override;
    // 添加任务：创建任务对象与卡片并调度 Worker
    void addTask(const QString& input, const QString& output);
    // 停止所有运行中的任务（关闭应用时调用）
    void stopAll();
signals:
    // 是否重复 任务路径列表 是否发送消息（对应 Python returnTask）
    void returnTask(bool duplicated, const QStringList& paths, bool notify);
protected:
    // ---------- 子类扩展点 ----------
    // 由参数创建任务对象（含输出路径修正）；返回 nullptr 则忽略本次添加
    virtual std::shared_ptr<TaskBase> createTask(const QString& input,
                                                 const QString& output);
    // 创建任务 Worker（QRunnable，通过 event_bus 上报进度/完成）；
    // 返回 nullptr 表示该 service 尚未移植，任务保持等待
    virtual TaskWorker* createWorker(const std::shared_ptr<TaskBase>& task);
    // 任务去重路径
    virtual QString getTaskPath(const std::shared_ptr<TaskBase>& task) const;
    // 任务类型文案（用于通知消息）
    virtual QString taskTypeText() const;
    // 实时日志通道名（taskLogSignal 用；返回空则完成任务时不写日志框）
    virtual QString logName() const;
    // 向旧功能信号转发完成通知（如托盘），子类按需覆盖
    virtual void emitLegacyFinished(bool success, TaskCard* card);
    // 任务生成的文件列表（勾选删除文件时一并删除，子类可覆盖扩展）
    virtual QStringList taskGeneratedFiles(const std::shared_ptr<TaskBase>& task) const;
private:
    void removeCard(TaskCard* card, bool deleteFile);
    void filterTasks(const QString& filter);
    void updateSelection(bool checked);
    void setSelectionMode(bool enabled);
    bool isIdleCard(TaskCard* card) const;
    void removeSelected();
    void restartSelected();
    void handleTaskDeleted(int taskId, bool deleteFile);
    void handleCancelTask(int taskId);
    void handleRetryTask(int taskId);
    void updateTaskStatus(int taskId, int progress, const QVariant& status,
                          const QString& size, double time,
                          const QString& bitrate, double speed);
    void handleTaskFinished(int taskId, bool success, const QString& logPath);
    void updateMaxConcurrentTasks(const QJsonValue& value);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    PreviewTaskKind kind_;
    QString filter_ = QStringLiteral("all");
    qfw::SegmentedWidget* segmented_ = nullptr;
    QVBoxLayout* taskList_ = nullptr;
    QStackedWidget* region_ = nullptr;
    EmptyStatusWidget* empty_ = nullptr;
    qfw::CommandBarView* commandView_ = nullptr;
    QList<TaskCard*> cards_;
    QHash<int, TaskCard*> cardMap_;
    QHash<int, QPointer<TaskWorker>> threadMap_;
    QThreadPool taskPool_;
    QStringList taskPaths_;
    QSet<QString> inputPaths_;
    QString concurrentGroup_;
    QString concurrentName_;
    int selectionCount_ = 0;
    bool selectionMode_ = false;
};
} // namespace fkw
