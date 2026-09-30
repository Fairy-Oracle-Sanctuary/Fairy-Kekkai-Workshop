#include "view/project_detail_interface.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <QDesktopServices>
#include <QAction>
#include <QScrollBar>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "common/app_data.h"
#include "common/event_bus.h"
#include "service/project_service.h"
#include "service/project_health.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/dialog.h"
#include "components/notification_service.h"
#include "components/file_item_widget.h"
#include "components/pager.h"

namespace fkw
{
    ProjectDetailInterface::ProjectDetailInterface(QWidget *parent) : qfw::ScrollArea(parent)
    {
        setObjectName(QStringLiteral("projectDetailInterface"));
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *view = new QWidget(this);
        layout_ = new QVBoxLayout(view);
        layout_->setContentsMargins(10, 10, 10, 10);
        layout_->setSpacing(12);
        setWidget(view);
        enableTransparentBackground();
        connect(&GlobalEventBus::instance(), &GlobalEventBus::download_finished_signal,
                this, [this](bool success, const QString& directory) {
            if (!success || path_.isEmpty()) return;
            const QString projectRoot = QDir::fromNativeSeparators(QDir(path_).absolutePath());
            const QString completedDirectory = QDir::fromNativeSeparators(
                QDir(directory).absolutePath());
            if (!completedDirectory.startsWith(projectRoot + QLatin1Char('/'),
                                               Qt::CaseInsensitive)) return;
            QTimer::singleShot(0, this, [this]() {
                if (!path_.isEmpty()) beginLoad(currentPage_);
            });
        });
    }
    void ProjectDetailInterface::loadProject(const QString &path)
    {
        if (path_ != path) {
            currentPage_ = 1;
            expandedEpisodes_.clear();
            selectedEpisodes_.clear();
            infoExpanded_ = false;
            verticalScrollBar()->setValue(0);
        }
        path_ = path;
        notifyOnLoad_ = true;
        notifyOnRefresh_ = false;
        beginLoad(currentPage_);
    }

    void ProjectDetailInterface::reloadCurrentProject(bool notify)
    {
        notifyOnLoad_ = false;
        notifyOnRefresh_ = notify;
        if (!path_.isEmpty()) beginLoad(currentPage_);
    }

    void ProjectDetailInterface::showLoadingView()
    {
        if (content_) {
            layout_->removeWidget(content_);
            content_->deleteLater();
        }
        content_ = new QWidget(widget());
        content_->setMinimumHeight(qMax(400, viewport()->height() - 20));
        auto* column = new QVBoxLayout(content_);
        column->setSpacing(20);
        column->addStretch();
        QString iconPath = QStringLiteral(":/app/images/icons/牛排.svg");
        QFile iconFile(QDir(path_).filePath(QStringLiteral("icon.txt")));
        if (iconFile.open(QIODevice::ReadOnly)) {
            const QString saved = QString::fromUtf8(iconFile.readAll()).trimmed();
            if (!saved.isEmpty()) iconPath = saved;
        }
        auto* icon = new qfw::IconWidget(QIcon(iconPath), content_);
        icon->setFixedSize(80, 80);
        column->addWidget(icon, 0, Qt::AlignHCenter);
        QSizePolicy wrapPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        wrapPolicy.setHeightForWidth(true);
        const QString projectName = QFileInfo(path_).fileName();
        auto* title = new qfw::SubtitleLabel(wrapLongLatinRuns(projectName), content_);
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        title->setAlignment(Qt::AlignCenter);
        title->setSizePolicy(wrapPolicy);
        title->setToolTip(projectName);
        column->addWidget(title, 0, Qt::AlignTop);
        auto* path = new qfw::BodyLabel(wrapLongLatinRuns(path_), content_);
        path->setTextFormat(Qt::PlainText);
        path->setWordWrap(true);
        path->setAlignment(Qt::AlignCenter);
        path->setSizePolicy(wrapPolicy);
        path->setToolTip(path_);
        column->addWidget(path, 0, Qt::AlignTop);
        auto* ring = new qfw::ProgressRing(content_, false);
        ring->setObjectName(QStringLiteral("projectLoadingProgress"));
        ring->setFixedSize(64, 64);
        ring->setRange(0, 100);
        ring->setTextVisible(true);
        ring->setValue(0);
        column->addWidget(ring, 0, Qt::AlignHCenter);
        auto* status = new qfw::BodyLabel(Text::instance().LoadingProject, content_);
        status->setObjectName(QStringLiteral("projectLoadingStatus"));
        column->addWidget(status, 0, Qt::AlignHCenter);
        column->addStretch();
        layout_->addWidget(content_);
    }

    void ProjectDetailInterface::showRepairView(const QStringList &issues)
    {
        if (content_)
        {
            layout_->removeWidget(content_);
            content_->deleteLater();
            content_ = nullptr;
        }
        content_ = new QWidget(widget());
        content_->setMinimumHeight(qMax(400, viewport()->height() - 20));
        auto *column = new QVBoxLayout(content_);
        column->setSpacing(16);
        column->addStretch();
        QString iconPath = QStringLiteral(":/app/images/icons/牛排.svg");
        QFile iconFile(QDir(path_).filePath(QStringLiteral("icon.txt")));
        if (iconFile.open(QIODevice::ReadOnly)) {
            const QString saved = QString::fromUtf8(iconFile.readAll()).trimmed();
            if (!saved.isEmpty()) iconPath = saved;
        }
        auto *icon = new qfw::IconWidget(QIcon(iconPath), content_);
        icon->setFixedSize(80, 80);
        column->addWidget(icon, 0, Qt::AlignHCenter);
        QSizePolicy wrapPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        wrapPolicy.setHeightForWidth(true);
        const QString projectName = QFileInfo(path_).fileName();
        auto *title = new qfw::SubtitleLabel(wrapLongLatinRuns(projectName), content_);
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        title->setAlignment(Qt::AlignCenter);
        title->setSizePolicy(wrapPolicy);
        title->setToolTip(projectName);
        column->addWidget(title, 0, Qt::AlignTop);
        auto *status = new qfw::BodyLabel(trText("项目存在异常，需修复后才能打开"), content_);
        status->setAlignment(Qt::AlignCenter);
        status->setWordWrap(true);
        status->setSizePolicy(wrapPolicy);
        status->setTextColor(QColor(QStringLiteral("#c42b1c")));
        column->addWidget(status, 0, Qt::AlignTop);
        column->addWidget(new qfw::BodyLabel(
            trText("检测到以下问题：\n%1").arg(issues.join(QLatin1Char('\n'))), content_),
            0, Qt::AlignHCenter);
        auto *actions = new QHBoxLayout();
        auto *repair = new qfw::PrimaryPushButton(trText("一键修复"), content_);
        auto *back = new qfw::PushButton(trText("返回项目列表"), content_);
        actions->setSpacing(10);
        actions->addStretch();
        actions->addWidget(repair);
        actions->addWidget(back);
        actions->addStretch();
        column->addLayout(actions);
        column->addStretch();
        layout_->addWidget(content_);
        connect(back, &QPushButton::clicked, this, [this]() { emit backToProjectList(); });
        connect(repair, &QPushButton::clicked, this, [this]() {
            projects::RepairReport report;
            QString error;
            if (!projects::repair(path_, &report, &error)) {
                NotificationService::error(trText("修复失败"), error, this);
                return;
            }
            if (!report.changed) {
                NotificationService::info(trText("智能修复"),
                    trText("未发现需要修改的内容"), this);
            } else {
                NotificationService::success(trText("修复完成"),
                    trText("已执行：%1").arg(report.actions.join(trText("；"))), this);
            }
            notifyOnLoad_ = false;
            notifyOnRefresh_ = false;
            beginLoad(currentPage_);
        });
    }

    void ProjectDetailInterface::beginLoad(int page)
    {
        if (path_.isEmpty()) return;
        pendingScrollPosition_ = page == currentPage_ ? verticalScrollBar()->value() : 0;
        currentPage_ = page;
        const unsigned generation = ++loadGeneration_;
        const QString path = path_;
        showLoadingView();
        auto error = std::make_shared<QString>();
        auto* worker = QThread::create([path, error]() {
            projects::Document document;
            if (!projects::read(path, &document, error.get())) return;
            QDir(path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &QThread::finished, this, [this, generation, page, path, error]() {
            if (generation != loadGeneration_) return;
            if (!error->isEmpty()) {
                notifyOnLoad_ = false;
                notifyOnRefresh_ = false;
                const projects::Health health = projects::check(path);
                if (health.blocking) {
                    QStringList issues;
                    for (const projects::Issue& issue : health.issues)
                        issues << QStringLiteral("· ") + issue.detail;
                    if (issues.isEmpty()) issues << *error;
                    showRepairView(issues);
                    return;
                }
                NotificationService::error(trText("读取项目失败"), *error, this);
                emit backToProjectList();
                return;
            }
            if (auto* ring = content_->findChild<qfw::ProgressRing*>(
                    QStringLiteral("projectLoadingProgress"))) ring->setValue(100);
            if (auto* status = content_->findChild<qfw::BodyLabel*>(
                    QStringLiteral("projectLoadingStatus")))
                status->setText(Text::instance().LoadComplete);
            QTimer::singleShot(150, this, [this, generation, page]() {
                if (generation == loadGeneration_) showPage(page);
            });
        });
        worker->start();
    }

    void ProjectDetailInterface::showPage(int page)
    {
        const int scrollPosition = pendingScrollPosition_ >= 0
            ? pendingScrollPosition_ : verticalScrollBar()->value();
        pendingScrollPosition_ = -1;
        ++loadGeneration_;
        {
            // 项目存在异常时不进入正常页面，改为展示修复入口
            const projects::Health health = projects::check(path_);
            if (health.blocking) {
                QStringList issues;
                for (const projects::Issue& issue : health.issues)
                    issues << QStringLiteral("· ") + issue.detail;
                showRepairView(issues);
                return;
            }
        }
        if (content_)
        {
            layout_->removeWidget(content_);
            content_->deleteLater();
            content_ = nullptr;
        }
        content_ = new QWidget(widget());
        auto *body = new QVBoxLayout(content_);
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(12);
        layout_->addWidget(content_);
        const QDir folder(path_);
        projects::Document document;
        QString documentError;
        if (!projects::read(path_, &document, &documentError)) {
            notifyOnLoad_ = false;
            notifyOnRefresh_ = false;
            const projects::Health health = projects::check(path_);
            if (health.level == projects::HealthLevel::Repairable) {
                QStringList issues;
                for (const projects::Issue& issue : health.issues)
                    issues << QStringLiteral("· ") + issue.detail;
                if (issues.isEmpty()) issues << documentError;
                showRepairView(issues);
                return;
            }
            NotificationService::error(trText("读取项目失败"), documentError, this);
            return;
        }
        auto entries = folder.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        entries.erase(std::remove_if(entries.begin(), entries.end(), [](const QFileInfo &item)
                                     {
        bool numeric = false;
        return item.fileName().toInt(&numeric) < 1 || !numeric; }),
                      entries.end());
        std::sort(entries.begin(), entries.end(), [](const QFileInfo &a, const QFileInfo &b)
                  { return a.fileName().toInt() < b.fileName().toInt(); });
        const auto config = readJson(QStringLiteral("config.json"));
        const int configured = config.value(QStringLiteral("Project")).toObject().value(QStringLiteral("DetailProjectItemNum")).toInt(5);
        const int perPage = std::clamp(configured, 1, 10);
        const int totalPages = std::max(1, (static_cast<int>(entries.size()) + perPage - 1) / perPage);
        currentPage_ = std::clamp(page, 1, totalPages);
        // 标题与常用操作只占一行，低频操作收进 Fluent 菜单。
        auto* header = new QHBoxLayout;
        const QString projectTitle = folder.dirName();
        auto* title = new qfw::StrongBodyLabel(projectTitle, content_);
        title->setTextFormat(Qt::PlainText);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setMinimumWidth(0);
        title->setToolTip(projectTitle);
        header->addWidget(title, 1);
        auto* batchTask = new qfw::PushButton(Text::instance().BatchAddTasks, content_);
        auto* append = new qfw::PrimaryPushButton(trText("添加新集"), content_);
        auto* more = new qfw::TransparentToolButton(qfw::FluentIconEnum::More, content_);
        more->setToolTip(Text::instance().ProjectManagement);
        header->addWidget(batchTask);
        header->addWidget(append);
        header->addWidget(more);
        body->addLayout(header);
        connect(append, &QPushButton::clicked, this,
                [this, count = entries.size()]() { addEpisode(count + 1); });
        auto* batchDelete = new qfw::PushButton(trText("批量删除"), content_);
        batchDelete->hide(); // 由菜单与选择工具栏触发，共用原删除确认流程。
        connect(more, &QPushButton::clicked, this, [this, more, batchDelete]() {
            auto* menu = new qfw::RoundMenu(QString(), more);
            const auto add = [menu, more](const QString& text, const std::function<void()>& action) {
                auto* item = new QAction(text, menu);
                menu->addAction(item);
                connect(item, &QAction::triggered, more, [more, action]() {
                    QTimer::singleShot(0, more, action);
                });
            };
            add(Text::instance().BackToProjectList, [this]() { emit backToProjectList(); });
            add(Text::instance().RefreshProjectList, [this]() { reloadCurrentProject(true); });
            add(trText("批量删除"), [batchDelete]() { batchDelete->click(); });
            connect(menu, &qfw::RoundMenu::closedSignal, menu, &QObject::deleteLater);
            menu->execAt(more->mapToGlobal(QPoint(0, more->height())));
        });
        // 五个工作流阶段汇总；OCR/Whisper/手工原文任一存在即算原文就绪。
        const auto stageStatus = [](const QDir& dir) {
            return QVector<bool>{QFileInfo::exists(dir.filePath(QStringLiteral("封面.jpg"))),
                QFileInfo::exists(dir.filePath(QStringLiteral("生肉.mp4"))),
                QFileInfo::exists(dir.filePath(QStringLiteral("原文.srt"))) ||
                    QFileInfo::exists(dir.filePath(QStringLiteral("原文_OCR.srt"))) ||
                    QFileInfo::exists(dir.filePath(QStringLiteral("原文_Whisper.srt"))),
                QFileInfo::exists(dir.filePath(QStringLiteral("译文.srt"))),
                QFileInfo::exists(dir.filePath(QStringLiteral("熟肉.mp4")))};
        };
        int complete = 0;
        for (const auto& entry : entries)
            for (bool ready : stageStatus(QDir(entry.absoluteFilePath()))) if (ready) ++complete;
        auto* overview = new QHBoxLayout;
        auto* progress = new qfw::ProgressBar(content_, false);
        progress->setFixedWidth(130);
        progress->setRange(0, 100);
        const int percent = entries.isEmpty() ? 0 : complete * 100 / (entries.size() * 5);
        progress->setValue(percent);
        overview->addWidget(progress);
        overview->addWidget(new qfw::CaptionLabel(QStringLiteral("%1% · %2").arg(percent).arg(
            formatText(Text::instance().EpisodesTotalPage,
                {QString::number(entries.size()), QString::number(currentPage_),
                 QString::number(totalPages)})), content_), 1);
        auto* infoToggle = new qfw::TransparentToolButton(qfw::FluentIconEnum::Info, content_);
        infoToggle->setCheckable(true);
        infoToggle->setChecked(infoExpanded_);
        infoToggle->setToolTip(Text::instance().Project);
        overview->addWidget(infoToggle);
        body->addLayout(overview);
        QStringList projectInfo{QDir::toNativeSeparators(path_)};
        for (const auto& marker : folder.entryInfoList({QStringLiteral("*.txt")}, QDir::Files)) {
            if (marker.fileName() == QStringLiteral("标题.txt") ||
                marker.fileName() == QStringLiteral("icon.txt")) continue;
            projectInfo << Text::instance().OriginalTitle + QStringLiteral(": ") + marker.completeBaseName();
            break;
        }
        auto* info = new qfw::BodyLabel(projectInfo.join(QLatin1Char('\n')), content_);
        info->setTextFormat(Qt::PlainText);
        info->setWordWrap(true);
        info->setTextInteractionFlags(Qt::TextSelectableByMouse);
        info->setVisible(infoExpanded_);
        body->addWidget(info);
        connect(infoToggle, &QPushButton::toggled, this, [this, info](bool expanded) {
            infoExpanded_ = expanded;
            info->setVisible(expanded);
        });
        QSizePolicy wrapPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        wrapPolicy.setHeightForWidth(true);
        connect(batchTask, &QPushButton::clicked, this,
                [this, entries, document]() {
            if (entries.isEmpty()) {
                NotificationService::warning(Text::instance().Info,
                    Text::instance().NEICP, this);
                return;
            }
            QVector<BatchTaskDialog::Episode> episodes;
            for (const auto &entry : entries) {
                const int folderNum = entry.fileName().toInt();
                if (!selectedEpisodes_.isEmpty() && !selectedEpisodes_.contains(folderNum)) continue;
                BatchTaskDialog::Episode episode{folderNum, entry.absoluteFilePath(), {}, {}};
                if (folderNum >= 1 && folderNum <= document.episodes.size()) {
                    episode.title = document.episodes.at(folderNum - 1).originalTitle;
                    episode.videoUrl = document.episodes.at(folderNum - 1).videoUrl;
                }
                episodes.append(episode);
            }
            BatchTaskDialog dialog(episodes, window(), !selectedEpisodes_.isEmpty());
            if (!dialog.exec()) return;
            const auto selected = dialog.selected();
            if (selected.isEmpty()) {
                NotificationService::warning(Text::instance().Info,
                    Text::instance().NoTasksSelected, this);
                return;
            }
            for (const auto &choice : selected) {
                QString videoUrl;
                for (const auto &episode : episodes)
                    if (episode.folderNum == choice.folderNum) {
                        videoUrl = episode.videoUrl;
                        break;
                    }
                dispatchTask(choice.taskType, choice.folderPath, videoUrl);
            }
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().TasksAdded,
                    {QString::number(selected.size())}), this);
        });
        connect(batchDelete, &QPushButton::clicked, this, [this, document]() {
            if (document.episodes.isEmpty()) {
                NotificationService::warning(Text::instance().Info,
                    Text::instance().NEICP, this);
                return;
            }
            QStringList titles;
            for (const auto& episode : document.episodes) titles << episode.originalTitle;
            BatchDeleteDialog dialog(path_, titles, window(), selectedEpisodes_);
            if (!dialog.exec()) return;
            const QStringList selected = dialog.selectedPaths();
            if (selected.isEmpty()) {
                NotificationService::warning(Text::instance().Info,
                    Text::instance().NoFilesSelected, this);
                return;
            }
            qfw::MessageDialog confirm(trText("确认批量删除"),
                QStringLiteral("确定删除选中的 %1 个文件？").arg(selected.size()), window());
            if (!confirm.exec()) return;
            QStringList failed;
            for (const QString& path : selected)
                if (!QFile::remove(path)) failed << path;
            showPage(currentPage_);
            if (failed.isEmpty())
                NotificationService::success(Text::instance().Success,
                    formatText(Text::instance().Deleted,
                        {QString::number(selected.size()),
                         QFileInfo(selected.first()).fileName()}), this);
            else NotificationService::error(Text::instance().Error,
                failed.join(QLatin1Char('\n')), this);
        });
        QSet<int> present;
        for (const auto& entry : entries) present.insert(entry.fileName().toInt());
        selectedEpisodes_.intersect(present);
        expandedEpisodes_.intersect(present);
        auto* selectPage = new qfw::TransparentPushButton(Text::instance().SelectAll, content_);
        body->addWidget(selectPage, 0, Qt::AlignLeft);
        connect(selectPage, &QPushButton::clicked, this, [this]() {
            for (auto* choice : content_->findChildren<qfw::CheckBox*>(QStringLiteral("episodeSelection")))
                choice->setChecked(true);
        });
        auto* selectionBar = new QWidget(content_);
        auto* selectionLayout = new QHBoxLayout(selectionBar);
        selectionLayout->setContentsMargins(0, 0, 0, 0);
        auto* selectionCount = new qfw::CaptionLabel(selectionBar);
        selectionLayout->addWidget(selectionCount, 1);
        auto* selectedTasks = new qfw::PushButton(Text::instance().BatchAddTasks, selectionBar);
        auto* selectedDelete = new qfw::PushButton(trText("批量删除"), selectionBar);
        auto* clearSelection = new qfw::TransparentPushButton(Text::instance().DeselectAll, selectionBar);
        selectionLayout->addWidget(selectedTasks);
        selectionLayout->addWidget(selectedDelete);
        selectionLayout->addWidget(clearSelection);
        body->addWidget(selectionBar);
        const auto updateSelection = [this, selectionBar, selectionCount, batchTask]() {
            selectionBar->setVisible(!selectedEpisodes_.isEmpty());
            selectionCount->setText(QStringLiteral("✓ %1").arg(selectedEpisodes_.size()));
            batchTask->setVisible(selectedEpisodes_.isEmpty());
        };
        updateSelection();
        connect(selectedTasks, &QPushButton::clicked, batchTask, &QPushButton::click);
        connect(selectedDelete, &QPushButton::clicked, batchDelete, &QPushButton::click);
        // 只取消分集选择，保持分页和展开状态。
        connect(clearSelection, &QPushButton::clicked, this, [this]() {
            selectedEpisodes_.clear();
            showPage(currentPage_);
        });
        auto addPager = [this, body, totalPages]() {
            if (totalPages < 2) return;
            auto* row = new QHBoxLayout;
            row->addStretch();
            auto* pager = new Pager(totalPages, std::min(totalPages, 5), content_);
            pager->setCurrentPage(currentPage_);
            connect(pager, &Pager::currentPageChanged, this, [this](int page) {
                if (page != currentPage_) beginLoad(page);
            });
            row->addWidget(pager);
            row->addStretch();
            body->addLayout(row);
        };
        const int start = (currentPage_ - 1) * perPage;
        const int end = std::min(start + perPage, static_cast<int>(entries.size()));
        for (int i = start; i < end; ++i)
        {
            const auto &entry = entries.at(i);
            const int number = entry.fileName().toInt();
            const projects::Episode data = number <= document.episodes.size()
                ? document.episodes.at(number - 1) : projects::Episode{};
            const QString episode = formatText(Text::instance().Episode,
                {QString::number(number), data.originalTitle});
            auto* episodeGroup = new qfw::SimpleCardWidget(content_);
            auto* episodeLayout = new QVBoxLayout(episodeGroup);
            episodeLayout->setContentsMargins(10, 6, 10, 6);
            episodeLayout->setSpacing(6);
            auto* heading = new QHBoxLayout;
            heading->setSpacing(6);
            auto* selected = new qfw::CheckBox(episodeGroup);
            selected->setObjectName(QStringLiteral("episodeSelection"));
            selected->setChecked(selectedEpisodes_.contains(number));
            selected->setToolTip(episode);
            heading->addWidget(selected);
            connect(selected, &QCheckBox::toggled, this, [this, number, updateSelection](bool checked) {
                if (checked) selectedEpisodes_.insert(number);
                else selectedEpisodes_.remove(number);
                updateSelection();
            });
            auto* expand = new qfw::TransparentToolButton(
                expandedEpisodes_.contains(number) ? qfw::FluentIconEnum::ChevronDown
                                                   : qfw::FluentIconEnum::ChevronRight, episodeGroup);
            expand->setCheckable(true);
            expand->setChecked(expandedEpisodes_.contains(number));
            expand->setToolTip(episode);
            heading->addWidget(expand);
            auto* label = new qfw::BodyLabel(wrapLongLatinRuns(episode), episodeGroup);
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
            label->setSizePolicy(wrapPolicy);
            label->setToolTip(episode);
            heading->addWidget(label, 1);
            const auto ready = stageStatus(QDir(entry.absoluteFilePath()));
            const QStringList names{Text::instance().Cover, Text::instance().OriginalVideo,
                Text::instance().OriginalSubtitle, Text::instance().TranslatedSubtitle,
                Text::instance().TranslatedVideo};
            const QVector<qfw::FluentIconEnum> icons{qfw::FluentIconEnum::Photo,
                qfw::FluentIconEnum::Video, qfw::FluentIconEnum::Document,
                qfw::FluentIconEnum::Globe, qfw::FluentIconEnum::Video};
            for (int stage = 0; stage < ready.size(); ++stage) {
                auto* status = new QWidget(episodeGroup);
                auto* statusLayout = new QHBoxLayout(status);
                statusLayout->setContentsMargins(0, 0, 0, 0);
                statusLayout->setSpacing(3);
                auto* icon = new qfw::IconWidget(qfw::FluentIcon(icons.at(stage)).qicon(), status);
                icon->setFixedSize(16, 16);
                statusLayout->addWidget(icon);
                statusLayout->addWidget(new qfw::CaptionLabel(
                    ready.at(stage) ? QStringLiteral("✓") : QStringLiteral("—"), status));
                status->setToolTip(names.at(stage) + QStringLiteral(": ") +
                    (ready.at(stage) ? QStringLiteral("✓") : QStringLiteral("—")));
                heading->addWidget(status);
            }
            const QString url = data.videoUrl.trimmed();
            auto* moreEpisode = new qfw::TransparentToolButton(qfw::FluentIconEnum::More, episodeGroup);
            moreEpisode->setToolTip(episode);
            heading->addWidget(moreEpisode);
            connect(moreEpisode, &QPushButton::clicked, this,
                    [this, moreEpisode, number, url, count = entries.size()]() {
                auto* menu = new qfw::RoundMenu(QString(), moreEpisode);
                const auto add = [menu, moreEpisode](const QString& text, bool enabled,
                                              const std::function<void()>& action) {
                    auto* item = new QAction(text, menu);
                    item->setEnabled(enabled);
                    menu->addAction(item);
                    connect(item, &QAction::triggered, moreEpisode, [moreEpisode, action]() {
                        QTimer::singleShot(0, moreEpisode, action);
                    });
                };
                add(trText("编辑标题"), true, [this, number]() { editEpisode(number); });
                add(trText("插入新集"), true, [this, number]() { addEpisode(number); });
                add(Text::instance().VideoURL, !url.isEmpty() && QUrl(url).isValid(),
                    [url]() { QDesktopServices::openUrl(QUrl(url)); });
                add(trText("删除本集"), count > 1, [this, number]() { removeEpisode(number); });
                connect(menu, &qfw::RoundMenu::closedSignal, menu, &QObject::deleteLater);
                menu->execAt(moreEpisode->mapToGlobal(QPoint(0, moreEpisode->height())));
            });
            episodeLayout->addLayout(heading);
            auto *files = new QWidget(episodeGroup);
            auto *fileLayout = new QVBoxLayout(files);
            fileLayout->setContentsMargins(10, 0, 0, 0);
            fileLayout->setSpacing(6);
            const auto createFiles = [this, files, fileLayout, url,
                                      episodePath = entry.absoluteFilePath()]() {
                if (files->property("filesLoaded").toBool()) return;
                files->setProperty("filesLoaded", true);
                const QDir episodeDir(episodePath);
                const bool original = QFileInfo::exists(episodeDir.filePath(QStringLiteral("原文.srt"))) || QFileInfo::exists(episodeDir.filePath(QStringLiteral("原文_OCR.srt"))) || QFileInfo::exists(episodeDir.filePath(QStringLiteral("原文_Whisper.srt")));
                const bool encoded = QFileInfo::exists(episodeDir.filePath(QStringLiteral("熟肉.mp4")));
                const struct
                {
                    const char *name;
                    qfw::FluentIconEnum icon;
                    bool download, extract, translate, encode, archive;
                } expected[] = {
                    {"封面.jpg", qfw::FluentIconEnum::Photo, true, false, false, false, false},
                    {"生肉.mp4", qfw::FluentIconEnum::Video, true, true, false, false, false},
                    {"熟肉.mp4", qfw::FluentIconEnum::Video, false, false, false, encoded, false},
                    {"原文.srt", qfw::FluentIconEnum::Document, false, false, false, false, false},
                    {"原文_OCR.srt", qfw::FluentIconEnum::Document, false, false, false, false, true},
                    {"原文_Whisper.srt", qfw::FluentIconEnum::Document, false, false, false, false, true},
                    {"译文.srt", qfw::FluentIconEnum::Document, false, false, original, false, false},
                };
                for (const auto &file : expected)
                {
                    const auto name = QString::fromUtf8(file.name);
                    auto* item = new FileItemWidget(name, episodeDir.filePath(name), file.icon,
                                                    file.download, file.extract, file.translate,
                                                    file.encode, file.archive, files, url);
                    connect(item, &FileItemWidget::fileChanged, this,
                            [this]() { showPage(currentPage_); });
                    fileLayout->addWidget(item);
                }
            };
            if (expandedEpisodes_.contains(number)) createFiles();
            files->setVisible(expandedEpisodes_.contains(number));
            connect(expand, &QPushButton::toggled, this, [this, number, files, expand, createFiles](bool expanded) {
                if (expanded) createFiles();
                if (expanded) expandedEpisodes_.insert(number);
                else expandedEpisodes_.remove(number);
                files->setVisible(expanded);
                expand->setIcon(qfw::FluentIcon(expanded ? qfw::FluentIconEnum::ChevronDown
                                                       : qfw::FluentIconEnum::ChevronRight));
            });
            episodeLayout->addWidget(files);
            body->addWidget(episodeGroup);
        }
        addPager();
        body->addStretch();
        QTimer::singleShot(0, this, [this, generation = loadGeneration_, scrollPosition]() {
            if (generation == loadGeneration_) verticalScrollBar()->setValue(scrollPosition);
        });
        if (notifyOnLoad_) {
            notifyOnLoad_ = false;
            emit projectLoaded(path_);
        }
        if (notifyOnRefresh_) {
            notifyOnRefresh_ = false;
            NotificationService::success(Text::instance().Success,
                Text::instance().FileListRefreshed, this);
        }
    }

    void ProjectDetailInterface::addEpisode(int number)
    {
        projects::Document document;
        QString error;
        if (!projects::read(path_, &document, &error)) {
            NotificationService::error(trText("读取项目失败"), error, this);
            return;
        }
        EpisodeEditor dialog(QStringLiteral("在第 %1 集插入新集").arg(number),
                             document.translated, window());
        if (!dialog.exec()) return;
        projects::Episode episode{dialog.originalInput->text().trimmed(),
            dialog.translatedInput ? dialog.translatedInput->text().trimmed() : QString(),
            dialog.urlInput->text().trimmed()};
        if (!projects::insertEpisode(path_, number, episode, &error))
            NotificationService::error(trText("插入失败"), error, this);
        else {
            selectedEpisodes_.clear();
            expandedEpisodes_.clear();
            showPage(currentPage_);
            NotificationService::success(Text::instance().Success,
                Text::instance().NewEpisodeInserted, this);
        }
    }

    void ProjectDetailInterface::removeEpisode(int number)
    {
        qfw::MessageDialog confirm(trText("确认删除"),
            QStringLiteral("删除第 %1 集及该集目录中的全部文件？").arg(number), window());
        if (!confirm.exec()) return;
        QString error;
        const bool removed = projects::deleteEpisode(path_, number, &error);
        selectedEpisodes_.clear();
        expandedEpisodes_.clear();
        showPage(currentPage_);
        if (!removed)
            NotificationService::error(trText("删除失败"), error, this);
        else
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().EpisodeDeleted,
                    {QString::number(number)}), this);
    }

    void ProjectDetailInterface::editEpisode(int number)
    {
        projects::Document document;
        QString error;
        if (!projects::read(path_, &document, &error) ||
            number < 1 || number > document.episodes.size()) {
            NotificationService::error(trText("读取项目失败"), error, this);
            return;
        }
        EpisodeEditor dialog(QStringLiteral("编辑第 %1 集").arg(number),
                             document.translated, window());
        const auto& previous = document.episodes.at(number - 1);
        dialog.originalInput->setText(previous.originalTitle);
        if (dialog.translatedInput)
            dialog.translatedInput->setText(previous.translatedTitle);
        dialog.urlInput->setText(previous.videoUrl);
        if (!dialog.exec()) return;
        projects::Episode episode{dialog.originalInput->text().trimmed(),
            dialog.translatedInput ? dialog.translatedInput->text().trimmed() : QString(),
            dialog.urlInput->text().trimmed()};
        if (!projects::editEpisode(path_, number, episode, &error))
            NotificationService::error(trText("编辑失败"), error, this);
        else {
            showPage(currentPage_);
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().ETAUU,
                    {QString::number(number)}), this);
        }
    }

    void ProjectDetailInterface::dispatchTask(int taskType, const QString& folderPath,
                                              const QString& videoUrl)
    {
        const QDir dir(folderPath);
        switch (taskType) {
        case 0:
            emit GlobalEventBus::instance().download_requested(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("video")},
                {QStringLiteral("url"), videoUrl.trimmed()},
                {QStringLiteral("save_path"), folderPath}});
            break;
        case 1:
            emit GlobalEventBus::instance().whisper_requested(
                dir.filePath(QStringLiteral("生肉.mp4")),
                dir.filePath(QStringLiteral("原文_Whisper.srt")));
            break;
        case 2:
            for (const char* name : {"原文.srt", "原文_OCR.srt", "原文_Whisper.srt"}) {
                const QString source = dir.filePath(QString::fromUtf8(name));
                if (QFileInfo::exists(source)) {
                    emit GlobalEventBus::instance().translate_requested(
                        source, dir.filePath(QStringLiteral("译文.srt")));
                    return;
                }
            }
            break;
        case 3:
            emit GlobalEventBus::instance().ffmpeg_requested(
                dir.filePath(QStringLiteral("熟肉.mp4")),
                dir.filePath(QStringLiteral("熟肉_压制.mp4")));
            break;
        default:
            break;
        }
    }
}
