#include "view/project_interface.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include "common/config.h"

#include "common/app_data.h"
#include "common/event_bus.h"
#include "service/download_service.h"
#include "service/project_service.h"
#include "service/project_health.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"
#include "components/dialog.h"
#include "components/project_card.h"

namespace fkw
{
    ProjectInterface::ProjectInterface(QWidget *parent) : qfw::ScrollArea(parent)
    {
        setObjectName(QStringLiteral("projectInterface"));
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *view = new QWidget(this);
        auto *layout = new QVBoxLayout(view);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(10);
        auto *buttons = new TopButtonCard(view);
        buttons->newProjectButton->setObjectName(QStringLiteral("tutorial-project-new"));
        buttons->importProjectButton->setObjectName(QStringLiteral("tutorial-project-import"));
        buttons->newFromPlaylistButton->setObjectName(QStringLiteral("tutorial-project-playlist"));
        layout->addWidget(buttons);
        cardsContainer_ = new QWidget(view);
        cardsContainer_->setAcceptDrops(true);
        cardsContainer_->installEventFilter(this);
        cardsLayout_ = new QVBoxLayout(cardsContainer_);
        cardsLayout_->setContentsMargins(0, 0, 0, 0);
        cardsLayout_->setSpacing(10);
        cardsLayout_->setAlignment(Qt::AlignTop);
        layout->addWidget(cardsContainer_, 1);
        setWidget(view);
        enableTransparentBackground();
        connect(buttons->refreshButton, &QPushButton::clicked, this, [this]() {
            refreshProjectList();
            NotificationService::success(Text::instance().Success,
                Text::instance().ProjectListRefreshed, this);
        });
        connect(buttons->newProjectButton, &QPushButton::clicked, this, [this]() {
            AddProject dialog(window());
            if (!dialog.exec()) return;
            QString error;
            if (!projects::create(dialog.nameInput->text(), dialog.numInput->text().toInt(),
                                  dialog.titleInput->text(), &error)) {
                NotificationService::error(trText("新建项目失败"), error, this);
                return;
            }
            refreshProjectList();
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().NewProjectCreated,
                    {dialog.nameInput->text().trimmed()}), this);
        });
        connect(buttons->importProjectButton, &QPushButton::clicked, this, [this]() {
            const QString folder = QFileDialog::getExistingDirectory(this, trText("导入项目"));
            if (folder.isEmpty()) return;
            if (!projects::isProject(folder)) {
                NotificationService::error(trText("导入项目失败"), trText("不是有效的项目目录"), this);
                return;
            }
            qfw::MessageDialog mode(trText("选择导入方式"),
                trText("复制到项目目录，或仅连接原目录？"), window());
            mode.yesButton->setText(trText("复制"));
            mode.cancelButton->setText(trText("仅连接"));
            const bool copy = mode.exec();
            QString error;
            if (!projects::importProject(folder, copy, &error))
                NotificationService::error(trText("导入项目失败"), error, this);
            else {
                refreshProjectList();
                NotificationService::success(Text::instance().Success,
                    formatText(Text::instance().Added, {folder}), this);
            }
        });
        connect(buttons->newFromPlaylistButton, &QPushButton::clicked, this, [this, buttons]() {
            PlaylistProjectDialog dialog(window());
            if (!dialog.exec()) return;
            const QString executable = DownloadProcess::executablePath();
            if (!QFileInfo::exists(executable)) {
                NotificationService::error(trText("创建失败"), trText("请先设置 yt-dlp 路径"), this);
                return;
            }
            const QString name = dialog.nameInput->text().trimmed();
            const QString title = dialog.titleInput->text().trimmed();
            const QString url = dialog.urlInput->text().trimmed();
            const QUrl playlistUrl(url);
            if ((playlistUrl.scheme() != QStringLiteral("http") &&
                 playlistUrl.scheme() != QStringLiteral("https")) || playlistUrl.host().isEmpty()) {
                NotificationService::warning(trText("输入错误"), trText("请输入有效的视频列表 URL"), this);
                return;
            }
            auto* process = new QProcess(this);
            buttons->newFromPlaylistButton->setEnabled(false);
            NotificationService::info(trText("创建项目"), trText("正在解析视频列表…"), this);
            connect(process, &QProcess::finished, this,
                    [this, process, buttons, name, title](int exitCode, QProcess::ExitStatus status) {
                buttons->newFromPlaylistButton->setEnabled(true);
                QVector<projects::Episode> episodes;
                for (const QByteArray& line : process->readAllStandardOutput().split('\n')) {
                    const QJsonObject item = QJsonDocument::fromJson(
                        QString::fromLocal8Bit(line).toUtf8()).object();
                    const QString episodeTitle = item.value(QStringLiteral("title")).toString().trimmed();
                    QString videoUrl = item.value(QStringLiteral("webpage_url")).toString();
                    if (videoUrl.isEmpty()) videoUrl = item.value(QStringLiteral("url")).toString();
                    if (!videoUrl.startsWith(QStringLiteral("http"))) {
                        const QString id = item.value(QStringLiteral("id")).toString();
                        if (!id.isEmpty()) videoUrl = QStringLiteral("https://www.youtube.com/watch?v=") + id;
                    }
                    if (!episodeTitle.isEmpty() && videoUrl.startsWith(QStringLiteral("http")) &&
                        QUrl(videoUrl).isValid())
                        episodes.append({episodeTitle, QString(), videoUrl});
                }
                QString error = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
                const bool timedOut = process->property("timedOut").toBool();
                process->deleteLater();
                if (timedOut || status != QProcess::NormalExit || episodes.isEmpty()) {
                    NotificationService::error(trText("创建失败"), timedOut
                        ? trText("解析视频列表超时")
                        : error.isEmpty() ? trText("播放列表没有可用视频") : error, this);
                    return;
                }
                if (exitCode != 0 && !error.isEmpty())
                    NotificationService::warning(trText("视频列表提示"), error, this);
                if (!projects::createPlaylist(name, title, episodes, &error)) {
                    NotificationService::error(trText("创建失败"), error, this);
                    return;
                }
                refreshProjectList();
                const QString projectPath = QDir(projects::root()).filePath(name);
                for (int i = 0; i < episodes.size(); ++i) {
                    const QString folder = QDir(projectPath).filePath(QString::number(i + 1));
                    const QString videoUrl = episodes.at(i).videoUrl;
                    emit GlobalEventBus::instance().download_requested(QJsonObject{
                        {QStringLiteral("url"), videoUrl},
                        {QStringLiteral("save_path"), folder},
                        {QStringLiteral("title"), episodes.at(i).originalTitle},
                        {QStringLiteral("type"), QStringLiteral("video")},
                        {QStringLiteral("silent"), true}});
                    emit GlobalEventBus::instance().download_requested(QJsonObject{
                        {QStringLiteral("url"), videoUrl},
                        {QStringLiteral("save_path"), folder},
                        {QStringLiteral("title"), episodes.at(i).originalTitle},
                        {QStringLiteral("type"), QStringLiteral("thumbnail")},
                        {QStringLiteral("silent"), true}});
                }
                NotificationService::success(trText("成功"),
                    trText("已创建 %1 集，视频与封面已加入下载队列").arg(episodes.size()), this);
            });
            connect(process, &QProcess::errorOccurred, this,
                    [this, buttons, process](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart) {
                    buttons->newFromPlaylistButton->setEnabled(true);
                    NotificationService::error(trText("创建失败"),
                        trText("无法启动 yt-dlp"), this);
                    process->deleteLater();
                }
            });
            QStringList args{QStringLiteral("-j"), QStringLiteral("--flat-playlist")};
            const QString proxy = DownloadProcess::configuredProxyUrl();
            if (!proxy.isEmpty()) args << QStringLiteral("--proxy") << proxy;
            args << url;
            process->start(executable, args);
            QTimer::singleShot(60000, process, [process]() {
                if (process->state() != QProcess::NotRunning) {
                    process->setProperty("timedOut", true);
                    process->kill();
                }
            });
        });
        refreshProjectList();
    }

    bool ProjectInterface::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched != cardsContainer_)
            return qfw::ScrollArea::eventFilter(watched, event);
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* drag = static_cast<QDragMoveEvent*>(event);
            if (drag->mimeData()->hasFormat("application/x-fkw-project-path")) {
                drag->acceptProposedAction();
                return true;
            }
        }
        if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            const QString path = QString::fromUtf8(
                drop->mimeData()->data("application/x-fkw-project-path"));
            ProjectCard* moved = nullptr;
            int from = -1, target = 0;
            for (int i = 0; i < cardsLayout_->count(); ++i) {
                auto* card = qobject_cast<ProjectCard*>(cardsLayout_->itemAt(i)->widget());
                if (card && card->path() == path) { moved = card; from = i; }
                if (card && drop->position().y() > card->geometry().center().y())
                    target = i + 1;
            }
            if (!moved) return true;
            cardsLayout_->removeWidget(moved);
            if (target > from) --target;
            cardsLayout_->insertWidget(target, moved);
            QStringList order;
            for (int i = 0; i < cardsLayout_->count(); ++i)
                if (auto* card = qobject_cast<ProjectCard*>(cardsLayout_->itemAt(i)->widget()))
                    order << card->path();
            QString error;
            if (!projects::setOrder(order, &error)) {
                refreshProjectList();
                NotificationService::error(trText("排序失败"), error, this);
            }
            drop->acceptProposedAction();
            return true;
        }
        return qfw::ScrollArea::eventFilter(watched, event);
    }

    void ProjectInterface::refreshProjectList()
    {
        while (auto *item = cardsLayout_->takeAt(0))
        {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        const QStringList paths = projects::candidates();
        const QStringList linked = projects::links();
        for (const QString &path : paths)
        {
            const QDir folder(path);
            const projects::Health health = projects::check(path);
            const int level = health.level == projects::HealthLevel::Broken ? 2
                            : health.ok() ? 0 : 1;
            QString title = folder.dirName();
            for (const auto &file : folder.entryInfoList({QStringLiteral("*.txt")}, QDir::Files))
            {
                if (file.fileName() != QStringLiteral("标题.txt") && file.fileName() != QStringLiteral("icon.txt"))
                {
                    title = file.completeBaseName();
                    break;
                }
            }
            QString iconPath = QStringLiteral(":/app/images/icons/牛排.svg");
            QFile iconFile(folder.filePath(QStringLiteral("icon.txt")));
            if (iconFile.open(QIODevice::ReadOnly))
                iconPath = QString::fromUtf8(iconFile.readAll()).trimmed();
            const bool isLinked = linked.contains(path);
            auto *card = new ProjectCard(folder.dirName(), title, path, QIcon(iconPath),
                                         isLinked, this);
            card->setHealth(level, static_cast<int>(health.issues.size()), health.blocking);
            cardsLayout_->addWidget(card);
            connect(card, &ProjectCard::openProject, this, &ProjectInterface::tryOpenProject);
            connect(card, &ProjectCard::repairProject, this, &ProjectInterface::repairProject);
            connect(card, &qfw::CardWidget::clicked, this, [this, path, title, blocked = health.blocking]() {
                if (blocked) {
                    NotificationService::warning(trText("无法打开项目"),
                        trText("项目存在异常，请先点击「一键修复」修复后再打开。"), this);
                    return;
                }
                const QDir projectDir(path);
                const int total = projectDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).size();
                QVector<int> counts(5, 0);
                for (int i = 1; i <= total; ++i) {
                    const QDir episode(projectDir.filePath(QString::number(i)));
                    const QStringList files{QStringLiteral("封面.jpg"), QStringLiteral("生肉.mp4"),
                        QStringLiteral("熟肉.mp4"), QStringLiteral("原文.srt"),
                        QStringLiteral("译文.srt")};
                    for (int j = 0; j < files.size(); ++j)
                        if (QFileInfo::exists(episode.filePath(files.at(j)))) ++counts[j];
                }
                if (total > 0)
                    for (int& value : counts) value = value * 100 / total;
                ProjectProgressDialog dialog(title, counts, window());
                dialog.exec();
            });
            connect(card, &ProjectCard::editProject, this, [this](const QString& projectPath) {
                const QDir dir(projectPath);
                QString originalTitle = dir.dirName();
                for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files))
                    if (entry.fileName() != QStringLiteral("标题.txt") &&
                        entry.fileName() != QStringLiteral("icon.txt")) {
                        originalTitle = entry.completeBaseName();
                        break;
                    }
                QFile iconFile(dir.filePath(QStringLiteral("icon.txt")));
                QString icon;
                if (iconFile.open(QIODevice::ReadOnly)) {
                    icon = QString::fromUtf8(iconFile.readAll()).trimmed();
                    iconFile.close();
                }
                EditProjectDialog dialog(dir.dirName(), originalTitle, icon, window());
                if (!dialog.exec()) return;
                QString error;
                if (!projects::update(projectPath, dialog.nameInput->text(),
                                      dialog.titleInput->text(), dialog.selectedIcon(), &error))
                    NotificationService::error(trText("编辑项目失败"), error, this);
                else {
                    const QString newPath = QDir(QFileInfo(projectPath).absolutePath())
                        .filePath(dialog.nameInput->text().trimmed());
                    emit projectChanged(projectPath, newPath);
                    refreshProjectList();
                    NotificationService::success(Text::instance().Success,
                        Text::instance().ProjectInfoUpdated, this);
                }
            });
            connect(card, &ProjectCard::moveToTop, this, [this](const QString& projectPath) {
                QStringList sorted = projects::order();
                sorted.removeAll(projectPath);
                sorted.prepend(projectPath);
                QString error;
                if (projects::setOrder(sorted, &error)) {
                    refreshProjectList();
                    NotificationService::success(Text::instance().Success,
                        Text::instance().ProjectPinnedToTop, this);
                } else NotificationService::error(trText("置顶失败"), error, this);
            });
            connect(card, &ProjectCard::removeProject, this,
                    [this](const QString& projectPath, bool unlink) {
                qfw::MessageDialog confirm(unlink ? trText("确认解除连接") : trText("确认删除"),
                    unlink ? trText("项目文件会保留在原目录。")
                           : trText("将永久删除项目目录及全部分集文件。"), window());
                if (!confirm.exec()) return;
                QString error;
                if (!projects::remove(projectPath, unlink, &error))
                    NotificationService::error(trText("操作失败"), error, this);
                else {
                    emit projectRemoved(projectPath);
                    refreshProjectList();
                    NotificationService::success(Text::instance().Success,
                        unlink ? formatText(Text::instance().UnlinkedProject, {projectPath})
                               : formatText(Text::instance().ProjectDeleted, {projectPath}),
                        this);
                }
            });
        }
    }

    void ProjectInterface::tryOpenProject(const QString &path)
    {
        const projects::Health health = projects::check(path);
        if (!health.blocking) {
            emit openProjectDetail(path);
            return;
        }
        QStringList lines;
        for (const projects::Issue &issue : health.issues)
            lines << QStringLiteral("· ") + issue.detail;
        NotificationService::warning(trText("无法打开项目"),
            trText("项目存在异常，请先点击「一键修复」修复后再打开：\n%1")
                .arg(lines.join(QLatin1Char('\n'))), this);
    }

    void ProjectInterface::repairProject(const QString &path)
    {
        const projects::Health health = projects::check(path);
        if (health.ok()) {
            refreshProjectList();
            NotificationService::success(Text::instance().Success,
                trText("项目状态正常，无需修复"), this);
            return;
        }
        if (health.level == projects::HealthLevel::Broken) {
            NotificationService::error(trText("无法自动修复"),
                health.issues.isEmpty() ? trText("项目目录已不存在")
                                        : health.issues.first().detail, this);
            return;
        }
        QStringList lines;
        for (const projects::Issue &issue : health.issues)
            lines << QStringLiteral("· ") + issue.detail;
        qfw::MessageDialog confirm(trText("智能修复项目"),
            trText("检测到以下问题：\n%1\n\n修复会先备份 标题.txt，"
                   "再自动补齐缺失的分集目录与标题记录。")
                .arg(lines.join(QLatin1Char('\n'))), window());
        confirm.yesButton->setText(trText("开始修复"));
        confirm.cancelButton->setText(trText("取消"));
        if (!confirm.exec()) return;
        projects::RepairReport report;
        QString error;
        if (!projects::repair(path, &report, &error)) {
            NotificationService::error(trText("修复失败"), error, this);
            return;
        }
        refreshProjectList();
        if (!report.changed) {
            NotificationService::info(trText("智能修复"),
                trText("未发现需要修改的内容"), this);
            return;
        }
        NotificationService::success(trText("修复完成"),
            trText("已执行：%1").arg(report.actions.join(trText("；"))), this);
    }
}
