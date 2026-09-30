#include <QApplication>
#include <QColor>
#include <QDir>
#include <QIcon>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkProxyFactory>
#include <QPalette>
#include <QProcess>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include <qtfluentwidgets.h>

#include "common/app_data.h"
#include "common/application.h"
#include "common/config.h"
#include "common/logger.h"
#include "components/startup_maintenance.h"
#include "service/ocr_migration.h"
#include "service/project_relocate.h"
#include "service/project_service.h"
#include "view/main_window.h"
#include "view/home_interface.h"
#include "view/project_interface.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
QLocale automaticLanguageLocale() {
#ifdef Q_OS_WIN
    // Windows UI language can differ from the user's region format. Use the
    // user locale, which matches the language chosen for regional formatting.
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0)
        return QLocale(QString::fromWCharArray(localeName));
#endif
    return QLocale::system();
}
}

int main(int argc, char* argv[]) {
    fkw::SingletonApplication app(argc, argv,
        QStringLiteral("Fairy-Kekkai-Workshop"));
    if (app.isRunning()) return 0;
    if (!app.ownsInstance()) return 1;
    fkw::Logger::cleanOldLogs();
    // 同步升级后的 OCR 资源可能与配置里记录的旧路径不一致，启动时先纠正再读配置
    fkw::ocr::fixOcrPaths();
    Q_INIT_RESOURCE(resource);
    Q_INIT_RESOURCE(app_assets);
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    app.setApplicationName(QStringLiteral("Fairy-Kekkai-Workshop"));
    app.setOrganizationName(QStringLiteral("Fairy-Oracle-Sanctuary"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/images/logo.png")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    auto applyPalette = [&app](qfw::Theme theme) {
        const bool dark = qfw::isDarkThemeMode(theme);
        QPalette palette = app.palette();
        palette.setColor(QPalette::Window, dark ? QColor(32, 32, 32) : QColor(243, 246, 251));
        palette.setColor(QPalette::WindowText, dark ? QColor(245, 245, 245) : QColor(24, 28, 33));
        palette.setColor(QPalette::Base, dark ? QColor(39, 39, 39) : QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, dark ? QColor(48, 48, 48) : QColor(246, 248, 251));
        palette.setColor(QPalette::Text, dark ? QColor(245, 245, 245) : QColor(24, 28, 33));
        palette.setColor(QPalette::Button, dark ? QColor(48, 48, 48) : QColor(255, 255, 255));
        palette.setColor(QPalette::ButtonText, dark ? QColor(245, 245, 245) : QColor(24, 28, 33));
        palette.setColor(QPalette::Highlight, QColor(0, 159, 170));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, dark ? QColor(170, 170, 170)
                                                         : QColor(110, 110, 110));
        app.setPalette(palette);
    };
    QObject::connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged,
                     &app, applyPalette);

    const auto config = fkw::readJson(QStringLiteral("config.json"));
    const QString mode = fkw::configString(config, QStringLiteral("QFluentWidgets"),
                                           QStringLiteral("ThemeMode"), QStringLiteral("Light"));
    qfw::QConfig::instance().setTheme(qfw::themeFromString(mode));
    applyPalette(qfw::QConfig::instance().theme());
    QTranslator translator;
    const QString language = fkw::configString(config, QStringLiteral("MainWindow"),
                                               QStringLiteral("Language"));
    const QLocale locale = language.isEmpty() || language == QStringLiteral("Auto")
        ? automaticLanguageLocale() : QLocale(language);
    QTranslator widgetTranslator;
    if (locale.name() == QStringLiteral("zh_CN")
        && widgetTranslator.load(QStringLiteral(
            ":/qfluentwidgets/i18n/qfluentwidgets.zh_CN.qm")))
        app.installTranslator(&widgetTranslator);
    if (translator.load(locale, QStringLiteral("app"), QStringLiteral("."),
                        QStringLiteral(":/app/i18n")))
        app.installTranslator(&translator);

    auto window = fkw::createMainWindow();
    auto* widget = window->widget();
    widget->show();
    const QStringList arguments = app.arguments();
    // 自动化截图跑批时不弹窗打断流程
    const bool automated = arguments.contains(QStringLiteral("--capture"))
        || arguments.contains(QStringLiteral("--capture-theme"))
        || arguments.contains(QStringLiteral("--capture-route"))
        || arguments.contains(QStringLiteral("--capture-project"))
        || arguments.contains(QStringLiteral("--capture-scroll-bottom"));
    if (!automated) {
        // 历史遗留：老版本把项目建在软件目录里，清理或重装软件会连带删掉数据；
        // 同时软件目录里可能还留着旧版本的 PaddleOCR 与识别模型。
        // 两者合成一个启动维护弹窗，串行在后台完成，不阻塞界面。
        const fkw::projects::RelocateReport report = fkw::runStartupMaintenance(widget);
        if (!report.records.isEmpty()) {
            fkw::projects::applyRelocation(report);
            if (auto* projects = widget->findChild<fkw::ProjectInterface*>())
                projects->refreshProjectList();
        }
    }
    // 在启动维护结束之后才显示首次引导，截图自动化不启动教程。
    if (!automated && fkw::AppConfig::instance().value(fkw::ConfigKeys::isFirstRun, true).toBool())
        window->startTutorial(500);
    const int themeAt = arguments.indexOf(QStringLiteral("--capture-theme"));
    if (themeAt >= 0 && themeAt + 1 < arguments.size())
        window->setAppTheme(arguments.at(themeAt + 1).compare(QStringLiteral("dark"),
                            Qt::CaseInsensitive) == 0 ? qfw::Theme::Dark : qfw::Theme::Light, false);
    const int routeAt = arguments.indexOf(QStringLiteral("--capture-route"));
    if (routeAt >= 0 && routeAt + 1 < arguments.size())
        window->openRoute(arguments.at(routeAt + 1));
    const int projectAt = arguments.indexOf(QStringLiteral("--capture-project"));
    if (projectAt >= 0 && projectAt + 1 < arguments.size()) {
        window->openRoute(QStringLiteral("projects"));
        if (auto* projects = widget->findChild<fkw::ProjectInterface*>())
            emit projects->openProjectDetail(arguments.at(projectAt + 1));
    }
    if (arguments.contains(QStringLiteral("--capture-scroll-bottom")))
        QTimer::singleShot(900, widget, [widget]() {
            if (auto* home = widget->findChild<fkw::HomeInterface*>())
                home->verticalScrollBar()->setValue(home->verticalScrollBar()->maximum());
        });
    const int captureAt = arguments.indexOf(QStringLiteral("--capture"));
    if (captureAt >= 0 && captureAt + 1 < arguments.size()) {
        const QString path = arguments.at(captureAt + 1);
        QTimer::singleShot(1200, widget, [widget, path]() {
            widget->grab().save(path);
            qApp->quit();
        });
    }
    const int result = app.exec();
    if (app.restartRequested()) {
        const QString program = app.applicationFilePath();
        const QStringList arguments = app.arguments().mid(1);
        const QString workingDirectory = QDir::currentPath();
        app.releaseInstance();
        if (!QProcess::startDetached(program, arguments, workingDirectory)) return 1;
    }
    return result;
}
