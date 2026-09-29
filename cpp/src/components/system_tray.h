#pragma once
#include <QSystemTrayIcon>
#include <QPointer>
namespace qfw { class SystemTrayMenu; }
namespace fkw {
class MainWindowHandle;
class SystemTray : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit SystemTray(MainWindowHandle* mainWindow);
    void onTrayActivated();
    void showMainWindow();
    void hideMainWindow();
    void quitApplication();
private:
    MainWindowHandle* mainWindow_ = nullptr;
    qfw::SystemTrayMenu* menu_ = nullptr;
};
}  // namespace fkw
