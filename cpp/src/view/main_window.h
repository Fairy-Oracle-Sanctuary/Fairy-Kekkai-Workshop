#pragma once

#include <QMap>
#include <QPointer>
#include <QPixmap>
#include <memory>
#include <qtfluentwidgets.h>

class QCloseEvent;
class QPaintEvent;
class QResizeEvent;

namespace fkw {
class LogWindow;
class SystemTray;
class VersionService;

/** 带加载进度条与状态文字的启动页（对应 Python 的 LoadingSplashScreen）。 */
class LoadingSplashScreen : public qfw::SplashScreen {
    Q_OBJECT

public:
    explicit LoadingSplashScreen(const QIcon& icon, QWidget* parent = nullptr);

    /** 更新加载进度和状态文字。 */
    void setProgress(int value, const QString& text = QString());

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /** 将进度条与状态文字放置在图标下方居中。 */
    void repositionExtras();

    qfw::ProgressBar* progressBar_ = nullptr;
    qfw::BodyLabel* statusLabel_ = nullptr;
};

class MainWindowHandle {
public:
    virtual ~MainWindowHandle() = default;
    virtual QWidget* widget() = 0;
    virtual void openRoute(const QString& route) = 0;
    virtual void setAppTheme(qfw::Theme theme, bool persist = true) = 0;
    virtual void quitFromTray() = 0;
};

std::unique_ptr<MainWindowHandle> createMainWindow();

template<class WindowType>
class MainWindowT : public WindowType, public MainWindowHandle {
public:
    explicit MainWindowT(QWidget* parent = nullptr);
    QWidget* widget() override { return this; }
    void openRoute(const QString& route) override;
    void setAppTheme(qfw::Theme theme, bool persist = true) override;
    void quitFromTray() override;
protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    void updateThemeButtonIcon();
    void checkUpdate();
    void refreshBackground();
    QPointer<LoadingSplashScreen> splashScreen_;
    VersionService* versionService_ = nullptr;
    qfw::TransparentToolButton* themeButton_ = nullptr;
    SystemTray* tray_ = nullptr;
    bool reallyQuit_ = false;
    bool showBackground_ = false;
    bool recoveringBackground_ = false;
    int backgroundShade_ = 0;
    QString backgroundPath_;
    QPixmap backgroundPixmap_;
    QPixmap scaledBackground_;
    QSize backgroundTargetSize_;
    QMap<QString, QWidget*> routes_;
    QPointer<LogWindow> logWindow_;
};
} // namespace fkw
