# coding: utf-8
import sys
from pathlib import Path

from PySide6.QtCore import QRect, QSettings, QSize, Qt, QTimer, QUrl
from PySide6.QtGui import QColor, QDesktopServices, QIcon, QPainter, QPixmap
from PySide6.QtWidgets import QApplication
from qfluentwidgets import (
    BodyLabel,
    InfoBarPosition,
    MessageBox,
    NavigationItemPosition,
    ProgressBar,
    SplashScreen,
    Theme,
    TransparentToolButton,
    isDarkTheme,
    setTheme,
)
from qfluentwidgets import FluentIcon as FIF

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.setting import GITHUB_URL, RELEASE_URL
from ..common.task_status import TaskStatus
from ..components.infobar import NotificationService
from ..components.system_tray import SystemTray
from ..service.version_service import VersionService
from .download_interface import DownloadStackedInterface
from .ffmpeg_interface import FFmpegStackedInterfaces
from .home_interface import HomeInterface
from .project_interface import ProjectStackedInterface

# from .release_interface import ReleaseStackedInterfaces
from .setting_interface import SettingInterface
from .translate_interface import TranslateStackedInterfaces
from .whisper_interface import WhisperStackedInterfaces
from ..common.text import Text

if sys.platform == "win32":
    from .videocr_interface import VideocrStackedInterfaces


class LoadingSplashScreen(SplashScreen):
    """带加载进度条与状态文字的启动页"""

    def __init__(self, icon, parent=None):
        super().__init__(icon, parent)
        self.globalText = Text()

        # 进度条（禁用动画，使同步初始化期间能即时显示进度）
        self.progressBar = ProgressBar(self, useAni=False)
        self.progressBar.setFixedWidth(320)
        self.progressBar.setValue(0)

        # 状态文字
        self.statusLabel = BodyLabel(self.globalText.Starting, self)
        self.statusLabel.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self._repositionExtras()

    def setProgress(self, value: int, text: str = None):
        """更新加载进度和状态文字"""
        self.progressBar.setValue(value)
        if text is not None:
            self.statusLabel.setText(text)
            self.statusLabel.adjustSize()
            self._repositionExtras()
        QApplication.processEvents()

    def _repositionExtras(self):
        """将进度条与状态文字放置在图标下方居中"""
        if not hasattr(self, "progressBar"):
            return
        ih = self.iconSize().height()
        cx = self.width() // 2
        cy = self.height() // 2
        py = cy + ih // 2 + 40
        self.progressBar.move(cx - self.progressBar.width() // 2, py)
        self.statusLabel.adjustSize()
        self.statusLabel.move(cx - self.statusLabel.width() // 2, py + 24)

    def resizeEvent(self, e):
        super().resizeEvent(e)
        self._repositionExtras()


window_class = cfg.get(cfg.windowClass)
if window_class == "MSFluentWindow":
    from qfluentwidgets import MSFluentWindow as window
elif window_class == "FluentWindow":
    from qfluentwidgets import FluentWindow as window
else:
    from qfluentwidgets import MSFluentWindow as window


class MainWindow(window):
    def __init__(self):
        super().__init__()
        self.globalText = Text()
        # 提前初始化背景标志，避免 paintEvent 在显示时访问未定义属性而崩溃
        self.isShowBackground = False

        # 先设置窗口图标/尺寸，再创建启动页
        self._initWindow()

        # 创建启动页面（在加载界面前显示 Logo 与加载进度）
        self.splashScreen = LoadingSplashScreen(self.windowIcon(), self)
        self.splashScreen.setIconSize(QSize(120, 120))
        self.splashScreen.raise_()

        # 显示主窗口，使作为其子控件的启动页可见，并立即绘制
        self.show()
        QApplication.processEvents()
        self.splashScreen.setProgress(10, self.globalText.InitializingServices)

        # 初始化版本服务
        self.versionManager = VersionService()

        # 初始化通知服务
        self.notification_service = NotificationService(self)

        # 可以自定义配置（可选）
        self.notification_service.set_default_duration(3000)
        self.notification_service.set_position(InfoBarPosition.BOTTOM_RIGHT)
        event_bus.notification_service = self.notification_service
        self.splashScreen.setProgress(30, self.globalText.LoadingSettings)

        # 读取设置
        self.settings = QSettings("Fairy-Kekkai-Workshop", "Settings")

        # 背景图片
        if cfg.get(cfg.showBackground):
            if Path(cfg.get(cfg.backgroundPath)).exists():
                self.isShowBackground = True
                self.backgroundPixmap = QPixmap(cfg.get(cfg.backgroundPath))
            else:
                event_bus.notification_service.show_error(
                    self.globalText.BackgroundImageError, self.globalText.PCITIE
                )

        self.splashScreen.setProgress(50, self.globalText.LoadingInterface)
        self._initNavigation()
        self.splashScreen.setProgress(80, self.globalText.IST)

        # 初始化系统托盘
        self.system_tray = SystemTray(self)

        # 设置应用程序不在最后一个窗口关闭时退出
        QApplication.setQuitOnLastWindowClosed(False)

        self._connectSignalToSlot()
        if sys.platform == "win32":
            self._initThemeButton()
        self.splashScreen.setProgress(100, self.globalText.StartupComplete)

        # 检查是否首次运行，显示新手引导
        if cfg.get(cfg.isFirstRun):
            from ..components.teaching_tips import TeachingTipManager

            self.teaching_tip_manager = TeachingTipManager(self)

            # 延迟显示，确保窗口完全加载
            QTimer.singleShot(500, self.teaching_tip_manager.show_teaching_tips)

        # 关闭启动页面
        self.splashScreen.finish()

    def paintEvent(self, event):
        """重绘事件，绘制背景图片"""
        if sys.platform != "win32":
            super().paintEvent(event)
            return

        if self.isShowBackground:
            painter = QPainter(self)
            painter.setRenderHints(
                QPainter.RenderHint.SmoothPixmapTransform
                | QPainter.RenderHint.Antialiasing
            )

            if not self.backgroundPixmap.isNull():
                # 缩放图片以适应窗口大小，同时保持比例
                scaledPixmap = self.backgroundPixmap.scaled(
                    self.size(),
                    Qt.AspectRatioMode.KeepAspectRatioByExpanding,
                    Qt.TransformationMode.SmoothTransformation,
                )

                # 计算居中位置
                x = (scaledPixmap.width() - self.width()) // 2
                y = (scaledPixmap.height() - self.height()) // 2

                # 绘制背景图片
                painter.drawPixmap(
                    0, 0, scaledPixmap, x, y, self.width(), self.height()
                )

                # 添加半透明黑色遮罩，让前景内容更清晰
                painter.fillRect(
                    self.rect(), QColor(0, 0, 0, cfg.get(cfg.backgroundRect))
                )

                # 为导航栏和标题栏添加更深的遮罩，确保内容清晰
                if self.navigationInterface.isVisible():
                    nav_rect = self.navigationInterface.geometry()
                    painter.fillRect(nav_rect, QColor(0, 0, 0, 0))

                if self.titleBar.isVisible():
                    title_bar_rect = self.titleBar.geometry()
                    painter.fillRect(title_bar_rect, QColor(0, 0, 0, 0))

                # 为标题栏按钮区域添加完全不透明的背景
                if self.titleBar.isVisible():
                    title_bar_rect = self.titleBar.geometry()

                    # 获取最小化、最大化、关闭、主题按钮的位置
                    # 这些按钮通常在标题栏的右侧
                    button_width = 46  # 标准按钮宽度
                    button_height = title_bar_rect.height() - 15

                    close_button_rect = QRect(
                        title_bar_rect.right() - button_width,
                        title_bar_rect.top(),
                        button_width,
                        button_height,
                    )

                    maximize_button_rect = QRect(
                        close_button_rect.left() - button_width,
                        title_bar_rect.top(),
                        button_width,
                        button_height,
                    )

                    minimize_button_rect = QRect(
                        maximize_button_rect.left() - button_width,
                        title_bar_rect.top(),
                        button_width,
                        button_height,
                    )

                    theme_button_rect = QRect(
                        minimize_button_rect.left() - button_width,
                        title_bar_rect.top(),
                        button_width,
                        button_height,
                    )

                    # 绘制不透明背景
                    button_color = QColor(45, 45, 45)  # 深灰色背景
                    painter.fillRect(theme_button_rect, button_color)
                    painter.fillRect(minimize_button_rect, button_color)
                    painter.fillRect(maximize_button_rect, button_color)
                    painter.fillRect(close_button_rect, button_color)
            # 调用父类的绘制事件，确保其他内容正常显示
            super().paintEvent(event)
        else:
            # 没有背景图片时，也要调用父类的绘制事件，确保 Windows 10 暗色模式背景正常
            super().paintEvent(event)

    def setBackgroundImage(self, imagePath):
        """设置背景图片路径"""
        self.backgroundPixmap = QPixmap(imagePath)
        self.update()  # 触发重绘

    def _initWindow(self):
        self.resize(960, 754 if sys.platform == "win32" else 773)
        self.setMinimumWidth(760)
        self.setWindowIcon(QIcon(":/app/images/logo.png"))
        self.setWindowTitle("Fairy Kekkai Workshop")

        desktop = QApplication.primaryScreen().availableGeometry()
        w, h = desktop.width(), desktop.height()
        self.move(w // 2 - self.width() // 2, h // 2 - self.height() // 2)

    def _initThemeButton(self):
        """在标题栏最小化按钮左侧添加主题切换按钮"""
        self.themeButton = TransparentToolButton(self.titleBar)
        # 与最小化按钮尺寸保持一致
        self.themeButton.setFixedSize(self.titleBar.minBtn.size())
        self._updateThemeButtonIcon()
        self.themeButton.clicked.connect(self._toggleTheme)
        # 插入到最小化按钮左侧（buttonLayout: minBtn, maxBtn, closeBtn）
        if window_class != "SplitFluentWindow":
            self.titleBar.buttonLayout.insertWidget(
                0, self.themeButton, 0, Qt.AlignmentFlag.AlignTop
            )

    def _updateThemeButtonIcon(self):
        """根据当前主题更新按钮图标"""
        # 深色模式显示太阳（切到浅色），浅色模式显示月亮（切到深色）
        self.themeButton.setIcon(FIF.BRIGHTNESS if isDarkTheme() else FIF.QUIET_HOURS)

    def _toggleTheme(self):
        """切换浅色/深色主题"""
        theme = Theme.LIGHT if isDarkTheme() else Theme.DARK
        cfg.set(cfg.themeMode, theme)
        setTheme(theme)
        self._updateThemeButtonIcon()

    def _initNavigation(self):
        """创建页面"""
        self.homeInterface = HomeInterface(self)
        self.projectInterface = ProjectStackedInterface(self)
        self.downloadInterface = DownloadStackedInterface(self)
        if sys.platform == "win32":
            self.videoCRInterface = VideocrStackedInterfaces(self)
        self.translateInterface = TranslateStackedInterfaces(self)
        self.ffmpegInterface = FFmpegStackedInterfaces(self)
        if sys.platform == "win32":
            self.whisperInterface = WhisperStackedInterfaces(self)
        # self.releaseInterface = ReleaseStackedInterfaces(self)
        self.settingInterface = SettingInterface(self)

        if sys.platform == "win32":
            self.interface = [
                self.homeInterface,
                self.projectInterface,
                self.downloadInterface,
                self.videoCRInterface,
                self.whisperInterface,
                self.translateInterface,
                self.ffmpegInterface,
                # self.releaseInterface,
                self.settingInterface,
            ]
        elif sys.platform == "darwin":
            self.interface = [
                self.homeInterface,
                self.projectInterface,
                self.downloadInterface,
                self.translateInterface,
                self.ffmpegInterface,
                # self.releaseInterface,
                self.settingInterface,
            ]

        self.addSubInterface(self.homeInterface, FIF.HOME, self.globalText.Home)
        self.addSubInterface(self.projectInterface, FIF.FOLDER, self.globalText.Project)
        self.addSubInterface(self.downloadInterface, FIF.DOWNLOAD, self.globalText.Download)
        if sys.platform == "win32":
            self.addSubInterface(self.videoCRInterface, FIF.VIDEO, self.globalText.Subtitles)
            self.addSubInterface(self.whisperInterface, FIF.MICROPHONE, self.globalText.Voice)
        self.addSubInterface(self.translateInterface, FIF.MESSAGE, self.globalText.Translate)
        self.addSubInterface(self.ffmpegInterface, FIF.ZIP_FOLDER, self.globalText.Encode)

        # self.addSubInterface(self.releaseInterface, FIF.IMAGE_EXPORT, "发布")

        # 添加自定义导航组件
        self.navigationInterface.addItem(
            routeKey="Help",
            icon=FIF.HELP,
            text=self.globalText.Help,
            onClick=self.showHelpBox,
            selectable=False,
            position=NavigationItemPosition.BOTTOM,
        )
        if window_class == "MSFluentWindow":
            self.addSubInterface(
                self.settingInterface,
                FIF.SETTING,
                self.globalText.Settings,
                FIF.SETTING,
                NavigationItemPosition.BOTTOM,
            )
        else:
            self.addSubInterface(
                self.settingInterface,
                FIF.SETTING,
                self.globalText.Settings,
                NavigationItemPosition.BOTTOM,
            )

    def _connectSignalToSlot(self):
        """连接信号到槽"""
        self.system_tray.messageClicked.connect(self.on_tray_message_clicked)
        event_bus.checkUpdateSig.connect(self.checkUpdate)
        event_bus.switchToSampleCard.connect(self.switchToSample)
        event_bus.openUrl.connect(self.openUrl)
        event_bus.download_finished_signal.connect(
            self.show_system_tray_message_download_finished
        )
        event_bus.ocr_finished_signal.connect(
            self.show_system_tray_message_videocr_finished
        )
        event_bus.translate_finished_signal.connect(
            self.show_system_tray_message_translate_finished
        )
        event_bus.download_list_finished_signal.connect(
            self.show_system_tray_message_download_list_finished
        )
        event_bus.ffmpeg_finished_signal.connect(
            self.show_system_tray_message_ffmpeg_finished
        )
        event_bus.release_finished_signal.connect(
            self.show_system_tray_message_release_finished
        )
        event_bus.whisper_finished_signal.connect(
            self.show_system_tray_message_whisper_finished
        )

    def showHelpBox(self):
        w = MessageBox(
            self.globalText.SupportProject,
            self.globalText.WNMTMJUII,
            self,
        )
        w.yesButton.setText(self.globalText.VisitRepository)
        w.cancelButton.setText(self.globalText.MaybeLater)

        if w.exec():
            QDesktopServices.openUrl(QUrl(GITHUB_URL))

    def save_window_state(self):
        """保存窗口状态"""
        # 如果窗口是最大化的，保存正常状态的大小
        if self.isMaximized():
            self.settings.setValue("window/maximized", True)
            self.showNormal()  # 临时恢复正常状态以获取大小
            self.settings.setValue("window/size", self.size())
            self.showMaximized()  # 恢复最大化
        else:
            self.settings.setValue("window/maximized", False)
            self.settings.setValue("window/size", self.size())
            self.settings.setValue("window/position", self.pos())

    def closeEvent(self, event):
        """重写关闭事件"""
        # 标记应用正在关闭，阻止通知显示
        from ..common.event_bus import event_bus

        event_bus.is_shutting_down = True

        # 非直接关闭模式：最小化到托盘，不检查任务
        if not (hasattr(self, "_really_quit") and self._really_quit) and not cfg.get(
            cfg.closeDirectly
        ):
            event_bus.is_shutting_down = False
            event.ignore()
            self.hide()
            return

        # 直接关闭模式：检查是否有运行中的任务
        running_tasks = self.check_running_tasks()

        if running_tasks:
            # 显示确认对话框
            from qfluentwidgets import MessageBox

            message = self.globalText.TFTASRCA.format(
                running_tasks
            )
            dialog = MessageBox(self.globalText.ConfirmClose, message, self)
            dialog.yesButton.setText(self.globalText.Close)
            dialog.cancelButton.setText(self.globalText.Cancel)

            if dialog.exec():
                # 用户确认关闭，先停止所有运行中的任务
                self.stop_all_tasks()
                # 保存窗口状态
                self.save_window_state()
                # 执行真正的退出
                super().closeEvent(event)
                QApplication.instance().exit(0)
            else:
                # 用户取消关闭
                event_bus.is_shutting_down = False
                event.ignore()
        else:
            # 没有运行中的任务，直接退出
            self.save_window_state()
            super().closeEvent(event)
            QApplication.instance().exit(0)

    def stop_all_tasks(self):
        """停止所有运行中的任务"""
        # 停止下载任务
        if hasattr(self, "downloadInterface"):
            download_interface = getattr(
                self.downloadInterface, "downloadInterface", None
            )
            if download_interface and hasattr(download_interface, "active_downloads"):
                for thread in download_interface.active_downloads[:]:
                    # 断开信号连接，避免关闭时弹出通知
                    if hasattr(thread, "finished_signal"):
                        try:
                            thread.finished_signal.disconnect()
                        except Exception:
                            pass
                    if hasattr(thread, "cancel"):
                        thread.cancel()
                    # 标记任务为已取消
                    if hasattr(thread, "task"):
                        thread.task.status = TaskStatus.CANCELLED
                # 强制终止残留进程
                if download_interface and hasattr(download_interface, "cleanup"):
                    download_interface.cleanup()

        # 停止翻译任务
        if hasattr(self, "translateInterface"):
            translate_interface = getattr(
                self.translateInterface, "taskInterface", None
            )
            if translate_interface and hasattr(translate_interface, "active_threads"):
                for thread in translate_interface.active_threads[:]:
                    # 断开信号连接
                    if hasattr(thread, "finished_signal"):
                        try:
                            thread.finished_signal.disconnect()
                        except Exception:
                            pass
                    if hasattr(thread, "_is_running"):
                        thread._is_running = False
                    # 标记任务为已取消
                    if hasattr(thread, "task"):
                        thread.task.status = TaskStatus.CANCELLED
                    if hasattr(thread, "wait"):
                        thread.wait(1000)

        # 停止OCR任务
        if hasattr(self, "videoCRInterface"):
            ocr_interface = getattr(self.videoCRInterface, "taskInterface", None)
            if ocr_interface and hasattr(ocr_interface, "active_threads"):
                from PySide6.QtCore import QProcess

                for thread in ocr_interface.active_threads[:]:
                    # 断开信号连接
                    if hasattr(thread, "finished_signal"):
                        try:
                            thread.finished_signal.disconnect()
                        except Exception:
                            pass
                    if hasattr(thread, "_is_running"):
                        thread._is_running = False
                    # 标记任务为已取消
                    if hasattr(thread, "task"):
                        thread.task.status = TaskStatus.CANCELLED
                    if hasattr(thread, "process") and thread.process:
                        if thread.process.state() == QProcess.ProcessState.Running:
                            thread.process.kill()
                            thread.process.waitForFinished(1000)
                    if hasattr(thread, "wait"):
                        thread.wait(1000)

        # 停止FFmpeg任务
        if hasattr(self, "ffmpegInterface"):
            ffmpeg_interface = getattr(self.ffmpegInterface, "taskInterface", None)
            if ffmpeg_interface and hasattr(ffmpeg_interface, "active_threads"):
                from PySide6.QtCore import QProcess

                for thread in ffmpeg_interface.active_threads[:]:
                    # 断开信号连接
                    if hasattr(thread, "finished_signal"):
                        try:
                            thread.finished_signal.disconnect()
                        except Exception:
                            pass
                    if hasattr(thread, "_is_running"):
                        thread._is_running = False
                    # 标记任务为已取消
                    if hasattr(thread, "task"):
                        thread.task.status = TaskStatus.CANCELLED
                    if hasattr(thread, "process") and thread.process:
                        if thread.process.state() == QProcess.ProcessState.Running:
                            thread.process.kill()
                            thread.process.waitForFinished(1000)
                    if hasattr(thread, "wait"):
                        thread.wait(1000)

        # 停止Whisper任务
        if sys.platform == "win32" and hasattr(self, "whisperInterface"):
            whisper_interface = getattr(self.whisperInterface, "taskInterface", None)
            if whisper_interface and hasattr(whisper_interface, "active_threads"):
                from PySide6.QtCore import QProcess

                for thread in whisper_interface.active_threads[:]:
                    # 断开信号连接
                    if hasattr(thread, "finished_signal"):
                        try:
                            thread.finished_signal.disconnect()
                        except Exception:
                            pass
                    if hasattr(thread, "_is_running"):
                        thread._is_running = False
                    # 标记任务为已取消
                    if hasattr(thread, "task"):
                        thread.task.status = TaskStatus.CANCELLED
                    if hasattr(thread, "process") and thread.process:
                        if thread.process.state() == QProcess.ProcessState.Running:
                            thread.process.kill()
                            thread.process.waitForFinished(1000)
                    if hasattr(thread, "wait"):
                        thread.wait(1000)

    def check_running_tasks(self):
        """检查是否有运行中的任务"""
        running_tasks = []

        # 检查下载任务
        if hasattr(self, "downloadInterface"):
            download_interface = getattr(
                self.downloadInterface, "downloadInterface", None
            )
            if download_interface and hasattr(download_interface, "active_downloads"):
                active_count = len(download_interface.active_downloads)
                if active_count > 0:
                    running_tasks.append(
                        self.globalText.DownloadTasks.format(active_count)
                    )

        # 检查翻译任务
        if hasattr(self, "translateInterface"):
            translate_interface = getattr(
                self.translateInterface, "taskInterface", None
            )
            if translate_interface and hasattr(translate_interface, "active_threads"):
                active_count = len(translate_interface.active_threads)
                if active_count > 0:
                    running_tasks.append(
                        self.globalText.TranslationTasks.format(active_count)
                    )

        # 检查OCR任务
        if hasattr(self, "videoCRInterface"):
            ocr_interface = getattr(self.videoCRInterface, "taskInterface", None)
            if ocr_interface and hasattr(ocr_interface, "active_threads"):
                active_count = len(ocr_interface.active_threads)
                if active_count > 0:
                    running_tasks.append(self.globalText.OCRTasks.format(active_count))

        # 检查FFmpeg任务
        if hasattr(self, "ffmpegInterface"):
            ffmpeg_interface = getattr(self.ffmpegInterface, "taskInterface", None)
            if ffmpeg_interface and hasattr(ffmpeg_interface, "active_threads"):
                active_count = len(ffmpeg_interface.active_threads)
                if active_count > 0:
                    running_tasks.append(
                        self.globalText.EncodingTasks.format(active_count)
                    )

        # 检查Whisper任务
        if sys.platform == "win32" and hasattr(self, "whisperInterface"):
            whisper_interface = getattr(self.whisperInterface, "taskInterface", None)
            if whisper_interface and hasattr(whisper_interface, "active_threads"):
                active_count = len(whisper_interface.active_threads)
                if active_count > 0:
                    running_tasks.append(
                        self.globalText.TranscriptionTasks.format(active_count)
                    )

        return "\n".join(running_tasks) if running_tasks else None

    def checkUpdate(self):
        if self.versionManager.hasNewVersion():
            self.showMessageBox(
                self.globalText.NewVersionDetected,
                self.globalText.NewVersion
                + f" {self.versionManager.lastestVersion} "
                + self.globalText.ADYWTDI,
                True,
                lambda: QDesktopServices.openUrl(QUrl(RELEASE_URL)),
            )
        else:
            self.showMessageBox(
                self.globalText.NoNewVersion,
                self.globalText.FKWIUTD,
            )

    def showMessageBox(
        self, title: str, content: str, showYesButton=False, yesSlot=None
    ):
        """show message box"""
        w = MessageBox(title, content, self)
        w.yesButton.setText(self.globalText.OK)
        w.cancelButton.setText(self.globalText.Close)
        if not showYesButton:
            w.cancelButton.setText(self.globalText.Close)
            w.yesButton.hide()
            w.buttonLayout.insertStretch(0, 1)

        if w.exec() and yesSlot is not None:
            yesSlot()

    def on_tray_message_clicked(self):
        """托盘消息被点击时的处理"""
        self.show_main_window_from_tray()

    def show_main_window_from_tray(self):
        """从托盘显示主窗口"""
        self.show()
        self.activateWindow()
        self.raise_()

    def really_quit(self):
        """真正退出应用程序"""
        self._really_quit = True
        self.system_tray.hide()
        # self.close()

    def switchToSample(self, routeKey, index):
        """切换界面"""
        self.switchTo(self.interface[index])

    def openUrl(self, url):
        """打开指定 URL"""
        QDesktopServices.openUrl(QUrl(url))

    def show_system_tray_message_download_finished(self, success, message):
        """通过系统托盘显示下载完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.DownloadComplete.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.DownloadFailed2.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )

    def show_system_tray_message_videocr_finished(self, success, message):
        """通过系统托盘显示视频字幕识别完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.SRC.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.SRF.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )

    def show_system_tray_message_translate_finished(self, success, message):
        """通过系统托盘显示翻译完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.TranslationComplete.format(message[-1]),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.TranslationFailed.format(message[0]),
                QIcon(":/app/images/logo.png"),
                3000,
            )

    def show_system_tray_message_download_list_finished(self, success, message):
        """通过系统托盘显示翻译完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.DownloadComplete.format(message[-1]),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.DownloadFailed2.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )

    def show_system_tray_message_ffmpeg_finished(self, success, message):
        """通过系统托盘显示翻译完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.EncodingComplete.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.EncodingFailed.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )

    def show_system_tray_message_release_finished(self, success, message):
        """通过系统托盘显示B站上传完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.UploadComplete.format(message),
                QIcon(":/app/images/logo/bilibili.svg"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.UploadFailed.format(message),
                QIcon(":/app/images/logo/bilibili.svg"),
                3000,
            )
            print(message)

    def show_system_tray_message_whisper_finished(self, success, message):
        """通过系统托盘显示语音识别完成消息"""
        # 检查应用是否正在关闭

        if hasattr(event_bus, "is_shutting_down") and event_bus.is_shutting_down:
            return

        if success:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.SRC2.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )
        else:
            self.system_tray.showMessage(
                "Fairy-Kekkai-Workshop",
                self.globalText.SRF2.format(message),
                QIcon(":/app/images/logo.png"),
                3000,
            )
