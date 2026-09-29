#include "view/project_detail_interface.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <QDesktopServices>
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
        if (path_ != path) currentPage_ = 1;
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
        auto *actions = new QHBoxLayout();
        auto *back = new qfw::PrimaryPushButton(trText("返回项目列表"), content_);
        auto *refresh = new qfw::PushButton(trText("刷新项目列表"), content_);
        auto *batchTask = new qfw::PushButton(trText("批量任务"), content_);
        auto *batchDelete = new qfw::PushButton(trText("批量删除"), content_);
        actions->setSpacing(6);
        actions->addWidget(back, 1);
        actions->addWidget(refresh, 1);
        actions->addWidget(batchTask, 1);
        actions->addWidget(batchDelete, 1);
        body->addLayout(actions);
        connect(back, &QPushButton::clicked, this, &ProjectDetailInterface::backToProjectList);
        connect(refresh, &QPushButton::clicked, this, [this]()
                { reloadCurrentProject(true); });
        const auto pending = [this](const QString &title)
        {
            NotificationService::warning(title, trText("功能尚未接入。"), this);
        };
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
                BatchTaskDialog::Episode episode{folderNum, entry.absoluteFilePath(), {}, {}};
                if (folderNum >= 1 && folderNum <= document.episodes.size()) {
                    episode.title = document.episodes.at(folderNum - 1).originalTitle;
                    episode.videoUrl = document.episodes.at(folderNum - 1).videoUrl;
                }
                episodes.append(episode);
            }
            BatchTaskDialog dialog(episodes, window());
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
            BatchDeleteDialog dialog(path_, titles, window());
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
        const QString projectTitle = folder.dirName();
        auto *title = new qfw::TitleLabel(wrapLongLatinRuns(projectTitle), content_);
        QSizePolicy wrapPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        wrapPolicy.setHeightForWidth(true);
        title->setTextFormat(Qt::PlainText);
        title->setWordWrap(true);
        title->setSizePolicy(wrapPolicy);
        title->setToolTip(projectTitle);
        title->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->addWidget(title, 0, Qt::AlignTop);
        body->addWidget(new qfw::BodyLabel(formatText(Text::instance().EpisodesTotalPage,
            {QString::number(entries.size()), QString::number(currentPage_),
             QString::number(totalPages)}), content_));
        auto addPips = [this, body, totalPages](bool top)
        {
            if (totalPages < 2)
                return;
            auto *row = new QHBoxLayout();
            row->addStretch();
            if (top)
            {
                auto *pager = new qfw::PipsPager(content_);
                pager->setPageNumber(totalPages);
                pager->setVisibleNumber(std::min(totalPages, 5));
                pager->setCurrentIndex(currentPage_ - 1);
                pager->setPreviousButtonDisplayMode(qfw::PipsScrollButtonDisplayMode::Always);
                pager->setNextButtonDisplayMode(qfw::PipsScrollButtonDisplayMode::Always);
                connect(pager, &qfw::PipsPager::currentIndexChanged, this,
                        [this](int index)
                        { if (index + 1 != currentPage_) beginLoad(index + 1); });
                row->addWidget(pager);
            }
            else
            {
                auto *pager = new Pager(totalPages, std::min(totalPages, 5), content_);
                pager->setCurrentPage(currentPage_);
                connect(pager, &Pager::currentPageChanged, this,
                        [this](int page)
                        { if (page != currentPage_) beginLoad(page); });
                row->addWidget(pager);
            }
            row->addStretch();
            body->addLayout(row);
        };
        addPips(true);
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
            auto* episodeGroup = new QWidget(content_);
            auto* episodeLayout = new QVBoxLayout(episodeGroup);
            episodeLayout->setContentsMargins(10, 0, 10, 0);
            episodeLayout->setSpacing(8);
            auto *heading = new QHBoxLayout();
            auto *label = new qfw::StrongBodyLabel(wrapLongLatinRuns(episode), episodeGroup);
            label->setTextFormat(Qt::PlainText);
            label->setWordWrap(true);
            label->setSizePolicy(wrapPolicy);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setToolTip(episode);
            heading->addWidget(label, 1);
            auto addButton = [this, heading](qfw::FluentIconEnum icon,
                                            const char* tooltip, bool enabled,
                                            const std::function<void()>& action) {
                auto* button = new qfw::TransparentToolButton(icon, content_);
                button->setToolTip(trText(tooltip));
                button->setEnabled(enabled);
                connect(button, &QPushButton::clicked, this, action);
                heading->addWidget(button);
            };
            addButton(qfw::FluentIconEnum::Add, "插入新集", true,
                      [this, number]() { addEpisode(number); });
            addButton(qfw::FluentIconEnum::Delete, "删除本集", entries.size() > 1,
                      [this, number]() { removeEpisode(number); });
            addButton(qfw::FluentIconEnum::Edit, "编辑标题", true,
                      [this, number]() { editEpisode(number); });
            const QString url = data.videoUrl.trimmed();
            auto *link = new qfw::TransparentToolButton(qfw::FluentIconEnum::Link, content_);
            link->setToolTip(url);
            connect(link, &QPushButton::clicked, this, [url]()
                    {
            if (QUrl(url).isValid()) QDesktopServices::openUrl(QUrl(url)); });
            heading->addWidget(link);
            episodeLayout->addLayout(heading);
            auto *files = new QWidget(episodeGroup);
            auto *fileLayout = new QVBoxLayout(files);
            fileLayout->setContentsMargins(10, 0, 0, 0);
            fileLayout->setSpacing(6);
            const QDir episodeDir(entry.absoluteFilePath());
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
            episodeLayout->addWidget(files);
            body->addWidget(episodeGroup);
        }
        addPips(false);
        if (currentPage_ == totalPages) {
            auto* append = new qfw::PrimaryPushButton(trText("添加新集"), content_);
            connect(append, &QPushButton::clicked, this,
                    [this, count = entries.size()]() { addEpisode(count + 1); });
            body->addWidget(append, 0, Qt::AlignHCenter);
        }
        body->addStretch();
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
