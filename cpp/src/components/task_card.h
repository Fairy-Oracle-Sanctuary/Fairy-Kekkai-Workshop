#pragma once

#include <QCheckBox>
#include <qtfluentwidgets.h>
#include <memory>

#include "common/task_status.h"
#include "service/task_base.h"

namespace fkw {
enum class PreviewTaskKind { Ocr, Whisper, Translate, FFmpeg };

// 删除确认对话框（对齐 Python DeleteTaskDialog：删除文件复选框默认不勾选）。
// 返回是否确认删除；确认时 *deleteFiles 为复选框状态。
bool confirmTaskDeletion(QWidget* parent, const QString& title,
                         const QString& content, bool* deleteFiles);

// 通用任务卡片：只依赖 TaskBase 的公共字段，具体 service 的任务对象由各任务
// 界面创建后传入。kind 决定是否显示压制专有的实时信息（大小/耗时/码率/速度）。
class TaskCard : public qfw::CardWidget {
    Q_OBJECT
public:
    TaskCard(std::shared_ptr<TaskBase> task, PreviewTaskKind kind,
             QWidget* parent = nullptr);
    TaskStatus status() const { return status_; }
    bool isChecked() const;
    void setChecked(bool checked);
    void setSelectionMode(bool enabled);
    // 对齐 Python TaskCardBase.updateTask：每次全量刷新进度/状态/压制信息。
    // 非压制卡片没有实时信息标签，多出的参数会被忽略。
    void updateTask(int progress = 0, TaskStatus status = TaskStatus::Waiting,
                    const QString& size = QStringLiteral("0KiB"),
                    double time = 0.0,
                    const QString& bitrate = QStringLiteral("0kbits/s"),
                    double speed = 0.0);
    const std::shared_ptr<TaskBase>& task() const { return task_; }
signals:
    void checkedChanged(bool checked);
    void statusChanged();
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    void updateStatus(TaskStatus status);
    void updateInfo(const QString& size, double time, const QString& bitrate,
                    double speed);
    void updateInfoVisible(bool visible);
    void openFolder();
    void cancelTask();
    void retryTask();
    void showLog();
    void deleteTask();
    std::shared_ptr<TaskBase> task_;
    QString stageText_;  // two-pass 阶段文案（预留，当前流程未启用）
    PreviewTaskKind kind_;
    TaskStatus status_ = TaskStatus::Waiting;
    bool selectionMode_ = false;
    qfw::CheckBox* checkBox_ = nullptr;
    qfw::CaptionLabel* statusLabel_ = nullptr;
    qfw::CaptionLabel* sizeLabel_ = nullptr;
    qfw::CaptionLabel* timeLabel_ = nullptr;
    qfw::CaptionLabel* bitrateLabel_ = nullptr;
    qfw::CaptionLabel* speedLabel_ = nullptr;
    qfw::CaptionLabel* finishTimeLabel_ = nullptr;
    qfw::ProgressBar* progressBar_ = nullptr;
    qfw::ToolButton* folderButton_ = nullptr;
    qfw::ToolButton* cancelButton_ = nullptr;
    qfw::ToolButton* retryButton_ = nullptr;
    qfw::ToolButton* logButton_ = nullptr;
    qfw::ToolButton* deleteButton_ = nullptr;
    QList<QWidget*> processingInfo_;
    QList<QWidget*> finishedInfo_;
};
} // namespace fkw
