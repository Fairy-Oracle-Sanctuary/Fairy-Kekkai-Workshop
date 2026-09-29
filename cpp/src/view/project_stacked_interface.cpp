#include "view/project_stacked_interface.h"

#include <QFileInfo>
#include <QShowEvent>
#include <QVBoxLayout>

#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"
#include "view/project_detail_interface.h"
#include "view/project_interface.h"

namespace fkw {
ProjectStackedInterface::ProjectStackedInterface(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("ProjectStackedInterface"));
    stacked_ = new QStackedWidget(this);
    projects_ = new ProjectInterface(stacked_);
    detail_ = new ProjectDetailInterface(stacked_);
    stacked_->addWidget(projects_);
    stacked_->addWidget(detail_);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stacked_);
    connect(projects_, &ProjectInterface::openProjectDetail, this, [this](const QString& path) {
        detail_->loadProject(path);
        stacked_->setCurrentWidget(detail_);
    });
    connect(detail_, &ProjectDetailInterface::projectLoaded, this, [this](const QString& path) {
        NotificationService::success(Text::instance().OpenedSuccessfully,
            formatText(Text::instance().ProjectOpened, {QFileInfo(path).fileName()}), this);
    });
    connect(detail_, &ProjectDetailInterface::backToProjectList, this, [this]() {
        projects_->refreshProjectList();
        stacked_->setCurrentWidget(projects_);
    });
}

void ProjectStackedInterface::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (stacked_->currentWidget() == detail_)
        detail_->reloadCurrentProject();
    else
        projects_->refreshProjectList();
}
}
