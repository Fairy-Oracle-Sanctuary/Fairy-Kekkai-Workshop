#include "components/project_card.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QHBoxLayout>
#include <QUrl>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"

namespace fkw
{
    TopButtonCard::TopButtonCard(QWidget *parent) : qfw::SimpleCardWidget(parent)
    {
        newProjectButton = new qfw::PushButton(trText("新建项目"), this);
        importProjectButton = new qfw::PushButton(trText("导入项目"), this);
        newFromPlaylistButton = new qfw::PushButton(trText("根据视频列表创建项目"), this);
        refreshButton = new qfw::PrimaryPushButton(trText("刷新项目列表"), this);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 15, 20, 15);
        layout->setSpacing(15);
        layout->addWidget(newProjectButton, 1);
        layout->addWidget(importProjectButton, 1);
        layout->addWidget(newFromPlaylistButton, 2);
        layout->addStretch();
        layout->addWidget(refreshButton, 1);
        setFixedHeight(70);
    }

    ProjectCard::ProjectCard(const QString &title, const QString &content, const QString &path,
                             const QIcon &icon, bool linked, QWidget *parent)
        : qfw::CardWidget(parent), path_(path)
    {
        setMinimumHeight(73);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *iconWidget = new qfw::IconWidget(icon, this);
        iconWidget->setFixedSize(48, 48);
        auto *titleLabel = new qfw::BodyLabel(wrapLongLatinRuns(title), this);
        auto *contentLabel = new qfw::CaptionLabel(wrapLongLatinRuns(content), this);
        titleLabel_ = titleLabel;
        contentLabel_ = contentLabel;
        QSizePolicy textPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        textPolicy.setHeightForWidth(true);
        titleLabel->setWordWrap(true);
        contentLabel->setWordWrap(true);
        titleLabel->setTextFormat(Qt::PlainText);
        contentLabel->setTextFormat(Qt::PlainText);
        titleLabel->setSizePolicy(textPolicy);
        contentLabel->setSizePolicy(textPolicy);
        titleLabel->setToolTip(title);
        contentLabel->setToolTip(content);
        contentLabel->setTextColor(QColor(QStringLiteral("#606060")), QColor(QStringLiteral("#d2d2d2")));
        auto *open = new qfw::PrimaryPushButton(trText("打开项目"), this);
        open->setFixedWidth(120);
        openButton_ = open;
        auto *edit = new qfw::TransparentToolButton(qfw::FluentIconEnum::Edit, this);
        auto *more = new qfw::TransparentToolButton(qfw::FluentIconEnum::More, this);
        healthBadge_ = new qfw::InfoBadge(trText("待修复"), this, qfw::InfoLevel::Warning);
        healthBadge_->setVisible(false);
        repairButton_ = new qfw::PushButton(trText("一键修复"), this);
        repairButton_->setFixedWidth(96);
        repairButton_->setVisible(false);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(20, 11, 11, 11);
        layout->setSpacing(15);
        layout->addWidget(iconWidget, 0, Qt::AlignVCenter);
        auto *labels = new QVBoxLayout();
        labelsLayout_ = labels;
        labels->setSpacing(0);
        labels->setAlignment(Qt::AlignVCenter);
        labels->addWidget(titleLabel, 0, Qt::AlignVCenter);
        labels->addWidget(contentLabel, 0, Qt::AlignVCenter);
        layout->addLayout(labels, 1);
        if (linked)
        {
            auto *link = new qfw::TransparentToolButton(qfw::FluentIconEnum::Link, this);
            link->setToolTip(path);
            layout->addWidget(link, 0, Qt::AlignVCenter);
        }
        layout->addWidget(healthBadge_, 0, Qt::AlignVCenter);
        layout->addWidget(repairButton_, 0, Qt::AlignVCenter);
        layout->addWidget(open, 0, Qt::AlignVCenter);
        layout->addWidget(edit, 0, Qt::AlignVCenter);
        layout->addWidget(more, 0, Qt::AlignVCenter);
        connect(open, &QPushButton::clicked, this, [this]()
                { emit openProject(path_); });
        connect(repairButton_, &QPushButton::clicked, this, [this]()
                { emit repairProject(path_); });
        connect(edit, &QPushButton::clicked, this, [this]()
                { emit editProject(path_); });
        connect(more, &QPushButton::clicked, this, [this, more, linked]()
                {
        auto* menu = new qfw::RoundMenu(QString(), this);
        auto* openFolder = new QAction(trText("打开项目路径"), menu);
        auto* pin = new QAction(trText("置顶项目"), menu);
        auto* remove = new QAction(linked ? trText("解除项目连接")
                                           : trText("永久删除项目"), menu);
        menu->addAction(openFolder);
        menu->addAction(pin);
        menu->addSeparator();
        menu->addAction(remove);
        // 菜单项的槽不能直接在菜单自己的事件分发栈里跑。RoundMenu 是在内部
        // QListWidget 的鼠标释放处理中 hideMenu()/close()（顺带 deleteLater 自己），
        // 然后才 action->trigger()。回调里只要弹出模态对话框（解除连接、编辑项目），
        // 就会开出嵌套事件循环，菜单和它的视图会在这个嵌套循环里被销毁；等嵌套
        // 循环退出、Qt 的视图调用帧继续执行时访问到的就是已释放内存，直接崩在
        // Qt6Widgetsd.dll（访问违例）。统一延后一个事件循环，等菜单关闭、事件栈
        // 完全退出之后再发信号。
        connect(openFolder, &QAction::triggered, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                if (!QDir(path_).exists()) {
                    NotificationService::error(Text::instance().Error,
                        formatText(Text::instance().PathDoesNotExist, {path_}), this);
                } else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path_))) {
                    NotificationService::error(Text::instance().Error,
                        formatText(Text::instance().CannotOpenFile, {path_}), this);
                }
            });
        });
        connect(pin, &QAction::triggered, this, [this]() {
            QTimer::singleShot(0, this, [this]() { emit moveToTop(path_); });
        });
        connect(remove, &QAction::triggered, this, [this, linked]() {
            QTimer::singleShot(0, this, [this, linked]() { emit removeProject(path_, linked); });
        });
        connect(menu, &qfw::RoundMenu::closedSignal, menu, &QObject::deleteLater);
        menu->execAt(more->mapToGlobal(QPoint(0, more->height()))); });
    }
    void ProjectCard::setHealth(int level, int issueCount, bool blocked)
    {
        const bool damaged = level != 0;
        if (healthBadge_)
        {
            if (damaged)
            {
                healthBadge_->setText(level == 2
                    ? trText("项目失联")
                    : blocked ? trText("待修复 %1 项").arg(issueCount)
                              : trText("可优化 %1 项").arg(issueCount));
                healthBadge_->setLevel(level == 2 ? qfw::InfoLevel::Error
                                      : blocked ? qfw::InfoLevel::Warning
                                                : qfw::InfoLevel::Attention);
                healthBadge_->setVisible(true);
            }
            else
            {
                healthBadge_->setVisible(false);
            }
        }
        if (repairButton_)
            repairButton_->setVisible(damaged);
        if (openButton_)
        {
            openButton_->setEnabled(!blocked);
            openButton_->setToolTip(level == 2 ? trText("项目目录已不存在")
                                    : blocked ? trText("项目存在异常，请先修复后再打开")
                                              : QString());
        }
        updateCardHeight();
    }
    void ProjectCard::resizeEvent(QResizeEvent *event)
    {
        qfw::CardWidget::resizeEvent(event);
        if (heightUpdateScheduled_)
            return;
        heightUpdateScheduled_ = true;
        QTimer::singleShot(0, this, [this]()
                           {
        heightUpdateScheduled_ = false;
        updateCardHeight(); });
    }
    void ProjectCard::updateCardHeight()
    {
        auto* row = qobject_cast<QHBoxLayout*>(layout());
        if (!row || !titleLabel_ || !contentLabel_ || !labelsLayout_) return;
        const QMargins margins = row->contentsMargins();
        int occupied = margins.left() + margins.right()
            + row->spacing() * (row->count() - 1);
        for (int i = 0; i < row->count(); ++i) {
            auto* item = row->itemAt(i);
            if (item->layout() == labelsLayout_) continue;
            if (auto* control = item->widget()) {
                if (control->isHidden()) continue;
                occupied += control->sizeHint().width();
            }
        }
        const int textWidth = qMax(20, width() - occupied);
        const auto textHeight = [textWidth](const QLabel* label) {
            const int measured = label->fontMetrics().boundingRect(
                QRect(0, 0, textWidth, 100000), Qt::TextWordWrap, label->text()).height();
            return qMax(label->fontMetrics().height(), measured);
        };
        const int titleHeight = textHeight(titleLabel_);
        const int contentHeight = textHeight(contentLabel_);
        titleLabel_->setMinimumHeight(titleHeight);
        contentLabel_->setMinimumHeight(contentHeight);
        const int required = qMax(73, 22 + qMax(48, titleHeight + contentHeight));
        if (minimumHeight() != required) setMinimumHeight(required);
    }
    void ProjectCard::mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton)
            dragStart_ = event->pos();
        qfw::CardWidget::mousePressEvent(event);
    }
    void ProjectCard::mouseMoveEvent(QMouseEvent *event)
    {
        if (!(event->buttons() & Qt::LeftButton) ||
            (event->pos() - dragStart_).manhattanLength() < QApplication::startDragDistance())
        {
            qfw::CardWidget::mouseMoveEvent(event);
            return;
        }
        auto *drag = new QDrag(this);
        auto *mime = new QMimeData();
        mime->setData("application/x-fkw-project-path", path_.toUtf8());
        drag->setMimeData(mime);
        setClickEnabled(false);
        drag->exec(Qt::MoveAction);
        setClickEnabled(true);
    }
}
