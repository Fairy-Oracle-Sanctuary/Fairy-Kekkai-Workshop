#include "components/project_migration.h"

#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QThread>

#include "common/app_data.h"

namespace fkw {
ProjectMigrationDialog::ProjectMigrationDialog(const QString& target, const QStringList& sources,
                                               QWidget* parent)
    : BaseInputDialog(trText("正在迁移项目"), 620, parent), total_(sources.size()) {
    report_.target = target;
    sources_ = sources;
    setClosableOnMaskClicked(false);
    heading_ = qobject_cast<qfw::SubtitleLabel*>(viewLayout->itemAt(0)->widget());

    auto* tip = new qfw::BodyLabel(trText("项目原本存放在软件目录里，清理或重装软件会连带删除项目数据，"
                                         "因此现在把它们搬到独立的数据目录。"), this);
    tip->setWordWrap(true);
    auto* targetLabel = new qfw::CaptionLabel(
        trText("新的项目目录：%1").arg(QDir::toNativeSeparators(target)), this);
    targetLabel->setWordWrap(true);
    currentLabel_ = new qfw::BodyLabel(trText("正在准备…"), this);
    bar_ = new qfw::ProgressBar(this, true);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    log_ = new qfw::PlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setFixedHeight(120);
    viewLayout->addWidget(tip);
    viewLayout->addWidget(targetLabel);
    viewLayout->addWidget(currentLabel_);
    viewLayout->addWidget(bar_);
    viewLayout->addWidget(log_);

    yesButton->setText(trText("请稍候…"));
    yesButton->setEnabled(false);
    hideCancelButton();
}

ProjectMigrationDialog::~ProjectMigrationDialog() {
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        thread_->wait();
    }
}

void ProjectMigrationDialog::start() {
    thread_ = new QThread(this);
    auto* worker = new projects::RelocateWorker(sources_, report_.target);
    worker->moveToThread(thread_);
    connect(thread_, &QThread::started, worker, &projects::RelocateWorker::run);
    connect(worker, &projects::RelocateWorker::itemStarted, this,
            [this](const QString& source, int index, int total) {
        currentLabel_->setText(trText("正在迁移：%1（%2/%3）")
            .arg(QFileInfo(source).fileName()).arg(index).arg(total));
    });
    connect(worker, &projects::RelocateWorker::progressed, this,
            [this](int percent) { bar_->setValue(percent); });
    connect(worker, &projects::RelocateWorker::itemFinished, this,
            [this](const QString& source, const QString& destination, bool ok,
                   const QString& message) {
        report_.records.append({source, destination, ok, message});
        const QString name = QFileInfo(source).fileName();
        if (!ok) {
            ++failed_;
            appendLog(trText("%1 迁移失败：%2").arg(name, message));
        } else if (!message.isEmpty()) {
            appendLog(trText("%1 迁移完成（%2）").arg(name, message));
        } else {
            appendLog(trText("%1 迁移完成").arg(name));
        }
    });
    connect(worker, &projects::RelocateWorker::done, this, &ProjectMigrationDialog::finish);
    connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
    thread_->start();
}

void ProjectMigrationDialog::reject() {
    // 搬迁期间不允许关闭，避免留下「搬了一半」的项目库
    if (running_) return;
    qfw::MessageBoxBase::reject();
}

void ProjectMigrationDialog::appendLog(const QString& text) {
    log_->appendPlainText(text);
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}

void ProjectMigrationDialog::finish() {
    if (thread_) {
        thread_->quit();
        thread_->wait();
    }
    running_ = false;
    bar_->setValue(100);
    if (failed_ == 0) {
        if (heading_) heading_->setText(trText("项目迁移完成"));
        currentLabel_->setText(trText("已迁移 %1 个项目").arg(total_));
        yesButton->setText(trText("完成"));
    } else {
        if (heading_) heading_->setText(trText("项目迁移完成，部分失败"));
        currentLabel_->setText(trText("成功 %1 个，失败 %2 个；失败的项目仍留在原位置")
            .arg(total_ - failed_).arg(failed_));
        yesButton->setText(trText("我知道了"));
    }
    yesButton->setEnabled(true);
    yesButton->setFocus();
}

projects::RelocateReport migrateProjects(const QString& target, const QStringList& sources,
                                         QWidget* parent) {
    ProjectMigrationDialog dialog(target, sources, parent);
    dialog.start();
    dialog.exec();
    return dialog.report();
}
} // namespace fkw
