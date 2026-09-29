#include "view/setting_interface.h"
#include <QApplication>
#include <QPalette>
#include <QPixmap>
#include <memory>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include "common/app_data.h"
#include "common/config.h"
#include "common/event_bus.h"
#include "common/setting.h"
#include "common/text.h"
#include "components/config_card.h"
#include "components/notification_service.h"
#include "components/project_migration.h"
#include "service/project_relocate.h"
#include "service/project_service.h"
#include "view/main_window.h"

namespace fkw {
namespace {
bool saveSetting(const char* group, const char* key, const QJsonValue& value) {
    return AppConfig::instance().set(QLatin1String(group), QLatin1String(key), value);
}
void groupFont(qfw::SettingCardGroup* group) {
    if (auto* label = group->findChild<QLabel*>()) {
        QFont font = label->font();
        font.setPixelSize(14);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
    }
}
void restartNotice(QWidget* parent) {
    NotificationService::success(trText("更新成功"),
                                 trText("配置在重启软件后生效"), parent);
}
}
SettingInterface::SettingInterface(QWidget* parent) : qfw::ScrollArea(parent) {
    setObjectName(QStringLiteral("settingInterface"));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setViewportMargins(0, 90, 0, 20);
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(36, 10, 36, 0);
    layout->setSpacing(26);
    setWidget(view);
    setWidgetResizable(true);
    enableTransparentBackground();
    auto* title = new qfw::TitleLabel(trText("设置"), this);
    QFont titleFont = title->font();
    titleFont.setPixelSize(23);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    title->move(36, 40);
    title->setFixedHeight(40);
    title->raise();

    auto* personal = new qfw::SettingCardGroup(trText("个性化"), view);
    groupFont(personal);
    auto* theme = new DictSettingCard(qfw::FluentIconEnum::Brush, trText("应用主题"),
        trText("调整应用的外观"), {trText("浅色"), trText("深色"), trText("跟随系统设置")}, personal);
    const QString themeMode = AppConfig::instance().value("QFluentWidgets", "ThemeMode").toString();
    theme->comboBox->setCurrentIndex(themeMode == QStringLiteral("Dark") ? 1
        : themeMode == QStringLiteral("Auto") ? 2 : 0);
    connect(theme->comboBox, &qfw::ComboBox::currentIndexChanged,
            this, [this](int index) {
        if (auto* window = dynamic_cast<MainWindowHandle*>(this->window()))
            window->setAppTheme(index == 2 ? qfw::Theme::Auto
                : index == 1 ? qfw::Theme::Dark : qfw::Theme::Light);
    });
    personal->addSettingCard(theme);
    auto* zoom = new DictSettingCard(qfw::FluentIconEnum::Zoom, trText("界面缩放"),
        Text::instance().ACAFS, {QStringLiteral("100%"), QStringLiteral("125%"),
        QStringLiteral("150%"), QStringLiteral("175%"), QStringLiteral("200%"),
        trText("跟随系统设置")}, personal);
    const QJsonValue dpi = AppConfig::instance().value("MainWindow", "DpiScale");
    zoom->comboBox->setCurrentIndex(dpi.isDouble()
        ? qBound(0, qRound((dpi.toDouble() - 1.0) * 4), 4) : 5);
    connect(zoom->comboBox, &qfw::ComboBox::currentIndexChanged,
            this, [this](int index) {
        saveSetting("MainWindow", "DpiScale", index == 5 ? QJsonValue(QStringLiteral("Auto"))
            : QJsonValue(1.0 + index * 0.25));
        restartNotice(this);
    });
    personal->addSettingCard(zoom);
    auto* language = new DictSettingCard(qfw::FluentIconEnum::Language, trText("语言"),
        trText("设置界面语言"), {QStringLiteral("简体中文"), QStringLiteral("English"),
        QStringLiteral("日本語"), QStringLiteral("한국어"), QStringLiteral("Deutsch"),
        QStringLiteral("Español"), QStringLiteral("Français"),
        QStringLiteral("Português (Brasil)"), QStringLiteral("繁體中文"),
        trText("跟随系统设置")}, personal);
    const QStringList locales{QStringLiteral("zh_CN"), QStringLiteral("en_US"),
        QStringLiteral("ja_JP"), QStringLiteral("ko_KR"), QStringLiteral("de_DE"),
        QStringLiteral("es_ES"), QStringLiteral("fr_FR"), QStringLiteral("pt_BR"),
        QStringLiteral("zh_TW"), QStringLiteral("Auto")};
    const QString savedLanguage = AppConfig::instance().value("MainWindow", "Language").toString(QStringLiteral("Auto"));
    const int languageIndex = locales.indexOf(savedLanguage);
    language->comboBox->setCurrentIndex(languageIndex < 0 ? 9 : languageIndex);
    connect(language->comboBox, &qfw::ComboBox::currentIndexChanged,
            this, [this, locales](int index) {
        saveSetting("MainWindow", "Language", locales.at(index));
        restartNotice(this);
    });
    personal->addSettingCard(language);
    auto* accent = new DictSettingCard(qfw::FluentIconEnum::Palette, trText("主题色"),
        Text::instance().AdjustThemeColor, {trText("海沫绿"), trText("跟随系统设置")}, personal);
    accent->comboBox->setCurrentIndex(AppConfig::instance().value("MainWindow", "AccentColor")
        .toString() == QStringLiteral("Auto") ? 1 : 0);
    connect(accent->comboBox, &qfw::ComboBox::currentIndexChanged,
            this, [](int index) {
        const QString color = index ? QStringLiteral("Auto") : QStringLiteral("#009faa");
        saveSetting("MainWindow", "AccentColor", color);
        qfw::QConfig::instance().setThemeColor(index
            ? qApp->palette().color(QPalette::Highlight) : QColor(color));
    });
    personal->addSettingCard(accent);
    auto* windowStyle = new DictSettingCard(qfw::FluentIconEnum::Embed, Text::instance().WindowStyle,
        Text::instance().AdjustWindowStyle, {QStringLiteral("MSFluentWindow"),
        QStringLiteral("FluentWindow"), QStringLiteral("SplitFluentWindow")}, personal);
    const QStringList styles{QStringLiteral("MSFluentWindow"),
        QStringLiteral("FluentWindow"), QStringLiteral("SplitFluentWindow")};
    windowStyle->comboBox->setCurrentIndex(qMax(0,
        styles.indexOf(AppConfig::instance().value("MainWindow", "WindowClass").toString())));
    connect(windowStyle->comboBox, &qfw::ComboBox::currentIndexChanged,
            this, [this, windowStyle, styles](int index) {
        if (!saveSetting("MainWindow", "WindowClass", styles.at(index))) {
            const QSignalBlocker blocker(windowStyle->comboBox);
            windowStyle->comboBox->setCurrentIndex(qMax(0, styles.indexOf(
                AppConfig::instance().value("MainWindow", "WindowClass").toString())));
            NotificationService::error(trText("保存失败"),
                trText("窗口类型配置写入失败"), this);
            return;
        }
        restartNotice(this);
    });
    personal->addSettingCard(windowStyle);
    auto* closeDirectly = new qfw::SwitchSettingCard(qfw::FluentIconEnum::Close,
        trText("直接关闭"), Text::instance().EODDC, nullptr, personal);
    closeDirectly->setChecked(AppConfig::instance().value("MainWindow", "CloseDirectly").toBool());
    connect(closeDirectly, &qfw::SwitchSettingCard::checkedChanged,
            this, [](bool checked) { saveSetting("MainWindow", "CloseDirectly", checked); });
    personal->addSettingCard(closeDirectly);
    auto* background = new qfw::SwitchSettingCard(qfw::FluentIconEnum::Photo,
        trText("背景图片"), Text::instance().EODBI, nullptr, personal);
    background->setChecked(AppConfig::instance().value("MainWindow", "ShowBackground").toBool());
    connect(background, &qfw::SwitchSettingCard::checkedChanged,
            this, [this, background](bool checked) {
        if (checked && QPixmap(AppConfig::instance().value(
                QStringLiteral("MainWindow"), QStringLiteral("BackgroundPath")).toString()).isNull()) {
            const QSignalBlocker blocker(background->findChild<qfw::SwitchButton*>());
            background->setChecked(false);
            saveSetting("MainWindow", "ShowBackground", false);
            saveSetting("MainWindow", "MicaEnabled", true);
            NotificationService::error(Text::instance().BackgroundImageError,
                                       Text::instance().PCITIE, this);
            return;
        }
        saveSetting("MainWindow", "ShowBackground", checked);
    });
    connect(&AppConfig::instance(), &AppConfig::valueChanged, background,
            [background](const QString& group, const QString& key, const QJsonValue&) {
        if (group != QStringLiteral("MainWindow") ||
            key != QStringLiteral("ShowBackground")) return;
        const QSignalBlocker blocker(background->findChild<qfw::SwitchButton*>());
        background->setChecked(AppConfig::instance().value(
            QStringLiteral("MainWindow"), QStringLiteral("ShowBackground")).toBool());
    }, Qt::QueuedConnection);
    personal->addSettingCard(background);
    auto* imagePath = new qfw::PushSettingCard(trText("选择文件"),
        qfw::FluentIconEnum::Photo, trText("选择背景图片"),
        AppConfig::instance().value("MainWindow", "BackgroundPath").toString(), personal);
    connect(imagePath, &qfw::PushSettingCard::clicked, this, [this, imagePath]() {
        const QString path = QFileDialog::getOpenFileName(this, trText("选择背景图片"));
        if (path.isEmpty())
            return;
        if (QPixmap(path).isNull()) {
            NotificationService::error(Text::instance().BackgroundImageError,
                                       Text::instance().PCITIE, this);
            return;
        }
        if (saveSetting("MainWindow", "BackgroundPath", path))
            imagePath->setContent(path);
    });
    personal->addSettingCard(imagePath);
    auto* backgroundRange = new qfw::RangeConfigItem(QStringLiteral("MainWindow"),
        QStringLiteral("BackgroundRect"), AppConfig::instance().value("MainWindow", "BackgroundRect").toInt(),
        std::make_shared<qfw::RangeValidator>(0, 200));
    backgroundRange->setParent(this);
    auto* opacity = new qfw::RangeSettingCard(backgroundRange,
        qfw::FluentIconEnum::Transparent, trText("背景透明度"),
        Text::instance().ABO, personal);
    connect(opacity, &qfw::RangeSettingCard::valueChanged,
            this, [](int value) { saveSetting("MainWindow", "BackgroundRect", value); });
    personal->addSettingCard(opacity);
    layout->addWidget(personal);

    auto* project = new qfw::SettingCardGroup(trText("项目"), view);
    groupFont(project);
    const QJsonValue count = AppConfig::instance().value("Project", "DetailProjectItemNum");
    auto* projectRange = new qfw::RangeConfigItem(QStringLiteral("Project"),
        QStringLiteral("DetailProjectItemNum"), count.isDouble() ? count.toInt() : 5,
        std::make_shared<qfw::RangeValidator>(1, 10));
    projectRange->setParent(this);
    auto* detailCount = new qfw::RangeSettingCard(projectRange, qfw::FluentIconEnum::Document,
        trText("项目详情页数量"), trText("调整项目详情页项目数量"), project);
    connect(detailCount, &qfw::RangeSettingCard::valueChanged,
            this, [](int value) { saveSetting("Project", "DetailProjectItemNum", value); });
    project->addSettingCard(detailCount);
    auto* projectFolder = new qfw::PushSettingCard(trText("更改目录"),
        qfw::FluentIconEnum::Folder, trText("项目目录"),
        QDir::toNativeSeparators(projects::root()), project);
    connect(projectFolder, &qfw::PushSettingCard::clicked, this, [this, projectFolder]() {
        const QString current = projects::root();
        const QString selected = QFileDialog::getExistingDirectory(this,
            trText("选择项目目录"), current,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (selected.isEmpty()) return;
        if (QDir::cleanPath(selected) == QDir::cleanPath(current)) {
            NotificationService::info(trText("项目目录"),
                trText("所选目录与当前目录相同，无需迁移"), this);
            return;
        }
        QString error;
        // 只接受空文件夹：新旧项目混在一起会让搬迁结果无法预期
        if (!projects::checkRoot(selected, &error)) {
            NotificationService::error(trText("无法使用该目录"), error, this);
            return;
        }
        const QStringList sources = projects::projectsIn(current);
        if (sources.isEmpty()) {
            if (projects::rememberRoot(selected, &error))
                projectFolder->setContent(QDir::toNativeSeparators(projects::root()));
            else
                NotificationService::error(trText("修改失败"), error, this);
            return;
        }
        // 选定新目录后立即把现有项目整体搬迁过去，搬完再让用户继续操作
        const projects::RelocateReport report = migrateProjects(selected, sources, window());
        projects::applyRelocation(report);
        if (!projects::rememberRoot(selected, &error)) {
            NotificationService::error(trText("修改失败"), error, this);
            return;
        }
        projectFolder->setContent(QDir::toNativeSeparators(projects::root()));
        if (report.allOk()) {
            NotificationService::success(trText("项目目录已更换"),
                trText("已迁移 %1 个项目到新目录").arg(int(report.records.size())), this);
        } else {
            NotificationService::warning(trText("项目目录已更换，部分项目迁移失败"),
                trText("未迁移成功的项目仍留在原目录，可在项目页重新导入"), this);
        }
    });
    project->addSettingCard(projectFolder);
    layout->addWidget(project);

    auto* download = new qfw::SettingCardGroup(trText("下载"), view);
    groupFont(download);
    const auto icon = [](const QString& path) { return QVariant::fromValue(QIcon(path)); };
    auto* ytdlp = new qfw::PushSettingCard(trText("选择文件"),
        icon(QStringLiteral(":/app/images/logo/ytdlp.svg")), QStringLiteral("yt-dlp"),
        AppConfig::instance().value("Download", "YTDLPPath").toString(), download);
    connect(ytdlp, &qfw::PushSettingCard::clicked, this, [this, ytdlp]() {
        const QString path = QFileDialog::getOpenFileName(this, Text::instance().SelectYtDlpFile);
        if (!path.isEmpty() && saveSetting("Download", "YTDLPPath", path))
            ytdlp->setContent(path);
    });
    download->addSettingCard(ytdlp);
    auto* ffmpeg = new qfw::PushSettingCard(trText("选择文件"),
        icon(QStringLiteral(":/app/images/logo/FFmpeg.svg")), QStringLiteral("FFmpeg"),
        AppConfig::instance().value("FFmpeg", "FFmpegPath").toString(), download);
    connect(ffmpeg, &qfw::PushSettingCard::clicked, this, [this, ffmpeg]() {
        const QString path = QFileDialog::getOpenFileName(this, Text::instance().SelectFfmpegFile);
        if (!path.isEmpty() && saveSetting("FFmpeg", "FFmpegPath", path))
            ffmpeg->setContent(path);
    });
    download->addSettingCard(ffmpeg);
    auto* detect = new qfw::PushSettingCard(trText("检测程序"),
        qfw::FluentIconEnum::Search, trText("检测程序"),
        trText("自动检测并更新程序路径"), download);
    connect(detect, &qfw::PushSettingCard::clicked, this, [this, ytdlp, ffmpeg]() {
        QStringList found;
        QStringList missing;
        const auto locate = [&](const QString& name, const char* group, const char* key,
                                qfw::PushSettingCard* card) {
            QString path = QDir(sourceRoot()).filePath(QStringLiteral("tools/") + name
                                                        + QStringLiteral(".exe"));
            if (!QFileInfo::exists(path))
                path = QStandardPaths::findExecutable(name);
            if (path.isEmpty()) { missing << name; return; }
            if (saveSetting(group, key, path)) {
                card->setContent(path);
                found << name;
            }
        };
        locate(QStringLiteral("yt-dlp"), "Download", "YTDLPPath", ytdlp);
        locate(QStringLiteral("ffmpeg"), "FFmpeg", "FFmpegPath", ffmpeg);
        if (!found.isEmpty())
            NotificationService::success(trText("检测成功"),
                trText("已检测到：") + found.join(QStringLiteral("、")), this);
        if (!missing.isEmpty())
            NotificationService::warning(trText("检测程序"),
                trText("未找到：") + missing.join(QStringLiteral("、")), this);
    });
    download->addSettingCard(detect);
    layout->addWidget(download);

    auto* about = new qfw::SettingCardGroup(trText("关于"), view);
    groupFont(about);
    auto* updates = new qfw::PrimaryPushSettingCard(trText("检查更新"),
        icon(QStringLiteral(":/app/images/logo.png")), trText("关于"),
        copyleftSymbol() + trText("Copyleft") + QStringLiteral(" ")
            + settingData(QStringLiteral("YEAR")).toString() + QStringLiteral(", ")
            + settingData(QStringLiteral("TEAM")).toString() + QStringLiteral(". ")
            + trText("当前版本") + QStringLiteral(" v")
            + settingData(QStringLiteral("VERSION")).toString(),
        about);
    connect(updates, &qfw::PushSettingCard::clicked,
            &GlobalEventBus::instance(), &GlobalEventBus::checkUpdateSig);
    about->addSettingCard(updates);
    auto* tutorial = new qfw::PushSettingCard(trText("查看新手引导"),
        qfw::FluentIconEnum::BookShelf, trText("新手引导"),
        trText("重新查看软件使用教程"), about);
    connect(tutorial, &qfw::PushSettingCard::clicked, this, [this]() {
        NotificationService::info(trText("新手引导"),
            trText("从左侧导航选择项目、下载、字幕、语音、翻译或压制功能。"), this);
    });
    about->addSettingCard(tutorial);
    layout->addWidget(about);
    layout->addStretch();
}
}
