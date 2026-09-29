#include "view/main_window.h"
#include <QCloseEvent>
#include <QFileInfo>
#include <QPainter>
#include <QSystemTrayIcon>
#include <QTimer>
#include <type_traits>

#include "common/app_data.h"
#include "common/application.h"
#include "common/config.h"
#include "common/event_bus.h"
#include "common/utils.h"
#include "service/version_service.h"
#include "components/update_dialog.h"
#include "common/setting.h"
#include "common/text.h"
#include "components/notification_service.h"
#include "components/system_tray.h"
#include "view/download_interface.h"
#include "view/ffmpeg_interface.h"
#include "view/home_interface.h"
#include "view/log_interface.h"
#include "view/project_stacked_interface.h"
#include "view/setting_interface.h"
#include "view/translate_interface.h"
#include "view/videocr_interface.h"
#include "view/whisper_interface.h"

namespace fkw
{
    template<class WindowType>
    MainWindowT<WindowType>::MainWindowT(QWidget* parent) : WindowType(parent)
    {
        this->setObjectName(QStringLiteral("MainWindow"));
        this->setWindowTitle(QStringLiteral("Fairy Kekkai Workshop"));
        this->setWindowIcon(QIcon(QStringLiteral(":/app/images/logo.png")));
        refreshBackground();
        this->resize(960, 754);
        this->setMinimumWidth(760);
        if constexpr (std::is_same_v<WindowType, qfw::SplitFluentWindow>) {
            // Split leaves its content under the title bar; reserve room for app pages.
            this->widgetLayout_->setContentsMargins(0, 48, 0, 0);
        }
        auto* bar = this->titleBar();
        themeButton_ = new qfw::TransparentToolButton(bar);
        themeButton_->setFixedSize(bar->minimizeButton()->size());
        updateThemeButtonIcon();
        QList<QLayout*> pending{bar->layout()};
        while (!pending.isEmpty()) {
            QLayout* layout = pending.takeLast();
            const int minimizeIndex = layout->indexOf(bar->minimizeButton());
            if (minimizeIndex >= 0) {
                if (auto* controls = qobject_cast<QHBoxLayout*>(layout))
                    controls->insertWidget(minimizeIndex, themeButton_);
                break;
            }
            for (int i = 0; i < layout->count(); ++i) {
                if (auto* child = layout->itemAt(i)->layout())
                    pending.append(child);
            }
        }
        QObject::connect(themeButton_, &QPushButton::clicked, this, [this]() {
            setAppTheme(qfw::isDarkTheme() ? qfw::Theme::Light : qfw::Theme::Dark);
        });
        QObject::connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged,
                this, &MainWindowT<WindowType>::updateThemeButtonIcon);
        auto *home = new HomeInterface(this);
        auto *projects = new ProjectStackedInterface(this);
        auto *download = new DownloadStackedInterface(this);
        auto *ocr = new VideocrStackedInterfaces(this);
        auto *whisper = new WhisperStackedInterfaces(this);
        auto *translate = new TranslateStackedInterfaces(this);
        auto *ffmpeg = new FFmpegStackedInterfaces(this);
        auto *settings = new SettingInterface(this);
        const auto top = qfw::NavigationItemPosition::Top;
        const auto bottom = qfw::NavigationItemPosition::Bottom;
        this->addSubInterface(home, qfw::FluentIconEnum::Home, Text::instance().Home, top);
        this->addSubInterface(projects, qfw::FluentIconEnum::Folder, trText("项目"), top);
        this->addSubInterface(download, qfw::FluentIconEnum::Download, trText("下载"), top);
        this->addSubInterface(ocr, qfw::FluentIconEnum::Video, trText("字幕"), top);
        this->addSubInterface(whisper, qfw::FluentIconEnum::Microphone, trText("语音"), top);
        this->addSubInterface(translate, qfw::FluentIconEnum::Message, trText("翻译"), top);
        this->addSubInterface(ffmpeg, qfw::FluentIconEnum::ZipFolder, trText("压制"), top);
        this->addSubInterface(settings, qfw::FluentIconEnum::Setting, trText("设置"), bottom);
        routes_ = {{QStringLiteral("home"), home}, {QStringLiteral("projects"), projects}, {QStringLiteral("download"), download}, {QStringLiteral("ocr"), ocr}, {QStringLiteral("whisper"), whisper}, {QStringLiteral("translate"), translate}, {QStringLiteral("ffmpeg"), ffmpeg}, {QStringLiteral("settings"), settings}};
        versionService_ = new VersionService(this);
        QObject::connect(versionService_, &VersionService::checked, this,
                [this](bool hasNewVersion, const QString& error) {
            const auto& t = Text::instance();
            if (!error.isEmpty()) {
                NotificationService::error(t.UpdateInfoError, error, this);
            } else if (hasNewVersion) {
                UpdateDialog dialog(versionService_, this);
                dialog.exec();
            } else {
                qfw::MessageDialog dialog(t.NoNewVersion, t.FKWIUTD, this);
                dialog.yesButton->setText(t.Close);
                dialog.cancelButton->hide();
                dialog.exec();
            }
        });
        auto& bus = GlobalEventBus::instance();
        QObject::connect(&bus, &GlobalEventBus::appMessageSig, this, [this](const QString&) {
            this->show();
            this->raise();
            this->activateWindow();
        });
        QObject::connect(&bus, &GlobalEventBus::navigation_requested, this,
                [this](const QJsonObject& event) {
            openRoute(event.value(QStringLiteral("target")).toString());
        });
        QObject::connect(&bus, &GlobalEventBus::openUrl, this,
                [](const QString& url) { fkw::openUrl(url); });
        QObject::connect(&bus, &GlobalEventBus::checkUpdateSig,
                this, &MainWindowT<WindowType>::checkUpdate);
        QObject::connect(&bus, &GlobalEventBus::notification, this,
                [this](const QJsonObject& event) {
            const QString type = event.value(QStringLiteral("type")).toString();
            const QString title = event.value(QStringLiteral("title")).toString();
            const QString message = event.value(QStringLiteral("message")).toString();
            if (type == QStringLiteral("success"))
                NotificationService::success(title, message, this);
            else if (type == QStringLiteral("error"))
                NotificationService::error(title, message, this);
            else if (type == QStringLiteral("warning"))
                NotificationService::warning(title, message, this);
            else NotificationService::info(title, message, this);
        });
        if (QSystemTrayIcon::isSystemTrayAvailable()) tray_ = new SystemTray(this);
        QObject::connect(home, &HomeInterface::routeRequested, this, &MainWindowT<WindowType>::openRoute);
        QObject::connect(home, &HomeInterface::restartRequested, this, [this]() {
            reallyQuit_ = true;
            if (tray_) tray_->hide();
            if (auto* app = qobject_cast<SingletonApplication*>(qApp)) app->requestRestart();
        });
        QObject::connect(home, &HomeInterface::logRequested, this, [this]()
                {
        if (!logWindow_) logWindow_ = new LogWindow(this);
        logWindow_->show();
        logWindow_->raise();
        logWindow_->activateWindow(); });
        QObject::connect(&AppConfig::instance(), &AppConfig::valueChanged, this,
                [this](const QString& group, const QString& key, const QJsonValue&) {
            if (group == QStringLiteral("MainWindow") &&
                (key == QStringLiteral("ShowBackground") ||
                 key == QStringLiteral("BackgroundPath") ||
                 key == QStringLiteral("BackgroundRect") ||
                 key == QStringLiteral("MicaEnabled"))) {
                refreshBackground();
            }
        });
    }

    template<class WindowType>
    void MainWindowT<WindowType>::checkUpdate()
    {
        versionService_->check();
    }

    template<class WindowType>
    void MainWindowT<WindowType>::refreshBackground()
    {
        auto& config = AppConfig::instance();
        const bool enabled = config.value(QStringLiteral("MainWindow"),
                                          QStringLiteral("ShowBackground")).toBool();
        backgroundShade_ = qBound(0, config.value(QStringLiteral("MainWindow"),
                                  QStringLiteral("BackgroundRect")).toInt(), 200);
        const QString path = config.value(QStringLiteral("MainWindow"),
                                          QStringLiteral("BackgroundPath")).toString();
        if (!enabled) {
            backgroundPath_.clear();
            backgroundPixmap_ = QPixmap();
            scaledBackground_ = QPixmap();
        } else if (path != backgroundPath_ || backgroundPixmap_.isNull() ||
                   !QFileInfo(path).isFile()) {
            backgroundPath_ = path;
            backgroundPixmap_ = QPixmap(path);
            scaledBackground_ = QPixmap();
        }
        showBackground_ = enabled && !backgroundPixmap_.isNull();
        if (enabled && !showBackground_) {
            if (recoveringBackground_) return;
            recoveringBackground_ = true;
            config.set(QStringLiteral("MainWindow"), QStringLiteral("ShowBackground"), false);
            config.set(QStringLiteral("MainWindow"), QStringLiteral("MicaEnabled"), true);
            recoveringBackground_ = false;
            if (!this->isMicaEffectEnabled())
                this->setMicaEffectEnabled(true);
            this->update();
            QTimer::singleShot(0, this, [this]() {
                NotificationService::error(Text::instance().BackgroundImageError,
                                           Text::instance().PCITIE, this);
            });
            return;
        }
        const bool useMica = !enabled && config.value(QStringLiteral("MainWindow"),
                                     QStringLiteral("MicaEnabled")).toBool();
        if (this->isMicaEffectEnabled() != useMica)
            this->setMicaEffectEnabled(useMica);
        this->update();
    }

    template<class WindowType>
    void MainWindowT<WindowType>::paintEvent(QPaintEvent* event)
    {
        WindowType::paintEvent(event);
        if (!showBackground_)
            return;

        if (scaledBackground_.isNull() || backgroundTargetSize_ != this->size()) {
            backgroundTargetSize_ = this->size();
            scaledBackground_ = backgroundPixmap_.scaled(
                this->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        QPainter painter(this);
        const QRect source((scaledBackground_.width() - this->width()) / 2,
                           (scaledBackground_.height() - this->height()) / 2,
                           this->width(), this->height());
        painter.drawPixmap(this->rect(), scaledBackground_, source);
        painter.fillRect(this->rect(), QColor(0, 0, 0, backgroundShade_));
    }

    template<class WindowType>
    void MainWindowT<WindowType>::closeEvent(QCloseEvent* event)
    {
        if (!reallyQuit_ && tray_ &&
            !AppConfig::instance().value(ConfigKeys::closeDirectly, false).toBool()) {
            event->ignore();
            tray_->hideMainWindow();
            return;
        }
        WindowType::closeEvent(event);
    }

    template<class WindowType>
    void MainWindowT<WindowType>::quitFromTray()
    {
        reallyQuit_ = true;
        this->show();
        this->close();
    }

    template<class WindowType>
    void MainWindowT<WindowType>::openRoute(const QString& route)
    {
        if (auto *page = routes_.value(route, nullptr))
            this->switchTo(page);
    }

    template<class WindowType>
    void MainWindowT<WindowType>::updateThemeButtonIcon()
    {
        if (!themeButton_)
            return;
        const bool dark = qfw::isDarkTheme();
        themeButton_->setIcon(qfw::FluentIcon(dark ? qfw::FluentIconEnum::Brightness
                                                 : qfw::FluentIconEnum::QuietHours));
        themeButton_->setToolTip(dark ? Text::instance().Light : Text::instance().Dark);
    }

    template<class WindowType>
    void MainWindowT<WindowType>::setAppTheme(qfw::Theme theme, bool persist)
    {
        qfw::QConfig::instance().setTheme(theme);
        updateThemeButtonIcon();
        if (!persist)
            return;

        AppConfig::instance().set(QStringLiteral("QFluentWidgets"),
            QStringLiteral("ThemeMode"), theme == qfw::Theme::Auto
                ? QStringLiteral("Auto") : theme == qfw::Theme::Dark
                ? QStringLiteral("Dark") : QStringLiteral("Light"));
    }
template class MainWindowT<qfw::MSFluentWindow>;
template class MainWindowT<qfw::FluentWindow>;
template class MainWindowT<qfw::SplitFluentWindow>;

std::unique_ptr<MainWindowHandle> createMainWindow()
{
    const QString style = AppConfig::instance().value(QStringLiteral("MainWindow"),
        QStringLiteral("WindowClass")).toString();
    if (style == QStringLiteral("FluentWindow"))
        return std::make_unique<MainWindowT<qfw::FluentWindow>>();
    if (style == QStringLiteral("SplitFluentWindow"))
        return std::make_unique<MainWindowT<qfw::SplitFluentWindow>>();
    return std::make_unique<MainWindowT<qfw::MSFluentWindow>>();
}
}
