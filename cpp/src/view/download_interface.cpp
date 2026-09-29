#include "view/download_interface.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include "common/app_data.h"
#include "common/config.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "components/config_card.h"
#include "components/dialog.h"
#include "components/empty_status_widget.h"
#include "components/notification_service.h"

namespace fkw
{
    struct DownloadEntry
    {
        int id = 0;
        DownloadRequest request;
        PreviewTaskStatus status = PreviewTaskStatus::Waiting;
        int progress = 0;
        QString filename;
        QString speed;
        QString stage;
        QString lastOutput;
        QString error;
        DownloadProcess *process = nullptr;
        DownloadItemWidget *card = nullptr;
    };

    class DownloadItemWidget final : public qfw::SimpleCardWidget
    {
    public:
        explicit DownloadItemWidget(const DownloadRequest &request, QWidget *parent = nullptr)
            : qfw::SimpleCardWidget(parent), path_(request.directory)
        {
            setMinimumHeight(120);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            QSizePolicy textPolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
            textPolicy.setHeightForWidth(true);
            auto *layout = new QVBoxLayout(this);
            layout->setContentsMargins(15, 10, 15, 10);
            layout->setSpacing(8);
            layout->setAlignment(Qt::AlignTop);
            auto *first = new QHBoxLayout;
            auto *icon = new qfw::IconWidget(qfw::FluentIcon(
                                                 request.thumbnailOnly ? qfw::FluentIconEnum::Photo : qfw::FluentIconEnum::Video),
                                             this);
            icon->setFixedSize(44, 44);
            first->addWidget(icon, 0, Qt::AlignTop);
            auto *titles = new QVBoxLayout;
            titles->setSpacing(2);
            titles->setAlignment(Qt::AlignTop);
            title_ = new qfw::StrongBodyLabel(!request.title.isEmpty() ? request.title
                : request.thumbnailOnly ? trText("封面下载")
                                        : Text::instance().VideoDownload, this);
            title_->setToolTip(request.title);
            title_->setWordWrap(true);
            title_->setSizePolicy(textPolicy);
            titles->addWidget(title_);
            auto *path = new qfw::CaptionLabel(request.directory, this);
            path->setWordWrap(true);
            path->setSizePolicy(textPolicy);
            path->setToolTip(request.directory);
            titles->addWidget(path);
            first->addLayout(titles, 1);
            pill_ = new qfw::PillPushButton(this);
            pill_->setDisabled(true);
            pill_->setChecked(true);
            pill_->setFixedWidth(110);
            first->addWidget(pill_, 0, Qt::AlignTop);
            layout->addLayout(first);

            auto *second = new QHBoxLayout;
            progress_ = new qfw::ProgressBar(this);
            progress_->setFixedHeight(8);
            second->addWidget(progress_, 4);
            speed_ = new qfw::CaptionLabel(Text::instance().Initializing, this);
            speed_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            speed_->setWordWrap(false);
            speed_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            second->addWidget(speed_, 1);
            layout->addLayout(second);

            auto *third = new QHBoxLayout;
            auto *url = new qfw::CaptionLabel(QStringLiteral("URL: ") + request.url, this);
            url->setWordWrap(true);
            url->setSizePolicy(textPolicy);
            url->setToolTip(request.url);
            url->setTextInteractionFlags(Qt::TextSelectableByMouse);
            third->addWidget(url, 1);
            auto makeButton = [this, third](qfw::FluentIconEnum icon, const QString &tip)
            {
                auto *result = new qfw::TransparentToolButton(icon, this);
                result->setFixedSize(26, 26);
                result->setToolTip(tip);
                third->addWidget(result);
                return result;
            };
            folder_ = makeButton(qfw::FluentIconEnum::Folder, Text::instance().OpenFolder);
            cancel_ = makeButton(qfw::FluentIconEnum::Close, Text::instance().CancelDownload);
            retry_ = makeButton(qfw::FluentIconEnum::Sync, Text::instance().RetryDownload);
            remove_ = makeButton(qfw::FluentIconEnum::Delete, Text::instance().RemoveTask);
            layout->addLayout(third);
            QObject::connect(folder_, &QToolButton::clicked, this, [this]()
                             {
            if (QFileInfo(path_).isDir())
                QDesktopServices::openUrl(QUrl::fromLocalFile(path_)); });
            QObject::connect(cancel_, &QToolButton::clicked, this, [this]()
                             {
            qfw::MessageDialog dialog(Text::instance().ConfirmCancellation,
                                       Text::instance().AYSYWTCTDT, window());
            dialog.yesButton->setText(Text::instance().OK);
            dialog.cancelButton->setText(Text::instance().Cancel);
            if (dialog.exec() && onCancel) onCancel(); });
            QObject::connect(retry_, &QToolButton::clicked, this,
                             [this]()
                             { if (onRetry) onRetry(); });
            QObject::connect(remove_, &QToolButton::clicked, this,
                             [this]()
                             { if (onRemove) onRemove(); });
            render(PreviewTaskStatus::Waiting, 0, {}, {});
        }
        std::function<void()> onCancel;
        std::function<void()> onRetry;
        std::function<void()> onRemove;
        void setDetail(const QString &detail) { speed_->setToolTip(detail); }
        void setVideoTitle(const QString &title) {
            if (title.isEmpty()) return;
            title_->setText(title);
            title_->setToolTip(title);
        }
        void render(PreviewTaskStatus status, int percent, const QString &speed,
                    const QString &stage)
        {
            const bool processing = status == PreviewTaskStatus::Processing;
            const bool success = status == PreviewTaskStatus::Succeeded;
            const bool failed = status == PreviewTaskStatus::Failed;
            pill_->setText(processing                               ? Text::instance().Downloading
                           : success                                ? Text::instance().TextAuto005
                           : failed                                 ? Text::instance().Failed3
                           : status == PreviewTaskStatus::Cancelled ? Text::instance().Cancelled
                                                                    : Text::instance().Waiting);
            pill_->setProperty("isSecondary", status == PreviewTaskStatus::Waiting);
            pill_->setProperty("isPrimary", processing);
            pill_->setProperty("isSuccess", success);
            pill_->setProperty("isError", failed);
            pill_->style()->unpolish(pill_);
            pill_->style()->polish(pill_);
            progress_->setValue(qBound(0, percent, 100));
            speed_->setText(!processing                    ? QString()
                            : !speed.isEmpty()             ? QStringLiteral("%1% %2").arg(percent).arg(speed)
                            : percent > 0 && percent < 100 ? QStringLiteral("%1%").arg(percent)
                            : !stage.isEmpty()             ? stage
                                                           : trText("正在连接"));
            folder_->setVisible(success);
            cancel_->setVisible(processing);
            retry_->setVisible(failed || status == PreviewTaskStatus::Cancelled);
            remove_->setEnabled(success || failed || status == PreviewTaskStatus::Cancelled);
        }

    private:
        QString path_;
        qfw::StrongBodyLabel *title_ = nullptr;
        qfw::PillPushButton *pill_ = nullptr;
        qfw::ProgressBar *progress_ = nullptr;
        qfw::CaptionLabel *speed_ = nullptr;
        qfw::TransparentToolButton *folder_ = nullptr;
        qfw::TransparentToolButton *cancel_ = nullptr;
        qfw::TransparentToolButton *retry_ = nullptr;
        qfw::TransparentToolButton *remove_ = nullptr;
    };

    DownloadInterface::DownloadInterface(QWidget *parent) : qfw::ScrollArea(parent)
    {
        setObjectName(QStringLiteral("downloadInterface"));
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *view = new QWidget(this);
        auto *layout = new QVBoxLayout(view);
        auto *add = new qfw::PrimaryPushButton(
            qfw::FluentIcon(qfw::FluentIconEnum::Add).qicon(), Text::instance().AddDownloadTask, view);
        updateButton_ = new qfw::PushButton(Text::instance().UpdateYtDlpButton, view);
        auto *tabs = new qfw::SegmentedWidget(view);
        tabs->addItem(QStringLiteral("allTab"), Text::instance().All,
                      [this](bool)
                      { filterTasks(QStringLiteral("all")); });
        tabs->addItem(QStringLiteral("downloadingTab"), Text::instance().Downloading,
                      [this](bool)
                      { filterTasks(QStringLiteral("processing")); });
        tabs->addItem(QStringLiteral("completedTab"), Text::instance().TextAuto005,
                      [this](bool)
                      { filterTasks(QStringLiteral("completed")); });
        tabs->addItem(QStringLiteral("failedTab"), Text::instance().Failed3,
                      [this](bool)
                      { filterTasks(QStringLiteral("failed")); });
        tabs->setCurrentItem(QStringLiteral("allTab"));
        tabs->setMaximumHeight(30);
        layout->addWidget(add);
        layout->addWidget(updateButton_);
        layout->addWidget(tabs);
        taskRegion_ = new QWidget(view);
        taskList_ = new QVBoxLayout(taskRegion_);
        taskList_->setAlignment(Qt::AlignTop);
        layout->addWidget(taskRegion_, 1);
        emptyRegion_ = new QWidget(view);
        auto *emptyLayout = new QVBoxLayout(emptyRegion_);
        auto *empty = new EmptyStatusWidget(Text::instance().NoTasks, emptyRegion_);
        emptyLayout->addStretch();
        emptyLayout->addWidget(empty, 0, Qt::AlignHCenter);
        emptyLayout->addStretch();
        layout->addWidget(emptyRegion_, 1);
        setWidget(view);
        enableTransparentBackground();
        filterTasks(QStringLiteral("all"));

        QObject::connect(add, &QPushButton::clicked, this, [this]()
                         {
        CustomMessageBox dialog(Text::instance().AddDownloadTask,
                                Text::instance().PleaseEnterVideoURL, 500, window());
        if (!dialog.exec()) return;
        const QString url = dialog.lineEdit->text().trimmed();
        if (url.isEmpty()) {
            NotificationService::warning(Text::instance().InputError,
                                         Text::instance().PleaseEnterAValidURL, this);
            return;
        }
        const QString directory = QFileDialog::getExistingDirectory(this,
            Text::instance().PSTDTDT,
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        if (directory.isEmpty()) {
            NotificationService::warning(Text::instance().InputError,
                                         Text::instance().PSTDTDT, this);
            return;
        }
        addDownloadTask({url, directory, {}}); });
        QObject::connect(updateButton_, &QPushButton::clicked,
                         this, &DownloadInterface::updateYtDlp);
        QObject::connect(&AppConfig::instance(), &AppConfig::valueChanged, this,
                         [this](const QString &group, const QString &key, const QJsonValue &)
                         {
                             if (group == QStringLiteral("YTDLP") && key == QStringLiteral("ConcurrentDownloads"))
                                 startNextDownloads();
                         });
        QObject::connect(&GlobalEventBus::instance(), &GlobalEventBus::download_requested,
                         this, [this](const QJsonObject &request)
                         {
        const QString url = request.value(QStringLiteral("url")).toString().trimmed();
        const QString path = request.value(QStringLiteral("save_path")).toString();
        if (url.isEmpty() || path.isEmpty()) return;
        const bool thumbnail = request.value(QStringLiteral("type")).toString()
            == QStringLiteral("thumbnail");
        addDownloadTask({url, path, thumbnail ? QStringLiteral("封面") : QStringLiteral("生肉"),
                         thumbnail, request.value(QStringLiteral("title")).toString()});
        if (!request.value(QStringLiteral("silent")).toBool())
            NotificationService::success(Text::instance().Download,
                                         Text::instance().AddedDownloadToQueue, this); });
    }
    DownloadInterface::~DownloadInterface()
    {
        for (auto *entry : tasks_)
        {
            if (entry->process)
            {
                QObject::disconnect(entry->process, nullptr, this, nullptr);
                delete entry->process;
            }
            delete entry;
        }
    }
    void DownloadInterface::addDownloadTask(const DownloadRequest &request)
    {
        if (request.url.trimmed().isEmpty() || request.directory.isEmpty())
            return;
        auto *entry = new DownloadEntry;
        entry->id = ++nextId_;
        entry->request = request;
        entry->card = new DownloadItemWidget(request, taskRegion_);
        entry->card->onCancel = [entry]()
        {
            if (entry->process && entry->status == PreviewTaskStatus::Processing)
                entry->process->cancel();
        };
        entry->card->onRetry = [this, entry]()
        { retryDownload(entry); };
        entry->card->onRemove = [this, entry]()
        { removeTask(entry); };
        tasks_.append(entry);
        taskList_->insertWidget(0, entry->card);
        filterTasks(filter_);
        startNextDownloads();
    }
    void DownloadInterface::startNextDownloads()
    {
        if (updating_)
            return;
        const int maxActive = qMax(1, AppConfig::instance().value(
                                                               QStringLiteral("YTDLP"), QStringLiteral("ConcurrentDownloads"))
                                          .toInt());
        int active = 0;
        for (auto *entry : tasks_)
            if (entry->process)
                ++active;
        for (auto *entry : tasks_)
        {
            if (active >= maxActive)
                break;
            if (entry->status != PreviewTaskStatus::Waiting)
                continue;
            auto *process = new DownloadProcess(entry->request, this);
            entry->process = process;
            entry->status = PreviewTaskStatus::Processing;
            updateTaskUI(entry);
            ++active;
            QObject::connect(process, &DownloadProcess::outputLine, this,
                             [this, entry](const QString &line)
                             {
                                 entry->lastOutput = line;
                                 entry->card->setDetail(line);
                             });
            QObject::connect(process, &DownloadProcess::titleResolved, this,
                             [entry](const QString& title) { entry->card->setVideoTitle(title); });
            QObject::connect(process, &DownloadProcess::status, this,
                             [this, entry](const QString &stage)
                             {
                                 if (entry->stage == stage)
                                     return;
                                 entry->stage = stage;
                                 entry->speed.clear();
                                 updateTaskUI(entry);
                             });
            QObject::connect(process, &DownloadProcess::progress, this,
                             [this, entry](int percent, const QString &speed, const QString &filename)
                             {
                                 entry->progress = percent;
                                 entry->speed = speed;
                                 if (!filename.isEmpty())
                                     entry->filename = filename;
                                 updateTaskUI(entry);
                                 emit GlobalEventBus::instance().download_progress(
                                     QJsonObject{{QStringLiteral("task_id"), entry -> id},
                                                 {QStringLiteral("progress"), percent},
                                                 {QStringLiteral("speed"), speed},
                                                 {QStringLiteral("filename"), entry->filename}});
                             });
            QObject::connect(process, &DownloadProcess::finished, this,
                             [this, entry, process](bool success, bool cancelled, const QString &message)
                             {
                                 entry->process = nullptr;
                                 process->deleteLater();
                                 entry->status = cancelled ? PreviewTaskStatus::Cancelled
                                                 : success ? PreviewTaskStatus::Succeeded
                                                           : PreviewTaskStatus::Failed;
                                 entry->progress = success ? 100 : cancelled ? 0
                                                                             : entry->progress;
                                 entry->error = success ? QString() : message;
                                 if (success && entry->filename.isEmpty())
                                     entry->filename = entry->request.fileName.isEmpty()
                                                           ? entry->request.url
                                                           : entry->request.fileName;
                                 updateTaskUI(entry);
                                 if (cancelled)
                                 {
                                     NotificationService::info(Text::instance().DownloadCancelled,
                                                               Text::instance().HasBeenCancelled, this);
                                 }
                                 else if (success)
                                 {
                                     NotificationService::success(Text::instance().DownloadCompleted,
                                                                  QString(Text::instance().TextAuto065).replace(QStringLiteral("{}"), entry->filename), this);
                                 }
                                 else
                                 {
                                     NotificationService::error(Text::instance().DownloadFailed, message, this);
                                 }
                                 if (!cancelled)
                                     emit GlobalEventBus::instance().download_finished_signal(
                                         success, success ? entry -> request.directory : message);
                                 QTimer::singleShot(0, this, [this]()
                                                    { startNextDownloads(); });
                             });
            process->start();
        }
        updateButton_->setEnabled(active == 0 && !updating_);
    }
    void DownloadInterface::retryDownload(DownloadEntry *entry)
    {
        if (entry->process)
            return;
        entry->status = PreviewTaskStatus::Waiting;
        entry->progress = 0;
        entry->speed.clear();
        entry->stage.clear();
        entry->lastOutput.clear();
        entry->error.clear();
        entry->filename.clear();
        updateTaskUI(entry);
        startNextDownloads();
    }
    void DownloadInterface::removeTask(DownloadEntry *entry)
    {
        if (entry->process)
            return;
        tasks_.removeAll(entry);
        taskList_->removeWidget(entry->card);
        entry->card->hide();
        entry->card->deleteLater();
        delete entry;
        filterTasks(filter_);
        startNextDownloads();
    }
    void DownloadInterface::updateTaskUI(DownloadEntry *entry)
    {
        entry->card->render(entry->status, entry->progress, entry->speed,
                            entry->stage);
        entry->card->setDetail(entry->error.isEmpty() ? entry->lastOutput : entry->error);
        filterTasks(filter_);
    }
    void DownloadInterface::filterTasks(const QString &filter)
    {
        filter_ = filter;
        bool any = false;
        for (auto *entry : tasks_)
        {
            const bool match = filter == QStringLiteral("all") ||
                               (filter == QStringLiteral("processing") && entry->status == PreviewTaskStatus::Processing) ||
                               (filter == QStringLiteral("completed") && entry->status == PreviewTaskStatus::Succeeded) ||
                               (filter == QStringLiteral("failed") && entry->status == PreviewTaskStatus::Failed);
            entry->card->setVisible(match);
            any |= match;
        }
        taskRegion_->setVisible(any);
        emptyRegion_->setVisible(!any);
    }
    void DownloadInterface::updateYtDlp()
    {
        if (updating_)
            return;
        for (auto *entry : tasks_)
            if (entry->process)
                return;
        const QString target = DownloadProcess::executablePath();
        if (target.isEmpty())
        {
            NotificationService::error(Text::instance().ConfigurationError,
                                       QString(Text::instance().YDPDNEPCTCPIS).replace(QStringLiteral("{}"), target), this);
            return;
        }
        updating_ = true;
        updateButton_->setEnabled(false);
        NotificationService::info(Text::instance().UpdateYtDlpButton,
                                  Text::instance().UpdateYtDlpInProgress, this);
        if (!network_)
            network_ = new QNetworkAccessManager(this);
        QNetworkRequest request(QUrl(QStringLiteral(
            "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *reply = network_->get(request);
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                         [this](qint64 received, qint64 total)
                         {
                             if (total > 0)
                                 updateButton_->setText(QString(Text::instance().Downloading2).replace(QStringLiteral("{}"), QString::number(qMin(100, int(received * 100 / total)))));
                         });
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, target]()
                         {
        QString error;
        if (reply->error() != QNetworkReply::NoError) error = reply->errorString();
        else {
            const QByteArray data = reply->readAll();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status < 200 || status >= 300 || !data.startsWith("MZ")) {
                error = trText("yt-dlp 更新文件无效，原有文件已保留");
            } else if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
                error = trText("无法创建 yt-dlp 目录");
            } else {
                QSaveFile file(target);
                if (!file.open(QIODevice::WriteOnly) ||
                    file.write(data) != data.size() || !file.commit())
                    error = file.errorString();
            }
        }
        reply->deleteLater();
        updating_ = false;
        updateButton_->setText(Text::instance().UpdateYtDlpButton);
        startNextDownloads();
        if (error.isEmpty())
            NotificationService::success(Text::instance().UpdateSuccessful, target, this);
        else NotificationService::error(Text::instance().UpdateFailed, error, this); });
    }
    DownloadStackedInterface::DownloadStackedInterface(QWidget *parent)
        : BaseStackedInterfaces(parent)
    {
        setObjectName(QStringLiteral("DownloadStackedInterfaces"));
        addSubInterface(new DownloadInterface(this), QStringLiteral("downloadInterface"),
                        Text::instance().Download);
        addSubInterface(new YTDLPSettingInterface(this), QStringLiteral("settingInterface"),
                        Text::instance().Settings);
    }
} // namespace fkw
