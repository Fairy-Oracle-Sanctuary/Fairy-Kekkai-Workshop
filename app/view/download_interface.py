# coding:utf-8
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request

from PySide6.QtCore import QProcess, Qt, QThread, Signal
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QLabel,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)
from qfluentwidgets import (
    FluentIcon,
    Pivot,
    PrimaryPushButton,
    PushButton,
    ScrollArea,
    SegmentedWidget,
)

from app.common.config import cfg

from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.task_status import TaskStatus, status_text
from ..common.text import Text
from ..components.config_card import YTDLPSettingInterface
from ..components.dialog import CustomMessageBox
from ..components.download_card import DownloadItemWidget
from ..service.download_service import DownloadProcess, DownloadTask

NO_WINDOW_KWARGS = (
    {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
)


class DownloadStackedInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()

        # 创建堆叠窗口
        self.pivot = Pivot(self)
        self.stackedWidget = QStackedWidget(self)
        self.vBoxLayout = QVBoxLayout(self)

        self.downloadInterface = DownloadInterface()
        self.settingInterface = YTDLPSettingInterface()

        # 添加标签页
        self.addSubInterface(
            self.downloadInterface, "downloadInterface", self.globalText.Download
        )
        self.addSubInterface(
            self.settingInterface, "settingInterface", self.globalText.Settings
        )

        # 连接信号并初始化当前标签页
        self.stackedWidget.currentChanged.connect(self.onCurrentIndexChanged)
        self.stackedWidget.setCurrentWidget(self.downloadInterface)
        self.pivot.setCurrentItem(self.downloadInterface.objectName())

        self.vBoxLayout.setContentsMargins(30, 0, 30, 30)
        self.vBoxLayout.addWidget(self.pivot, 0, Qt.AlignHCenter)
        self.vBoxLayout.addWidget(self.stackedWidget)

        self.resize(780, 800)
        self.setObjectName("DownloadStackedInterfaces")

    def addSubInterface(self, widget: QLabel, objectName: str, text: str):
        widget.setObjectName(objectName)
        widget.setAlignment(Qt.AlignCenter)
        self.stackedWidget.addWidget(widget)

        # 使用全局唯一的 objectName 作为路由键
        self.pivot.addItem(
            routeKey=objectName,
            text=text,
            onClick=lambda: self.stackedWidget.setCurrentWidget(widget),
        )

    def onCurrentIndexChanged(self, index):
        widget = self.stackedWidget.widget(index)
        self.pivot.setCurrentItem(widget.objectName())


class DownloadInterface(ScrollArea):
    """下载界面"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.view = QWidget(self)
        self.vBoxLayout = QVBoxLayout(self.view)

        self.download_tasks = []  # 所有下载任务
        self.active_downloads = []  # 活跃的下载线程
        self.max_concurrent_downloads = cfg.concurrentDownloads.value  # 最大同时下载数

        self.logger = Logger("DownloadInterface", "download")

        self._initWidget()

        event_bus.download_requested.connect(self.addDownloadFromProject)
        # 监听配置变化，更新最大并发数
        cfg.concurrentDownloads.valueChanged.connect(self._updateMaxConcurrentDownloads)

    def _initWidget(self):
        self.setWidget(self.view)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setObjectName("downloadInterface")
        self.enableTransparentBackground()

        # 创建添加下载按钮
        addDownloadBtn = PrimaryPushButton(self.globalText.AddDownloadTask, self)
        addDownloadBtn.setIcon(FluentIcon.ADD)
        addDownloadBtn.clicked.connect(self.showAddDownloadDialog)

        # 创建更新组件
        if sys.platform == "win32":
            self.updateBtn = PushButton(self.globalText.UpdateYtDlp, self)

        # 创建分段控件
        self.segmentedWidget = SegmentedWidget(self)
        self.allTab = QWidget()
        self.downloadingTab = QWidget()
        self.completedTab = QWidget()
        self.failedTab = QWidget()

        self.segmentedWidget.addItem(
            self.allTab, self.globalText.All, lambda: self.filterTasks("all")
        )
        self.segmentedWidget.addItem(
            self.downloadingTab,
            status_text(TaskStatus.PROCESSING, self.globalText.Downloading),
            lambda: self.filterTasks(TaskStatus.PROCESSING),
        )
        self.segmentedWidget.addItem(
            self.completedTab,
            status_text(TaskStatus.DONE),
            lambda: self.filterTasks(TaskStatus.DONE),
        )
        self.segmentedWidget.addItem(
            self.failedTab,
            status_text(TaskStatus.FAILED),
            lambda: self.filterTasks(TaskStatus.FAILED),
        )

        self.segmentedWidget.setCurrentItem(self.allTab)
        self.segmentedWidget.setMaximumHeight(30)

        # 创建任务列表容器
        self.taskListContainer = QWidget(self)
        self.taskListLayout = QVBoxLayout(self.taskListContainer)
        self.taskListLayout.setAlignment(Qt.AlignTop)

        # 设置布局
        self.vBoxLayout.addWidget(addDownloadBtn)
        if sys.platform == "win32":
            self.vBoxLayout.addWidget(self.updateBtn)
            self.updateBtn.clicked.connect(self.updateYtDlp)
        self.vBoxLayout.addWidget(self.segmentedWidget)
        self.vBoxLayout.addWidget(self.taskListContainer)

        # 连接信号
        # self.retryDownloadSignal.connect(self.retryDownload)
        # self.removeTaskSignal.connect(self.removeTask)

    def _updateMaxConcurrentDownloads(self, value):
        """更新最大并发下载数"""
        self.max_concurrent_downloads = value
        # 如果当前活跃下载数超过新的限制，需要停止一些任务
        active_count = len(
            [t for t in self.download_tasks if t.status == TaskStatus.PROCESSING]
        )
        if active_count > self.max_concurrent_downloads:
            # 停止超出限制的任务
            excess_count = active_count - self.max_concurrent_downloads
            stopped = 0
            for task in reversed(self.download_tasks):
                if task.status == TaskStatus.PROCESSING and stopped < excess_count:
                    # 找到对应的线程并停止
                    for thread in self.active_downloads:
                        if thread.task.id == task.id:
                            thread.cancel()
                            task.status = TaskStatus.WAITING
                            self.updateTaskUI(task.id)
                            stopped += 1
                            break

    def showAddDownloadDialog(self):
        """显示添加下载对话框"""
        main_window = None
        for widget in QApplication.topLevelWidgets():
            if widget.isWindow() and widget.isVisible():
                main_window = widget
                break

        dialog = CustomMessageBox(
            title=self.globalText.AddDownloadTask,
            text=self.globalText.PleaseEnterVideoURL,
            parent=main_window if main_window else self.window(),
            min_width=500,
        )

        if dialog.exec():
            url = dialog.LineEdit.text().strip()
            if not url:
                event_bus.notification_service.show_warning(
                    self.globalText.InputError, self.globalText.PleaseEnterAValidURL
                )
                return

            path = QFileDialog.getExistingDirectory(
                self,
                self.globalText.PSTDTDT,
                os.path.expanduser("~\\Downloads"),
            )
            if not path:
                event_bus.notification_service.show_warning(
                    self.globalText.InputError, self.globalText.PSTDTDT
                )
                return

            task = DownloadTask(
                url=url,
                download_path=path,
                file_name="",  # 使用配置中的输出模板
            )
            self.addDownloadTask(task)
        else:
            pass

    def addDownloadTask(self, task):
        """添加下载任务"""
        self.logger.info(f"添加下载任务: {task.url} -> {task.download_path}")
        self.download_tasks.append(task)

        # 创建任务项
        self.task_item = DownloadItemWidget(task, self.taskListContainer)
        self.taskListLayout.insertWidget(0, self.task_item)

        # 连接信号 - 添加这四行代码
        self.task_item.removeTaskSignal.connect(self.removeTask)
        self.task_item.retryDownloadSignal.connect(self.retryDownload)

        # 开始下载（如果没有超过最大并发数）
        self.startNextDownload()

        # 更新过滤视图
        self.filterTasks(self._currentFilter())

    def startNextDownload(self):
        """开始下一个下载任务"""
        # 检查当前活跃下载数
        active_count = len(
            [t for t in self.download_tasks if t.status == TaskStatus.PROCESSING]
        )

        if active_count >= self.max_concurrent_downloads:
            return

        # 查找等待中的任务
        waiting_tasks = [
            t for t in self.download_tasks if t.status == TaskStatus.WAITING
        ]

        if waiting_tasks:
            task = waiting_tasks[0]
            self.startDownload(task)
            if sys.platform == "win32":
                self.setUpdateBoxEnabled(False)

    def startDownload(self, task: DownloadTask):
        """开始下载任务"""
        self.logger.info(
            f"[开始下载] task_id={task.id}, url={task.url}, path={task.download_path}"
        )
        # 检查 yt-dlp 路径
        ytdlp_path = cfg.ytdlpPath.value
        self.logger.info(
            f"[开始下载] yt-dlp 配置路径: {ytdlp_path}, 存在={os.path.exists(ytdlp_path)}"
        )
        if not os.path.exists(ytdlp_path):
            self.logger.error(f"[开始下载] yt-dlp 路径不存在: {ytdlp_path}")
            event_bus.notification_service.show_error(
                self.globalText.ConfigurationError,
                self.globalText.YDPDNEPCTCPIS.format(ytdlp_path),
            )
            task.status = TaskStatus.FAILED
            self.updateTaskUI(task.id)
            return

        # 创建下载线程
        self.logger.info(f"[开始下载] 创建 DownloadProcess, task_id={task.id}")
        download_thread = DownloadProcess(task)
        download_thread.progress_signal.connect(
            lambda progress, speed, filename: self.onDownloadProgress(
                task.id, progress, speed, filename
            )
        )
        download_thread.finished_signal.connect(
            lambda success, message: self.onDownloadFinished(task.id, success, message)
        )

        # 存储线程引用到对应的DownloadItemWidget
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if isinstance(widget, DownloadItemWidget) and widget.task.id == task.id:
                widget.download_thread = download_thread
                break

        # 存储线程引用
        self.active_downloads.append(download_thread)

        # 更新任务状态
        task.status = TaskStatus.PROCESSING

        # 更新UI
        self.updateTaskUI(task.id)

        # 开始下载
        download_thread.start()

    def onDownloadProgress(self, task_id, progress, speed, filename):
        """下载进度更新"""
        for task in self.download_tasks:
            if task.id == task_id:
                task.progress = progress
                task.speed = speed
                if filename and not task.filename:
                    task.filename = filename
                self.updateTaskUI(task_id)
                break

    def onDownloadFinished(self, task_id, success, message):
        """下载完成"""
        if sys.platform == "win32":
            self.setUpdateBoxEnabled(True)
        for task in self.download_tasks:
            if task.id == task_id:
                if success:
                    task.status = TaskStatus.DONE
                    event_bus.notification_service.show_success(
                        self.globalText.DownloadCompleted,
                        self.globalText.TextAuto065.format(task.filename),
                    )
                    self.logger.info(
                        f"下载完成: -{task.filename}- 路径: {task.download_path}"
                    )
                else:
                    task.status = TaskStatus.FAILED
                    event_bus.notification_service.show_error(
                        self.globalText.DownloadFailed, message.strip()
                    )
                    self.logger.error(
                        f"下载失败: -{task.filename}- 路径: {task.download_path} 错误信息: {message.strip()}"
                    )

                # 移除活跃下载
                for thread in self.active_downloads[:]:
                    if thread.task.id == task_id:
                        self.active_downloads.remove(thread)
                        break

                self.updateTaskUI(task_id)

                # 开始下一个下载
                self.startNextDownload()
                break

    def cleanup(self):
        """关闭前清理所有活跃下载进程"""
        for thread in self.active_downloads:
            if thread.process and thread.process.state() == QProcess.Running:
                thread.process.kill()
                thread.process.waitForFinished(1000)
        self.active_downloads.clear()

    def updateTaskUI(self, task_id):
        """更新任务UI"""
        # 查找对应的任务项
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if isinstance(widget, DownloadItemWidget) and widget.task.id == task_id:
                widget.updateProgress(
                    widget.task.progress, widget.task.speed, widget.task.filename
                )
                widget.updateStatus(widget.task.status)
                break

        # 更新过滤视图
        self.filterTasks(self._currentFilter())

    def _currentFilter(self):
        """根据当前选中的标签页返回对应的过滤条件"""
        current = self.segmentedWidget.currentItem()
        if current == self.allTab:
            return "all"
        elif current == self.downloadingTab:
            return TaskStatus.PROCESSING
        elif current == self.completedTab:
            return TaskStatus.DONE
        elif current == self.failedTab:
            return TaskStatus.FAILED
        return "all"

    def filterTasks(self, filter_type):
        """过滤任务显示"""
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if isinstance(widget, DownloadItemWidget):
                if filter_type == "all" or widget.task.status == filter_type:
                    widget.setVisible(True)
                else:
                    widget.setVisible(False)

    def retryDownload(self, task_id):
        """重新下载任务"""
        for task in self.download_tasks:
            if task.id == task_id:
                task.status = TaskStatus.WAITING
                task.progress = 0
                task.speed = ""
                task.error_message = ""
                task.start_time = None
                task.end_time = None

                self.updateTaskUI(task_id)
                self.startNextDownload()
                break

    def removeTask(self, task_id):
        """移除任务"""
        try:
            for task in self.download_tasks[:]:
                if task.id == task_id:
                    # 如果任务正在下载，先取消
                    for thread in self.active_downloads[:]:
                        if thread.task.id == task_id:
                            return
                            thread.cancel()
                            thread.wait()
                            self.active_downloads.remove(thread)
                            break

                    self.download_tasks.remove(task)
                    break

            # 从UI中移除
            for i in range(self.taskListLayout.count()):
                widget = self.taskListLayout.itemAt(i).widget()
                if isinstance(widget, DownloadItemWidget) and widget.task.id == task_id:
                    self.taskListLayout.removeWidget(widget)
                    widget.deleteLater()
                    break

            # 开始下一个下载
            self.startNextDownload()
        except Exception as e:
            event_bus.notification_service.show_error(
                self.globalText.Error, self.globalText.TaskRemovalFailed.format(e)
            )

    def addDownloadFromProject(self, request_data):
        """从项目界面添加下载任务"""
        self.logger.info(f"[信号] 收到 download_requested: {request_data}")
        task = DownloadTask(
            url=request_data["url"],
            download_path=request_data["save_path"],
            file_name="生肉",
        )
        self.addDownloadTask(task)
        event_bus.notification_service.show_success(
            "下载",
            "已添加下载任务到队列",
        )

    def setUpdateBoxEnabled(self, enabled):
        """设置更新框状态"""
        self.updateBtn.setEnabled(enabled)

    def updateYtDlp(self):
        """Update yt-dlp"""
        self.setUpdateBoxEnabled(False)
        if sys.platform == "linux":
            self.update_thread = LinuxUpdateYtDlpThread()
        else:
            self.update_thread = UpdateYtDlpThread()
        self.update_thread.progress_signal.connect(self.onUpdateProgress)
        self.update_thread.finished_signal.connect(self.onUpdateFinished)
        self.update_thread.start()
        event_bus.notification_service.show_info("yt-dlp", "下载/更新yt-dlp中...")

    def onUpdateProgress(self, progress, message):
        """Handle yt-dlp update progress"""
        self.updateBtn.setText(self.globalText.Downloading2.format(progress))

    def onUpdateFinished(self, success, message):
        """Handle yt-dlp update completion"""
        self.setUpdateBoxEnabled(True)
        self.updateBtn.setText("下载/更新yt-dlp")
        if success:
            event_bus.notification_service.show_success(
                self.globalText.UpdateSuccessful, message
            )
            self.logger.info(f"yt-dlp update success: {message}")
        else:
            event_bus.notification_service.show_error(
                self.globalText.UpdateFailed, message
            )
            self.logger.error(f"yt-dlp update failed: {message}")


class UpdateYtDlpThread(QThread):
    """更新yt-dlp线程"""

    progress_signal = Signal(int, str)  # progress, status message
    finished_signal = Signal(bool, str)  # success, message

    # yt-dlp release URLs
    YTDLP_URLS = {
        "GitHub": "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
        "Mirror": "https://ghproxy.cc/https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
    }

    def __init__(self, source="GitHub"):
        super().__init__()
        self.source = source
        self._is_cancelled = False

    def run(self):

        try:
            url = self.YTDLP_URLS.get(self.source, self.YTDLP_URLS["GitHub"])
            target_path = cfg.ytdlpPath.value

            # Check current version
            self.progress_signal.emit(0, "正在检查 yt-dlp 版本...")
            current_version = self._get_current_version(target_path)
            latest_version = self._get_latest_version()

            if current_version and current_version == latest_version:
                self.finished_signal.emit(
                    True, f"yt-dlp 已是最新版本 ({latest_version})"
                )
                return

            self.progress_signal.emit(
                0, f"从 {self.source} 下载 yt-dlp {latest_version}..."
            )

            # Create temp file for download
            temp_dir = tempfile.gettempdir()
            temp_file = os.path.join(temp_dir, "yt-dlp_download.exe")

            # Download with progress
            def report_progress(block_num, block_size, total_size):
                if self._is_cancelled:
                    raise Exception("下载取消")
                if total_size > 0:
                    progress = min(
                        int((block_num * block_size / total_size) * 100), 100
                    )
                    self.progress_signal.emit(progress, f"yt-dlp已下载: {progress}%")

            urllib.request.urlretrieve(url, temp_file, reporthook=report_progress)

            if self._is_cancelled:
                if os.path.exists(temp_file):
                    os.remove(temp_file)
                self.finished_signal.emit(False, "yt-dlp下载已取消")
                return

            self.progress_signal.emit(100, "yt-dlp下载完成，正在移动文件...")

            # Ensure target directory exists
            target_dir = os.path.dirname(target_path)
            if target_dir and not os.path.exists(target_dir):
                os.makedirs(target_dir)

            # Force replace existing file
            if os.path.exists(target_path):
                try:
                    # Try to delete first (more reliable than overwrite on Windows)
                    os.remove(target_path)
                except PermissionError:
                    # File might be in use, try rename old file
                    backup_path = target_path + ".old"
                    if os.path.exists(backup_path):
                        os.remove(backup_path)
                    os.rename(target_path, backup_path)

            # Move downloaded file to target location
            shutil.move(temp_file, target_path)

            self.finished_signal.emit(True, f"yt-dlp更新成功: {target_path}")

        except Exception as e:
            self.finished_signal.emit(False, f"yt-dlp更新失败: {str(e)}")

    def cancel(self):
        """Cancel the download"""
        self._is_cancelled = True

    def _get_current_version(self, target_path: str) -> str:
        """Get current yt-dlp version"""
        if not os.path.exists(target_path):
            return None
        try:
            result = subprocess.run(
                [target_path, "--version"],
                capture_output=True,
                text=True,
                timeout=10,
                **NO_WINDOW_KWARGS,
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except Exception:
            pass
        return None

    def _get_latest_version(self) -> str:
        """Get latest yt-dlp version from GitHub API"""
        try:
            response = urllib.request.urlopen(
                "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest",
                timeout=10,
            )
            data = json.loads(response.read().decode())
            return data.get("tag_name", "").lstrip("v")
        except Exception:
            return None


class LinuxUpdateYtDlpThread(QThread):
    """Linux 下更新 yt-dlp 线程（使用 pip）"""

    progress_signal = Signal(int, str)
    finished_signal = Signal(bool, str)

    def __init__(self):
        super().__init__()
        self._is_cancelled = False

    def run(self):
        try:
            self.progress_signal.emit(0, "正在检查 yt-dlp 版本...")
            current_version = self._get_current_version()
            latest_version = self._get_latest_version()

            if current_version and current_version == latest_version:
                self.finished_signal.emit(
                    True, f"yt-dlp 已是最新版本 ({latest_version})"
                )
                return

            self.progress_signal.emit(0, f"正在更新 yt-dlp 到 {latest_version}...")

            # 使用 pip 安装/更新 yt-dlp
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", "-U", "yt-dlp"],
                capture_output=True,
                text=True,
                timeout=120,
                **NO_WINDOW_KWARGS,
            )

            if self._is_cancelled:
                self.finished_signal.emit(False, "yt-dlp 更新已取消")
                return

            if result.returncode == 0:
                self.progress_signal.emit(100, "yt-dlp 更新完成")
                self.finished_signal.emit(True, "yt-dlp 更新成功")
            else:
                self.finished_signal.emit(
                    False, f"yt-dlp 更新失败:\n{result.stderr.strip()}"
                )

        except subprocess.TimeoutExpired:
            self.finished_signal.emit(False, "yt-dlp 更新超时")
        except Exception as e:
            self.finished_signal.emit(False, f"yt-dlp 更新失败: {str(e)}")

    def cancel(self):
        self._is_cancelled = True

    def _get_current_version(self) -> str:
        try:
            result = subprocess.run(
                ["yt-dlp", "--version"],
                capture_output=True,
                text=True,
                timeout=10,
                **NO_WINDOW_KWARGS,
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except Exception:
            pass
        return None

    def _get_latest_version(self) -> str:
        try:
            response = urllib.request.urlopen(
                "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest",
                timeout=10,
            )
            data = json.loads(response.read().decode())
            return data.get("tag_name", "").lstrip("v")
        except Exception:
            return None
