#include "components/system_tray.h"
#include <QAction>
#include <QIcon>
#include <qtfluentwidgets.h>
#include "common/app_data.h"
#include "view/main_window.h"

namespace fkw {
SystemTray::SystemTray(MainWindowHandle* mainWindow)
    : QSystemTrayIcon(mainWindow->widget()), mainWindow_(mainWindow) {
    setIcon(mainWindow->widget()->windowIcon());
    menu_ = new qfw::SystemTrayMenu(QString(), mainWindow->widget());
    auto* showHide = new QAction(trText("显示/隐藏界面"), menu_);
    auto* exit = new QAction(trText("退出"), menu_);
    menu_->addActions({showHide, exit});
    connect(showHide, &QAction::triggered, this, &SystemTray::onTrayActivated);
    connect(exit, &QAction::triggered, this, &SystemTray::quitApplication);
    setContextMenu(menu_);
    connect(this, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            onTrayActivated();
    });
    show();
}
void SystemTray::onTrayActivated() {
    if (mainWindow_ && mainWindow_->widget()->isVisible()) hideMainWindow();
    else showMainWindow();
}
void SystemTray::showMainWindow() {
    if (!mainWindow_) return;
    mainWindow_->widget()->show();
    mainWindow_->widget()->activateWindow();
    mainWindow_->widget()->raise();
}
void SystemTray::hideMainWindow() {
    if (!mainWindow_) return;
    mainWindow_->widget()->hide();
    showMessage(QStringLiteral("Fairy-Kekkai-Workshop"),
                trText("程序已最小化到系统托盘"), QIcon(QStringLiteral(":/app/images/logo.png")),
                1500);
}
void SystemTray::quitApplication() {
    if (!mainWindow_) return;
    mainWindow_->quitFromTray();
}
}  // namespace fkw
