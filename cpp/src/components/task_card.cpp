#include "components/task_card.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QUrl>
#include <QVBoxLayout>
#include "common/app_data.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/task_status.h"
#include "common/utils.h"

namespace fkw {
namespace {
qfw::ToolButton* button(qfw::FluentIconEnum icon, const QString& tip, QWidget* parent) {
    auto* result = new qfw::ToolButton(qfw::FluentIcon(icon).qicon(), parent);
    result->setToolTip(tip);
    return result;
}
// 对齐 Python str(float)：整数值也保留一位小数（如 0.0、100.0）
QString pythonFloat(double value) {
    QString text = QString::number(value);
    if (!text.contains(QLatin1Char('.'))) text += QStringLiteral(".0");
    return text;
}
}

bool confirmTaskDeletion(QWidget* parent, const QString& title,
                         const QString& content, bool* deleteFiles) {
    qfw::MessageDialog dialog(title, content, parent);
    auto* contentLabel = dialog.findChild<QLabel*>(QStringLiteral("contentLabel"));
    if (contentLabel) {
        auto* body = static_cast<QVBoxLayout*>(contentLabel->parentWidget()->layout()->itemAt(0)->layout());
        auto* deleteFilesBox = new qfw::CheckBox(Text::instance().DeleteFiles,
                                                 contentLabel->parentWidget());
        deleteFilesBox->setChecked(false);
        body->insertWidget(2, deleteFilesBox);
        if (dialog.exec() == QDialog::Accepted) {
            *deleteFiles = deleteFilesBox->isChecked();
            return true;
        }
        return false;
    }
    return dialog.exec() == QDialog::Accepted;
}

TaskCard::TaskCard(std::shared_ptr<TaskBase> task, PreviewTaskKind kind, QWidget* parent)
    : qfw::CardWidget(parent), task_(std::move(task)), kind_(kind) {
    setMinimumHeight(75);
    setClickEnabled(true);
    setCursor(Qt::PointingHandCursor);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(20, 11, 20, 11);
    checkBox_ = new qfw::CheckBox(this);
    checkBox_->setFixedSize(23, 23);
    checkBox_->hide();
    row->addWidget(checkBox_);
    row->addSpacing(5);
    const QIcon image = task_->iconName.isEmpty()
        ? QFileIconProvider().icon(QFileInfo(task_->inputPath))
        : QIcon(QStringLiteral(":/app/images/icons/") + task_->iconName +
                QStringLiteral(".svg"));
    auto* fileIcon = new qfw::IconWidget(image, this);
    fileIcon->setFixedSize(32, 32);
    row->addWidget(fileIcon);
    row->addSpacing(5);

    auto* details = new QVBoxLayout;
    details->setContentsMargins(0, 0, 0, 0);
    details->setSpacing(5);
    auto* name = new qfw::BodyLabel(task_->fileName, this);
    QFont font = name->font();
    font.setPixelSize(18);
    font.setBold(true);
    name->setFont(font);
    name->setWordWrap(true);
    details->addWidget(name);
    auto* info = new QHBoxLayout;
    info->setContentsMargins(0, 0, 0, 0);
    info->setSpacing(3);
    auto addInfo = [this, info](qfw::FluentIconEnum icon, const QString& value, bool processing) {
        auto* image = new qfw::IconWidget(qfw::FluentIcon(icon), this);
        image->setFixedSize(16, 16);
        auto* label = new qfw::CaptionLabel(value, this);
        info->addWidget(image);
        info->addWidget(label);
        (processing ? processingInfo_ : finishedInfo_).append(image);
        (processing ? processingInfo_ : finishedInfo_).append(label);
        return label;
    };
    addInfo(qfw::FluentIconEnum::Tag, statusText(status_), false);
    statusLabel_ = qobject_cast<qfw::CaptionLabel*>(finishedInfo_.takeLast());
    finishedInfo_.takeLast(); // status icon always remains visible
    if (kind == PreviewTaskKind::FFmpeg) {
        info->addSpacing(5);
        addInfo(qfw::FluentIconEnum::BookShelf, QStringLiteral("0MB"), true);
        info->addSpacing(5);
        addInfo(qfw::FluentIconEnum::StopWatch, QStringLiteral("0.0s"), true);
        info->addSpacing(5);
        addInfo(qfw::FluentIconEnum::IOT, QStringLiteral("0kbits/s"), true);
        info->addSpacing(5);
        addInfo(qfw::FluentIconEnum::SpeedHigh, QStringLiteral("0x"), true);
        sizeLabel_ = qobject_cast<qfw::CaptionLabel*>(processingInfo_.at(1));
        timeLabel_ = qobject_cast<qfw::CaptionLabel*>(processingInfo_.at(3));
        bitrateLabel_ = qobject_cast<qfw::CaptionLabel*>(processingInfo_.at(5));
        speedLabel_ = qobject_cast<qfw::CaptionLabel*>(processingInfo_.at(7));
    }
    info->addStretch();
    auto* clock = new qfw::IconWidget(qfw::FluentIcon(qfw::FluentIconEnum::Calendar), this);
    clock->setFixedSize(16, 16);
    finishTimeLabel_ = new qfw::CaptionLabel(this);
    info->addWidget(clock);
    info->addWidget(finishTimeLabel_);
    finishedInfo_.append(clock);
    finishedInfo_.append(finishTimeLabel_);
    details->addLayout(info);
    progressBar_ = new qfw::ProgressBar(this);
    details->addWidget(progressBar_);
    row->addLayout(details, 1);
    row->addSpacing(20);

    folderButton_ = button(qfw::FluentIconEnum::Folder, Text::instance().ShowInFolder, this);
    cancelButton_ = button(qfw::FluentIconEnum::Close, Text::instance().CancelTask, this);
    retryButton_ = button(qfw::FluentIconEnum::Sync, Text::instance().RetryTask, this);
    logButton_ = button(qfw::FluentIconEnum::CommandPrompt, Text::instance().ViewLog, this);
    deleteButton_ = button(qfw::FluentIconEnum::Delete, Text::instance().RemoveTask, this);
    for (auto* action : {folderButton_, cancelButton_, retryButton_, logButton_, deleteButton_})
        row->addWidget(action);
    QObject::connect(checkBox_, &QCheckBox::toggled, this, [this](bool checked) {
        emit checkedChanged(checked);
        update();
    });
    QObject::connect(this, &qfw::CardWidget::clicked, this, [this]() {
        if (!selectionMode_) setSelectionMode(true);
        setChecked(!isChecked());
    });
    QObject::connect(folderButton_, &QToolButton::clicked, this, &TaskCard::openFolder);
    QObject::connect(cancelButton_, &QToolButton::clicked, this, &TaskCard::cancelTask);
    QObject::connect(retryButton_, &QToolButton::clicked, this, &TaskCard::retryTask);
    QObject::connect(logButton_, &QToolButton::clicked, this, &TaskCard::showLog);
    QObject::connect(deleteButton_, &QToolButton::clicked, this, &TaskCard::deleteTask);
    updateStatus(status_);
    updateInfoVisible(false);
}

bool TaskCard::isChecked() const { return checkBox_->isChecked(); }
void TaskCard::setChecked(bool checked) { checkBox_->setChecked(checked); }
void TaskCard::setSelectionMode(bool enabled) {
    selectionMode_ = enabled;
    checkBox_->setVisible(enabled);
    if (!enabled) setChecked(false);
    update();
}

void TaskCard::updateTask(int progress, TaskStatus status, const QString& size,
                          double time, const QString& bitrate, double speed) {
    progressBar_->setValue(progress);
    updateStatus(status);
    updateInfo(size, time, bitrate, speed);
}

void TaskCard::updateStatus(TaskStatus status) {
    status_ = status;
    QString text = statusText(status);
    // two-pass 阶段文案仅在压制中显示
    if (status == TaskStatus::Processing && !stageText_.isEmpty())
        text = text + QStringLiteral(" · ") + stageText_;
    statusLabel_->setText(text);
    switch (status) {
    case TaskStatus::Waiting:
    case TaskStatus::Pending:
        folderButton_->setVisible(false);
        cancelButton_->setVisible(false);
        retryButton_->setVisible(false);
        logButton_->setVisible(false);
        deleteButton_->setVisible(true);
        updateInfoVisible(false);
        break;
    case TaskStatus::Processing:
        folderButton_->setVisible(false);
        cancelButton_->setVisible(true);
        cancelButton_->setEnabled(true);
        retryButton_->setVisible(false);
        logButton_->setVisible(false);
        deleteButton_->setVisible(false);
        updateInfoVisible(true);
        break;
    case TaskStatus::Cancelling:
        folderButton_->setVisible(false);
        cancelButton_->setVisible(true);
        cancelButton_->setEnabled(false);
        retryButton_->setVisible(false);
        logButton_->setVisible(false);
        deleteButton_->setVisible(false);
        updateInfoVisible(false);
        break;
    case TaskStatus::Cancelled:
        folderButton_->setVisible(false);
        cancelButton_->setVisible(false);
        cancelButton_->setEnabled(true);
        retryButton_->setVisible(true);
        logButton_->setVisible(false);
        deleteButton_->setVisible(true);
        updateInfoVisible(false);
        break;
    case TaskStatus::Failed:
        folderButton_->setVisible(false);
        cancelButton_->setVisible(false);
        retryButton_->setVisible(true);
        logButton_->setVisible(true);
        deleteButton_->setVisible(true);
        updateInfoVisible(false);
        break;
    case TaskStatus::Succeeded:
        folderButton_->setVisible(true);
        cancelButton_->setVisible(false);
        retryButton_->setVisible(false);
        logButton_->setVisible(false);
        deleteButton_->setVisible(true);
        updateInfoVisible(false);
        break;
    }
    emit statusChanged();
}

void TaskCard::updateInfo(const QString& size, double time, const QString& bitrate,
                          double speed) {
    if (!sizeLabel_) return;  // 仅压制卡片有实时信息标签
    sizeLabel_->setText(size);
    timeLabel_->setText(pythonFloat(time) + QStringLiteral("s"));
    bitrateLabel_->setText(bitrate);
    speedLabel_->setText(pythonFloat(speed) + QStringLiteral("x"));
}

void TaskCard::updateInfoVisible(bool visible) {
    for (auto* item : processingInfo_) item->setVisible(visible);
    for (auto* item : finishedInfo_) item->setVisible(!visible);
    if (!visible)
        finishTimeLabel_->setText(QDateTime::currentDateTime().toString(
            QStringLiteral("yyyy-MM-dd hh:mm:ss")));
}

void TaskCard::paintEvent(QPaintEvent* event) {
    qfw::CardWidget::paintEvent(event);
    if (!selectionMode_ || !isChecked()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
    painter.setBrush(QColor(0, 159, 170, 15));
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);
}

void TaskCard::openFolder() {
    showInFolder(task_->outputPath);
}

void TaskCard::cancelTask() {
    emit GlobalEventBus::instance().cancelTaskSig(task_->taskId);
}

void TaskCard::retryTask() {
    emit GlobalEventBus::instance().retryTaskSig(task_->taskId);
}

void TaskCard::showLog() {
    if (!task_->logPath.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(task_->logPath));
}

void TaskCard::deleteTask() {
    bool deleteFiles = false;
    if (confirmTaskDeletion(window(), Text::instance().DeleteTask,
                            Text::instance().ConfirmDeleteTask, &deleteFiles))
        emit GlobalEventBus::instance().deleteTaskSig(task_->taskId, deleteFiles);
}
} // namespace fkw
