#include "components/startup_maintenance.h"

#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QThread>

#include <utility>

#include "common/app_data.h"
#include "service/project_service.h"

namespace fkw {
namespace {

// 释放空间的人性化显示
QString humanSize(qint64 bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

}  // namespace

StartupMaintenanceWorker::StartupMaintenanceWorker(QStringList projectSources,
                                                   QString projectTarget, bool cleanOcr,
                                                   QObject* parent)
    : QObject(parent),
      projectSources_(std::move(projectSources)),
      projectTarget_(std::move(projectTarget)),
      cleanOcr_(cleanOcr) {}

void StartupMaintenanceWorker::run() {
    // 第一阶段：搬迁软件目录里的老项目
    if (!projectSources_.isEmpty()) {
        projects::RelocateWorker worker(projectSources_, projectTarget_);
        connect(&worker, &projects::RelocateWorker::itemStarted, this,
                &StartupMaintenanceWorker::projectItemStarted);
        connect(&worker, &projects::RelocateWorker::progressed, this,
                &StartupMaintenanceWorker::projectProgressed);
        connect(&worker, &projects::RelocateWorker::itemFinished, this,
                &StartupMaintenanceWorker::projectItemFinished);
        worker.run();
    }
    // 第二阶段：清理旧版 OCR 资源
    if (cleanOcr_) {
        emit ocrScanStarted();
        ocr::ResourceCleaner cleaner;
        connect(&cleaner, &ocr::ResourceCleaner::itemStarted, this,
                &StartupMaintenanceWorker::ocrItemStarted);
        connect(&cleaner, &ocr::ResourceCleaner::progressed, this,
                &StartupMaintenanceWorker::ocrProgressed);
        connect(&cleaner, &ocr::ResourceCleaner::itemFinished, this,
                &StartupMaintenanceWorker::ocrItemFinished);
        cleaner.run();
        const ocr::CleanupResult& result = cleaner.result();
        emit ocrFinished(result.removed.size(), result.failed.size(), result.freedBytes);
    }
    emit done();
}

StartupMaintenanceDialog::StartupMaintenanceDialog(const QStringList& projectSources,
                                                   const QString& projectTarget, bool cleanOcr,
                                                   QWidget* parent)
    : BaseInputDialog(trText("正在整理运行环境"), 640, parent),
      sources_(projectSources),
      cleanOcr_(cleanOcr),
      total_(projectSources.size()) {
    report_.target = projectTarget;
    setClosableOnMaskClicked(false);
    heading_ = qobject_cast<qfw::SubtitleLabel*>(viewLayout->itemAt(0)->widget());

    // 两个阶段都要跑时给项目搬迁留出前半段进度，避免后半段长期停在 0
    const bool hasProjects = !projectSources.isEmpty();
    if (hasProjects && cleanOcr) {
        projectWindow_ = {0, 55};
        ocrWindow_ = {55, 100};
    } else if (hasProjects) {
        projectWindow_ = {0, 100};
    } else {
        ocrWindow_ = {0, 100};
    }

    if (hasProjects) {
        auto* tip = new qfw::BodyLabel(
            trText("项目原本存放在软件目录里，清理或重装软件会连带删除项目数据，"
                   "因此现在把它们搬到独立的数据目录。"), this);
        tip->setWordWrap(true);
        viewLayout->addWidget(tip);
        auto* targetLabel = new qfw::CaptionLabel(
            trText("新的项目目录：%1").arg(QDir::toNativeSeparators(projectTarget)), this);
        targetLabel->setWordWrap(true);
        viewLayout->addWidget(targetLabel);
    }
    if (cleanOcr) {
        auto* ocrTip = new qfw::BodyLabel(
            trText("同时清除软件目录里旧版本的 PaddleOCR 与识别模型，回收磁盘空间。"), this);
        ocrTip->setWordWrap(true);
        viewLayout->addWidget(ocrTip);
    }

    currentLabel_ = new qfw::BodyLabel(
        hasProjects ? trText("正在准备迁移…") : trText("正在检查需要清理的资源…"), this);
    bar_ = new qfw::ProgressBar(this, true);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    log_ = new qfw::PlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setFixedHeight(120);
    viewLayout->addWidget(currentLabel_);
    viewLayout->addWidget(bar_);
    viewLayout->addWidget(log_);

    yesButton->setText(trText("请稍候…"));
    yesButton->setEnabled(false);
    hideCancelButton();
}

StartupMaintenanceDialog::~StartupMaintenanceDialog() {
    if (thread_ && thread_->isRunning()) {
        thread_->quit();
        thread_->wait();
    }
}

void StartupMaintenanceDialog::start() {
    thread_ = new QThread(this);
    auto* worker = new StartupMaintenanceWorker(sources_, report_.target, cleanOcr_);
    worker->moveToThread(thread_);
    connect(thread_, &QThread::started, worker, &StartupMaintenanceWorker::run);

    connect(worker, &StartupMaintenanceWorker::projectItemStarted, this,
            [this](const QString& source, int index, int total) {
        currentLabel_->setText(trText("正在迁移：%1（%2/%3）")
            .arg(QFileInfo(source).fileName()).arg(index).arg(total));
    });
    connect(worker, &StartupMaintenanceWorker::projectProgressed, this,
            [this](int percent) { setProgress(projectWindow_, percent); });
    connect(worker, &StartupMaintenanceWorker::projectItemFinished, this,
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

    connect(worker, &StartupMaintenanceWorker::ocrScanStarted, this, [this] {
        currentLabel_->setText(trText("正在统计需要清理的资源…"));
        setProgress(ocrWindow_, 0);
    });
    connect(worker, &StartupMaintenanceWorker::ocrItemStarted, this,
            [this](const QString& name, int index, int total) {
        currentLabel_->setText(trText("正在清理旧版资源：%1（%2/%3）").arg(name).arg(index).arg(total));
    });
    connect(worker, &StartupMaintenanceWorker::ocrProgressed, this,
            [this](int percent) { setProgress(ocrWindow_, percent); });
    connect(worker, &StartupMaintenanceWorker::ocrItemFinished, this,
            [this](const QString& path, bool ok, const QString& reason) {
        const QString name = QFileInfo(path).fileName();
        if (ok) appendLog(trText("已清理 %1（%2）").arg(name, reason));
        else appendLog(trText("%1 清理失败（%2），目录被占用或权限不足").arg(name, reason));
    });
    connect(worker, &StartupMaintenanceWorker::ocrFinished, this,
            [this](int removed, int failed, qint64 freedBytes) {
        ocrRemoved_ = removed;
        ocrFailed_ = failed;
        ocrFreedBytes_ = freedBytes;
    });
    connect(worker, &StartupMaintenanceWorker::done, this, &StartupMaintenanceDialog::finish);
    connect(thread_, &QThread::finished, worker, &QObject::deleteLater);
    thread_->start();
}

void StartupMaintenanceDialog::reject() {
    // 维护期间不允许关闭：搬了一半的项目或删了一半的目录都不该留下
    if (running_) return;
    qfw::MessageBoxBase::reject();
}

void StartupMaintenanceDialog::appendLog(const QString& text) {
    log_->appendPlainText(text);
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}

void StartupMaintenanceDialog::setProgress(const QPair<int, int>& window, int percent) {
    const int clamped = qBound(0, percent, 100);
    const int value = window.first + (window.second - window.first) * clamped / 100;
    if (value > bar_->value()) bar_->setValue(value);
}

void StartupMaintenanceDialog::finish() {
    if (thread_) {
        thread_->quit();
        thread_->wait();
    }
    running_ = false;
    bar_->setValue(100);

    QStringList summary;
    if (!sources_.isEmpty()) {
        const int migrated = total_ - failed_;
        if (failed_ == 0) summary << trText("已迁移 %1 个项目").arg(migrated);
        else summary << trText("项目迁移成功 %1 个，失败 %2 个").arg(migrated).arg(failed_);
    }
    if (cleanOcr_) {
        if (ocrRemoved_ > 0)
            summary << trText("已清理 %1 项旧版 OCR 资源，释放 %2")
                           .arg(ocrRemoved_)
                           .arg(humanSize(ocrFreedBytes_));
        if (ocrFailed_ > 0)
            summary << trText("有 %1 项清理失败，可在关闭占用后重试").arg(ocrFailed_);
    }
    if (summary.isEmpty()) summary << trText("没有需要处理的内容");

    const bool partiallyFailed = failed_ > 0 || ocrFailed_ > 0;
    if (heading_)
        heading_->setText(partiallyFailed ? trText("运行环境整理完成，部分内容失败")
                                          : trText("运行环境整理完成"));
    currentLabel_->setText(summary.join(trText("；")));
    yesButton->setText(partiallyFailed ? trText("我知道了") : trText("完成"));
    yesButton->setEnabled(true);
    yesButton->setFocus();
}

projects::RelocateReport runStartupMaintenance(QWidget* parent) {
    const QString projectTarget = projects::root();
    // 项目目录本身就在软件目录里时不做搬迁，否则等于原地搬运
    const QStringList legacy = insideSoftwareFolder(projectTarget)
        ? QStringList()
        : projects::projectsIn(sourceRoot());
    const bool cleanOcr = ocr::hasObsoleteResources();
    if (legacy.isEmpty() && !cleanOcr) return {};

    StartupMaintenanceDialog dialog(legacy, projectTarget, cleanOcr, parent);
    dialog.start();
    dialog.exec();
    return dialog.report();
}

} // namespace fkw
