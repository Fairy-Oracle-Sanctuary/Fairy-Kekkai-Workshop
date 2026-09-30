#include "view/project_stacked_interface.h"

#include <QDir>
#include <QFileInfo>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"
#include "view/project_detail_interface.h"
#include "view/project_interface.h"

namespace fkw {
namespace {
const QString kListKey = QStringLiteral("project-list");
QString normalizedProjectPath(const QString& path) {
    const QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty()) result = info.absoluteFilePath();
    result = QDir::cleanPath(QDir::fromNativeSeparators(result));
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
}

ProjectStackedInterface::ProjectStackedInterface(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ProjectStackedInterface"));
    tabBar_ = new qfw::TabBar(this);
    tabBar_->setObjectName(QStringLiteral("tutorial-project-tabs"));
    tabBar_->setMovable(true);
    tabBar_->setScrollable(true);
    tabBar_->setTabMaximumWidth(220);
    tabBar_->setTabMinimumWidth(100);
    tabBar_->setAddButtonVisible(true);
    stacked_ = new QStackedWidget(this);
    projects_ = new ProjectInterface(stacked_);
    stacked_->addWidget(projects_);
    pages_.insert(kListKey, projects_);
    auto* listTab = tabBar_->addTab(kListKey, Text::instance().ProjectManagement,
                                  qfw::FluentIconEnum::Folder);
    listTab->setCloseButtonDisplayMode(qfw::TabCloseButtonDisplayMode::Never);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tabBar_);
    layout->addWidget(stacked_, 1);
    connect(projects_, &ProjectInterface::openProjectDetail,
            this, &ProjectStackedInterface::openProject);
    connect(projects_, &ProjectInterface::projectChanged,
            this, &ProjectStackedInterface::updateProjectTab);
    connect(projects_, &ProjectInterface::projectRemoved,
            this, &ProjectStackedInterface::removeProjectTab);
    connect(tabBar_, &qfw::TabBar::currentChanged, this, [this](int index) {
        if (auto* tab = tabBar_->tabItem(index)) selectTab(tab->routeKey());
    });
    // TabBar 拖动时交换条目而非移动 QStackedWidget；始终按 routeKey 找页面。
    connect(tabBar_, &qfw::TabBar::tabMoved, this, [this](int, int) {
        if (auto* tab = tabBar_->currentTab()) selectTab(tab->routeKey());
    });
    connect(tabBar_, &qfw::TabBar::tabAddRequested, this, &ProjectStackedInterface::selectList);
    connect(tabBar_, &qfw::TabBar::tabCloseRequested, this, [this](int index) {
        if (auto* tab = tabBar_->tabItem(index)) {
            const QString key = tab->routeKey();
            // 不在关闭按钮自身的事件栈中删除所属标签。
            QTimer::singleShot(0, this, [this, key]() { closeTab(key); });
        }
    });
    // 设置页迁移项目库后，同步已打开页面的路径。
    connect(&GlobalEventBus::instance(), &GlobalEventBus::project_updated,
            this, [this](const QJsonObject& event) {
        const QString oldPath = event.value(QStringLiteral("old_path")).toString();
        const QString newPath = event.value(QStringLiteral("path")).toString();
        if (!oldPath.isEmpty() && !newPath.isEmpty()) updateProjectTab(oldPath, newPath);
    });
}

QString ProjectStackedInterface::keyForPath(const QString& path) const {
    const QString normalized = normalizedProjectPath(path);
    for (auto it = pages_.constBegin(); it != pages_.constEnd(); ++it) {
        auto* detail = qobject_cast<ProjectDetailInterface*>(it.value());
        if (detail && normalizedProjectPath(detail->projectPath()) == normalized)
            return it.key();
    }
    return {};
}

void ProjectStackedInterface::openProject(const QString& path) {
    const QString existing = keyForPath(path);
    if (!existing.isEmpty()) {
        selectTab(existing);
        return;
    }
    const QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto* detail = new ProjectDetailInterface(stacked_);
    stacked_->addWidget(detail);
    pages_.insert(key, detail);
    connect(detail, &ProjectDetailInterface::projectLoaded, this, [this](const QString& loaded) {
        NotificationService::success(Text::instance().OpenedSuccessfully,
            formatText(Text::instance().ProjectOpened, {QFileInfo(loaded).fileName()}), this);
    });
    connect(detail, &ProjectDetailInterface::backToProjectList, this, [this, detail]() {
        // 非当前标签的异步读取失败不能抢走用户正在查看的其它项目。
        if (stacked_->currentWidget() == detail) selectList();
    });
    auto* tab = tabBar_->addTab(key, QFileInfo(path).fileName(), qfw::FluentIconEnum::Folder);
    tab->setToolTip(QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()));
    detail->loadProject(QFileInfo(path).absoluteFilePath());
    selectTab(key);
}

void ProjectStackedInterface::selectTab(const QString& key) {
    if (auto* page = pages_.value(key, nullptr)) {
        if (page == projects_ && stacked_->currentWidget() != projects_)
            projects_->refreshProjectList();
        tabBar_->setCurrentTab(key);
        stacked_->setCurrentWidget(page);
    }
}

void ProjectStackedInterface::selectList() {
    if (stacked_->currentWidget() == projects_) projects_->refreshProjectList();
    selectTab(kListKey);
}

void ProjectStackedInterface::closeTab(const QString& key) {
    if (key == kListKey || !pages_.contains(key)) return;
    int index = -1;
    for (int i = 0; i < tabBar_->count(); ++i)
        if (tabBar_->tabItem(i)->routeKey() == key) { index = i; break; }
    if (index < 0) return;
    const QString active = tabBar_->currentTab()->routeKey();
    QWidget* page = pages_.take(key);
    // 库 removeTab 的中间信号仍带删除前索引，先屏蔽，删除后再选正确页面。
    {
        const QSignalBlocker blocker(tabBar_);
        tabBar_->removeTab(index);
    }
    stacked_->removeWidget(page);
    page->deleteLater(); // 只关页面，不取消功能页任务队列或删除项目文件。
    const QString next = active == key
        ? tabBar_->tabItem(qMin(index, tabBar_->count() - 1))->routeKey() : active;
    selectTab(next);
}

void ProjectStackedInterface::updateProjectTab(const QString& oldPath, const QString& newPath) {
    const QString key = keyForPath(oldPath);
    if (key.isEmpty()) return;
    auto* detail = qobject_cast<ProjectDetailInterface*>(pages_.value(key));
    detail->loadProject(newPath);
    if (auto* tab = tabBar_->tab(key)) {
        tab->setText(QFileInfo(newPath).fileName());
        tab->setToolTip(QDir::toNativeSeparators(newPath));
    }
}

void ProjectStackedInterface::removeProjectTab(const QString& path) {
    const QString key = keyForPath(path);
    if (!key.isEmpty()) closeTab(key);
}

void ProjectStackedInterface::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (stacked_->currentWidget() == projects_) projects_->refreshProjectList();
    else if (auto* detail = qobject_cast<ProjectDetailInterface*>(stacked_->currentWidget()))
        detail->reloadCurrentProject();
}
} // namespace fkw
