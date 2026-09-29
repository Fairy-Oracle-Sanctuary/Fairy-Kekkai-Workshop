#pragma once

#include <QString>
#include <QStringList>
#include <qtfluentwidgets.h>

#include "components/dialog.h"
#include "service/project_relocate.h"

class QThread;

namespace fkw {
// 项目搬迁对话框：模态展示搬迁进度，过程中忽略一切关闭操作，结束后由调用方处理结果
class ProjectMigrationDialog : public BaseInputDialog {
    Q_OBJECT
public:
    ProjectMigrationDialog(const QString& target, const QStringList& sources,
                           QWidget* parent = nullptr);
    ~ProjectMigrationDialog() override;
    const projects::RelocateReport& report() const { return report_; }
    void start();
protected:
    void reject() override;
private:
    void appendLog(const QString& text);
    void finish();
    projects::RelocateReport report_;
    QStringList sources_;
    int total_ = 0;
    int failed_ = 0;
    bool running_ = true;
    qfw::SubtitleLabel* heading_ = nullptr;
    qfw::BodyLabel* currentLabel_ = nullptr;
    qfw::ProgressBar* bar_ = nullptr;
    qfw::PlainTextEdit* log_ = nullptr;
    QThread* thread_ = nullptr;
};

// 启动流程与设置页共用的搬迁入口：跑完对话框后把结果回写到链接表与排序表
projects::RelocateReport migrateProjects(const QString& target, const QStringList& sources,
                                         QWidget* parent);
} // namespace fkw
