#include "components/base_task_interface.h"

#include <QAction>
#include <QFile>
#include <QJsonValue>
#include <QResizeEvent>
#include <QShowEvent>
#include <functional>
#include "common/app_data.h"
#include "common/config.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/empty_status_widget.h"
#include "components/notification_service.h"

namespace fkw {
BaseTaskInterface::BaseTaskInterface(PreviewTaskKind kind,
                                     const ConfigKeys::Key* concurrentItem,
                                     QWidget* parent)
    : qfw::ScrollArea(parent), kind_(kind), taskPool_(this) {
    // 默认串行（对齐 Python 旧 BaseTaskInterface 的 max_concurrent_tasks=1）；
    // 传入 cfg 配置项则按用户设置
    if (concurrentItem) {
        concurrentGroup_ = QLatin1String(concurrentItem->group);
        concurrentName_ = QLatin1String(concurrentItem->name);
        taskPool_.setMaxThreadCount(
            AppConfig::instance().value(*concurrentItem).toInt());
    } else {
        taskPool_.setMaxThreadCount(1);
    }

    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    setWidget(view);
    enableTransparentBackground();
    segmented_ = new qfw::SegmentedWidget(view);
    segmented_->addItem(QStringLiteral("allTab"), Text::instance().All,
                        [this](bool) { filterTasks(QStringLiteral("all")); });
    segmented_->addItem(QStringLiteral("processingTab"), Text::instance().Processing,
                        [this](bool) { filterTasks(QStringLiteral("processing")); });
    segmented_->addItem(QStringLiteral("completedTab"), Text::instance().TextAuto005,
                        [this](bool) { filterTasks(QStringLiteral("completed")); });
    segmented_->addItem(QStringLiteral("failedTab"), Text::instance().Failed3,
                        [this](bool) { filterTasks(QStringLiteral("failed")); });
    segmented_->setCurrentItem(QStringLiteral("allTab"));
    segmented_->setMaximumHeight(30);
    layout->addWidget(segmented_);
    region_ = new QStackedWidget(view);
    auto* emptyPage = new QWidget(region_);
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    empty_ = new EmptyStatusWidget(Text::instance().NoTasks, emptyPage);
    emptyLayout->addStretch();
    emptyLayout->addWidget(empty_, 0, Qt::AlignHCenter);
    emptyLayout->addStretch();
    region_->addWidget(emptyPage);
    auto* container = new QWidget(region_);
    taskList_ = new QVBoxLayout(container);
    taskList_->setAlignment(Qt::AlignTop);
    region_->addWidget(container);
    region_->setCurrentIndex(0);
    layout->addWidget(region_, 1);

    commandView_ = new qfw::CommandBarView(this);
    commandView_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    commandView_->setIconSize(QSize(18, 18));
    auto makeAction = [this](qfw::FluentIconEnum icon, const QString& text,
                             const std::function<void()>& handler) {
        auto* action = new QAction(qfw::FluentIcon(icon).qicon(), text, commandView_);
        QObject::connect(action, &QAction::triggered, this,
                         [handler](bool) { handler(); });
        commandView_->addAction(action);
    };
    makeAction(qfw::FluentIconEnum::Update, Text::instance().RetryAction,
               [this]() { restartSelected(); });
    makeAction(qfw::FluentIconEnum::Delete, Text::instance().DeleteAction,
               [this]() { removeSelected(); });
    commandView_->addSeparator();
    makeAction(qfw::FluentIconEnum::CheckBox, Text::instance().SelectAll,
               [this]() { for (auto* card : cards_) card->setChecked(true); });
    makeAction(qfw::FluentIconEnum::ClearSelection, Text::instance().FWCancelSelect,
               [this]() { setSelectionMode(false); });
    commandView_->resizeToSuitableWidth();
    commandView_->hide();

    auto& bus = GlobalEventBus::instance();
    QObject::connect(&bus, &GlobalEventBus::updateTaskStatusSig, this,
                     &BaseTaskInterface::updateTaskStatus);
    QObject::connect(&bus, &GlobalEventBus::finishTaskSig, this,
                     &BaseTaskInterface::handleTaskFinished);
    QObject::connect(&bus, &GlobalEventBus::deleteTaskSig, this,
                     &BaseTaskInterface::handleTaskDeleted);
    QObject::connect(&bus, &GlobalEventBus::cancelTaskSig, this,
                     &BaseTaskInterface::handleCancelTask);
    QObject::connect(&bus, &GlobalEventBus::retryTaskSig, this,
                     &BaseTaskInterface::handleRetryTask);
    if (concurrentItem) {
        QObject::connect(&AppConfig::instance(), &AppConfig::valueChanged, this,
                         [this](const QString& group, const QString& key,
                                const QJsonValue& value) {
            if (group == concurrentGroup_ && key == concurrentName_)
                updateMaxConcurrentTasks(value);
        });
    }
}

BaseTaskInterface::~BaseTaskInterface() { stopAll(); }

std::shared_ptr<TaskBase> BaseTaskInterface::createTask(const QString& input,
                                                        const QString& output) {
    // 未移植 service 的兜底：任务卡片会出现但 Worker 为 nullptr，一直停在等待中
    return std::make_shared<TaskBase>(input, output);
}

TaskWorker* BaseTaskInterface::createWorker(const std::shared_ptr<TaskBase>&) {
    return nullptr;  // 该 service 尚未移植，任务保持等待
}

QString BaseTaskInterface::getTaskPath(const std::shared_ptr<TaskBase>& task) const {
    return task->inputPath;
}

QString BaseTaskInterface::taskTypeText() const { return Text::instance().Task2; }

QString BaseTaskInterface::logName() const { return {}; }

void BaseTaskInterface::emitLegacyFinished(bool, TaskCard*) {}

QStringList BaseTaskInterface::taskGeneratedFiles(
    const std::shared_ptr<TaskBase>& task) const {
    return task->outputPath.isEmpty() ? QStringList{} : QStringList{task->outputPath};
}

void BaseTaskInterface::addTask(const QString& input, const QString& output) {
    auto task = createTask(input, output);
    if (!task) return;
    const QString taskPath = getTaskPath(task);
    if (inputPaths_.contains(taskPath)) {
        emit returnTask(true, taskPaths_, true);
        return;
    }
    inputPaths_.insert(taskPath);
    taskPaths_.append(taskPath);
    emit returnTask(false, taskPaths_, true);

    auto* card = new TaskCard(task, kind_, region_->widget(1));
    QObject::connect(card, &TaskCard::checkedChanged, this,
                     [this](bool checked) { updateSelection(checked); });
    QObject::connect(card, &TaskCard::statusChanged, this,
                     [this]() { filterTasks(filter_); });
    if (selectionMode_) card->setSelectionMode(true);
    taskList_->insertWidget(0, card, 0, Qt::AlignTop);
    cards_.prepend(card);
    cardMap_.insert(task->taskId, card);
    if (TaskWorker* worker = createWorker(task)) {
        // autoDelete：run() 结束后由线程池回收；QPointer 自动置空防悬挂
        threadMap_.insert(task->taskId, worker);
        taskPool_.start(worker);
    }
    emit GlobalEventBus::instance().taskCountChanged(cards_.size());
    filterTasks(QStringLiteral("all"));
}

void BaseTaskInterface::stopAll() {
    for (auto it = threadMap_.cbegin(); it != threadMap_.cend(); ++it)
        if (TaskWorker* worker = it.value()) worker->cancel();
    threadMap_.clear();
    taskPool_.clear();
}

void BaseTaskInterface::updateMaxConcurrentTasks(const QJsonValue& value) {
    taskPool_.setMaxThreadCount(value.toInt());
}

void BaseTaskInterface::removeCard(TaskCard* card, bool deleteFile) {
    const int taskId = card->task()->taskId;
    const QString taskPath = getTaskPath(card->task());
    // 断开信号，防止 deleteLater 时触发 checkedChanged
    card->disconnect(this);
    taskList_->removeWidget(card);
    cards_.removeAll(card);
    cardMap_.remove(taskId);
    threadMap_.remove(taskId);
    inputPaths_.remove(taskPath);
    taskPaths_.removeAll(taskPath);
    // 按需删除任务输出及附加文件
    if (deleteFile) {
        for (const QString& outputFile : taskGeneratedFiles(card->task())) {
            if (outputFile.isEmpty()) continue;
            QFile::remove(outputFile);
        }
    }
    // 删除日志文件（如存在）
    if (!card->task()->logPath.isEmpty()) QFile::remove(card->task()->logPath);
    // 被删卡片如果是选中状态，手动更新计数
    if (card->isChecked()) {
        selectionCount_ = qMax(0, selectionCount_ - 1);
        if (selectionCount_ == 0) setSelectionMode(false);
    }
    card->hide();
    card->deleteLater();
    filterTasks(filter_);
    emit GlobalEventBus::instance().taskCountChanged(cards_.size());
}

void BaseTaskInterface::filterTasks(const QString& filter) {
    filter_ = filter;
    bool any = false;
    for (auto* card : cards_) {
        const bool match = filter == QStringLiteral("all") ||
            (filter == QStringLiteral("processing") && card->status() == TaskStatus::Processing) ||
            (filter == QStringLiteral("completed") && card->status() == TaskStatus::Succeeded) ||
            (filter == QStringLiteral("failed") && card->status() == TaskStatus::Failed);
        card->setVisible(match);
        any |= match;
    }
    region_->setCurrentIndex(any ? 1 : 0);
    if (!any) empty_->advanceIcon();
}

void BaseTaskInterface::updateSelection(bool) {
    if (!selectionMode_ && cards_.isEmpty()) return;
    selectionCount_ = 0;
    for (auto* card : cards_) if (card->isChecked()) ++selectionCount_;
    if (selectionCount_ > 0) setSelectionMode(true);
    else if (selectionMode_) setSelectionMode(false);
}

void BaseTaskInterface::setSelectionMode(bool enabled) {
    if (selectionMode_ == enabled) return;
    selectionMode_ = enabled;
    for (auto* card : cards_) card->setSelectionMode(enabled);
    commandView_->setVisible(enabled);
    if (enabled) {
        commandView_->raise();
        commandView_->move((width() - commandView_->width()) / 2,
                           height() - commandView_->sizeHint().height() - 20);
    } else selectionCount_ = 0;
}

bool BaseTaskInterface::isIdleCard(TaskCard* card) const {
    // 是否可删除/可重试（非进行中的任务）
    switch (card->status()) {
    case TaskStatus::Waiting:
    case TaskStatus::Succeeded:
    case TaskStatus::Failed:
    case TaskStatus::Cancelled:
        return true;
    default:
        return false;
    }
}

void BaseTaskInterface::removeSelected() {
    bool deleteFiles = false;
    if (confirmTaskDeletion(window(), Text::instance().DeleteTask,
                            Text::instance().ConfirmDeleteTask, &deleteFiles)) {
        const auto selected = cards_;
        for (auto* card : selected)
            if (card->isChecked() && isIdleCard(card)) removeCard(card, deleteFiles);
    }
    setSelectionMode(false);
}

void BaseTaskInterface::restartSelected() {
    for (auto* card : cards_)
        if (card->isChecked() && isIdleCard(card))
            emit GlobalEventBus::instance().retryTaskSig(card->task()->taskId);
}

void BaseTaskInterface::handleTaskDeleted(int taskId, bool deleteFile) {
    const auto snapshot = cards_;
    for (auto* card : snapshot) {
        if (card->task()->taskId == taskId) {
            removeCard(card, deleteFile);
            break;
        }
    }
}

void BaseTaskInterface::handleCancelTask(int taskId) {
    TaskCard* card = cardMap_.value(taskId);
    if (!card || card->status() == TaskStatus::Cancelled ||
        card->status() == TaskStatus::Cancelling)
        return;
    card->updateTask(0, TaskStatus::Cancelling);
    if (TaskWorker* worker = threadMap_.take(taskId)) worker->cancel();
    card->updateTask(0, TaskStatus::Cancelled);
}

void BaseTaskInterface::handleRetryTask(int taskId) {
    TaskCard* card = cardMap_.value(taskId);
    if (!card) return;
    const auto task = card->task();
    // Worker 在内部重建命令与探测（线程内完成，不阻塞主线程）
    card->updateTask(0, TaskStatus::Waiting);
    if (TaskWorker* worker = createWorker(task)) {
        threadMap_.insert(taskId, worker);
        taskPool_.start(worker);
    }
}

void BaseTaskInterface::updateTaskStatus(int taskId, int progress,
                                         const QVariant& status, const QString& size,
                                         double time, const QString& bitrate,
                                         double speed) {
    TaskCard* card = cardMap_.value(taskId);
    if (card)
        card->updateTask(progress, static_cast<TaskStatus>(status.toInt()), size,
                         time, bitrate, speed);
}

void BaseTaskInterface::handleTaskFinished(int taskId, bool success,
                                           const QString& logPath) {
    Q_UNUSED(logPath);
    TaskCard* card = cardMap_.value(taskId);
    if (!card || card->status() == TaskStatus::Cancelled ||
        card->status() == TaskStatus::Cancelling)
        return;
    card->updateTask(success ? 100 : 0,
                     success ? TaskStatus::Succeeded : TaskStatus::Failed);
    // 任务完成/失败写入日志框（子类通过 logName 开启通道）
    const QString logChannel = logName();
    if (!logChannel.isEmpty()) {
        emit GlobalEventBus::instance().taskLogSignal(
            logChannel,
            success ? Text::instance().TaskDone : Text::instance().TaskFailedLog,
            !success, false);
    }
    if (success) {
        NotificationService::success(
            Text::instance().Success,
            formatText(Text::instance().Completed,
                       {card->task()->outputName, taskTypeText()}));
    } else {
        NotificationService::error(
            Text::instance().Failed,
            formatText(Text::instance().TaskFailed,
                       {card->task()->fileName, taskTypeText(), QString()}));
    }
    // 兼容旧功能通知通道（托盘等），子类可覆盖
    emitLegacyFinished(success, card);
}

void BaseTaskInterface::resizeEvent(QResizeEvent* event) {
    qfw::ScrollArea::resizeEvent(event);
    commandView_->move((width() - commandView_->width()) / 2,
                       height() - commandView_->sizeHint().height() - 20);
}

void BaseTaskInterface::showEvent(QShowEvent* event) {
    qfw::ScrollArea::showEvent(event);
    if (segmented_) segmented_->setCurrentItem(QStringLiteral("allTab"));
    if (region_) filterTasks(QStringLiteral("all"));
}
} // namespace fkw
