import os
import platform
import subprocess

from PySide6.QtCore import QFileInfo, Qt, QUrl, Signal
from PySide6.QtGui import QColor, QDesktopServices, QPainter, QPen
from PySide6.QtWidgets import QFileIconProvider, QHBoxLayout, QVBoxLayout

from libs.qfluentwidgets_pro import (
    BodyLabel,
    CaptionLabel,
    CardWidget,
    CheckBox,
    FluentIcon,
    ImageLabel,
    IndeterminateProgressBar,
    MessageBox,
    MessageBoxBase,
    ProgressBar,
    SubtitleLabel,
    TransparentToolButton,
    isDarkTheme,
    themeColor,
)

from ..common.event_bus import event_bus
from ..common.task_status import TaskStatus, status_text
from ..common.text import Text


class BaseItemWidget(CardWidget):
    """任务项组件"""

    # 定义信号
    removeTaskSignal = Signal(int)  # 任务ID
    retryTaskSignal = Signal(int)  # 任务ID

    def __init__(
        self,
        task,
        progressBar_type="common",
        task_type=None,
        parent=None,
    ):
        super().__init__(parent)
        self.globalText = Text()
        self.task = task
        self.task_thread = None
        self.progressBar_type = progressBar_type
        self.task_type = task_type or self.globalText.Default

        self._initUI()

    def _initUI(self):
        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()
        self.infoLayout = QHBoxLayout()

        self.fileNameLabel = BodyLabel(str(self.task.input_file))

        self.imageLabel = ImageLabel()
        self.imageLabel.setImage(
            QFileIconProvider()
            .icon(QFileInfo(str(self.task.input_file)))
            .pixmap(32, 32)
        )

        self.filePathLabel = BodyLabel(str(self.task.input_file))

        if self.progressBar_type == "determinate":
            self.progressBar = IndeterminateProgressBar()
        else:
            self.progressBar = ProgressBar()

        self.statusLabel = CaptionLabel(
            status_text(
                self.task.status, self.globalText.InProgress.format(self.task_type)
            )
        )

        self.openFolderBtn = TransparentToolButton(FluentIcon.FOLDER, self)
        self.openFolderBtn.setToolTip(self.globalText.OpenFolder)
        self.openFolderBtn.setVisible(self.task.status == TaskStatus.Succeeded)
        self.openFolderBtn.clicked.connect(self.openFolder)

        self.cancelBtn = TransparentToolButton(FluentIcon.CLOSE, self)
        self.cancelBtn.setToolTip(self.globalText.Cancel + str(self.task_type))
        self.cancelBtn.setVisible(
            self.task.status == TaskStatus.Processing
            or self.task.status == TaskStatus.Waiting
        )
        self.cancelBtn.clicked.connect(self.cancelTranslate)

        self.retryBtn = TransparentToolButton(FluentIcon.SYNC, self)
        self.retryBtn.setToolTip(self.globalText.Retry + str(self.task_type))
        self.retryBtn.setVisible(self.task.status == TaskStatus.Failed)
        self.retryBtn.clicked.connect(self.retryTranslate)

        self.removeBtn = TransparentToolButton(FluentIcon.DELETE, self)
        self.removeBtn.setToolTip(self.globalText.RemoveTask)
        self.removeBtn.setDisabled(True)
        self.removeBtn.clicked.connect(self.removeTask)

        self.hBoxLayout.addWidget(self.imageLabel)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        self.hBoxLayout.addSpacing(20)
        self.hBoxLayout.addWidget(self.openFolderBtn)
        self.hBoxLayout.addWidget(self.cancelBtn)
        self.hBoxLayout.addWidget(self.retryBtn)
        self.hBoxLayout.addWidget(self.removeBtn)

        self.vBoxLayout.setSpacing(5)
        self.vBoxLayout.setContentsMargins(0, 0, 0, 0)
        self.vBoxLayout.addWidget(self.fileNameLabel)
        self.vBoxLayout.addLayout(self.infoLayout)
        self.vBoxLayout.addWidget(self.progressBar)

        self.infoLayout.addWidget(self.statusLabel)

        self.setMinimumHeight(75)

    def updateStatusStyle(self, statusPill):
        """更新状态标签样式"""
        if self.task.status == TaskStatus.Waiting:
            statusPill.setProperty("isSecondary", True)
        elif self.task.status == TaskStatus.Processing:
            statusPill.setProperty("isPrimary", True)
        elif self.task.status == TaskStatus.Succeeded:
            statusPill.setProperty("isSuccess", True)
        elif self.task.status == TaskStatus.Failed:
            statusPill.setProperty("isError", True)
        statusPill.setStyle(statusPill.style())

    def updateStatus(self, status, success=True, error_message=""):
        """更新状态"""
        self.task.status = status
        if not success:
            self.task.error_message = error_message
        self.statusLabel.setText(
            status_text(status, self.globalText.InProgress.format(self.task_type))
        )

        # 显示/隐藏按钮
        self.openFolderBtn.setVisible(status == TaskStatus.Succeeded)
        self.cancelBtn.setVisible(status == TaskStatus.Processing)
        self.retryBtn.setVisible(status == TaskStatus.Failed)

        # 设置按钮可用性
        self.removeBtn.setEnabled(
            status == TaskStatus.Succeeded
            or status == TaskStatus.Failed
            or status == TaskStatus.Cancelled
        )

        # 进度条
        self.progressBar.setVisible(status == TaskStatus.Processing)

    def updateProgress(self, progress, input_file):
        """更新进度"""
        if self.progressBar_type == "determinate":
            return
        self.task.progress = progress
        if input_file and not self.task.input_file:
            self.task.input_file = input_file
            self.fileNameLabel.setText(str(self.task.input_file))

        self.progressBar.setValue(progress)

        # 更新状态标签
        self.statusLabel.setText(
            status_text(
                self.task.status, self.globalText.InProgress.format(self.task_type)
            )
        )

    def openFolder(self):
        """打开文件夹"""
        if os.path.exists(self.task.output_file):
            # 打开文件所在文件夹并选中文件
            if platform.system() == "Windows":
                subprocess.Popen(
                    f'explorer /select,"{self.task.output_file}"',
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
            elif platform.system() == "Darwin":
                subprocess.Popen(["open", "-R", self.task.output_file])
            else:
                # Linux系统
                folder_path = os.path.dirname(self.task.output_file)
                subprocess.Popen(["xdg-open", folder_path])
        else:
            # 如果文件不存在，只打开文件夹
            folder_path = os.path.dirname(self.task.output_file)
            if os.path.exists(folder_path):
                QDesktopServices.openUrl(QUrl.fromLocalFile(folder_path))

    def cancelTranslate(self):
        """取消下载 - 异步版本"""
        # 添加确认对话框
        box = MessageBox(
            self.globalText.ConfirmCancellation,
            self.globalText.AYSYWTCT + str(self.task_type) + self.globalText.Task,
            self.window(),
        )
        box.yesButton.setText(self.globalText.OK)
        box.cancelButton.setText(self.globalText.Cancel)
        if box.exec():
            # 如果任务正在提取，找到对应的提取线程并取消
            if self.task_thread:
                # 连接取消完成信号
                self.task_thread.cancelled_signal.connect(self._onCancellationComplete)

                # 立即更新UI状态，不等待线程结束
                self.task.status = TaskStatus.Cancelling
                self.updateStatus(TaskStatus.Cancelling)

                # 异步取消，不阻塞界面
                self.task_thread.cancel()

                # 禁用取消按钮，避免重复点击
                self.cancelBtn.setEnabled(False)
            else:
                # 如果没有线程引用，直接更新状态
                self._completeCancellation()

    def _onCancellationComplete(self):
        """取消完成后的处理"""
        self._completeCancellation()

        # 断开信号连接，避免重复调用
        if self.task_thread:
            try:
                self.task_thread.cancelled_signal.disconnect(
                    self._onCancellationComplete
                )
            except Exception:
                pass

    def _completeCancellation(self):
        """完成取消操作"""
        # 更新任务状态
        self.task.status = TaskStatus.Cancelled

        # 更新UI状态
        self.updateStatus(TaskStatus.Cancelled)

        # 恢复按钮状态
        self.removeBtn.setDisabled(False)
        self.retryBtn.setVisible(True)
        self.cancelBtn.setVisible(False)

        # 重新启用取消按钮（虽然它已经隐藏了）
        self.cancelBtn.setEnabled(True)

        # 显示取消提示
        event_bus.notification_service.show_info(
            str(self.task_type) + self.globalText.Cancelled,
            self.globalText.Task2
            + f" '{self.task.input_file}' "
            + self.globalText.HasBeenCancelled,
        )

    def retryTranslate(self):
        """重新下载"""
        # 发送重新下载信号
        self.retryTaskSignal.emit(self.task.id)

    def removeTask(self):
        """移除任务"""
        # 发送移除任务信号
        self.removeTaskSignal.emit(self.task.id)


class TaskCardBase(CardWidget):
    """任务卡片基类

    子类需实现 removeTask()，并调用 self._updateStatus() 完成布局后的状态初始化。
    """

    deleted = Signal()
    checkedChanged = Signal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.checkBox = CheckBox()
        self.checkBox.setFixedSize(23, 23)
        self.setSelectionMode(False)

        self.checkBox.stateChanged.connect(self._onCheckedChanged)

    def setSelectionMode(self, enter: bool):
        self.isSelectionMode = enter
        self.checkBox.setVisible(enter)
        if not enter:
            self.checkBox.setChecked(False)

        self.update()

    def isChecked(self):
        return self.checkBox.isChecked()

    def setChecked(self, checked):
        if checked == self.isChecked():
            return

        self.checkBox.setChecked(checked)
        self.update()

    def removeTask(self, deleteFile=False):
        raise NotImplementedError

    def mouseReleaseEvent(self, e):
        super().mouseReleaseEvent(e)
        if self.isSelectionMode:
            self.setChecked(not self.isChecked())
        else:
            self.setSelectionMode(True)
            self.setChecked(True)

    def _onDeleteButtonClicked(self):
        w = DeleteTaskDialog(self.window(), deleteOnClose=False)
        w.deleteFileCheckBox.setChecked(False)

        if w.exec():
            self.removeTask(w.deleteFileCheckBox.isChecked())

        w.deleteLater()

    def _onCheckedChanged(self):
        self.setChecked(self.checkBox.isChecked())
        self.checkedChanged.emit(self.checkBox.isChecked())
        self.update()

    def paintEvent(self, e):
        if not (self.isSelectionMode and self.isChecked()):
            return super().paintEvent(e)

        painter = QPainter(self)
        painter.setRenderHints(QPainter.Antialiasing)

        r = self.borderRadius
        painter.setPen(QPen(themeColor(), 2))
        painter.setBrush(
            QColor(255, 255, 255, 15) if isDarkTheme() else QColor(0, 0, 0, 8)
        )
        painter.drawRoundedRect(self.rect().adjusted(2, 2, -2, -2), r, r)


class DeleteTaskDialog(MessageBoxBase):
    """删除任务确认对话框（对齐 Easy-FFmpeg）"""

    def __init__(self, parent=None, showCheckBox=True, deleteOnClose=True):
        super().__init__(parent)
        self.globalText = Text()
        self.titleLabel = SubtitleLabel(self.globalText.DeleteTask, self)
        self.contentLabel = BodyLabel(self.globalText.ConfirmDeleteTask, self)
        self.deleteFileCheckBox = CheckBox(self.globalText.DeleteFiles, self)

        self.deleteFileCheckBox.setVisible(showCheckBox)

        if deleteOnClose:
            self.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose, True)

        self._initWidgets()

    def _initWidgets(self):
        self.deleteFileCheckBox.setChecked(True)
        self.widget.setMinimumWidth(330)

        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self.titleLabel)
        layout.addSpacing(12)
        layout.addWidget(self.contentLabel)
        layout.addSpacing(10)
        layout.addWidget(self.deleteFileCheckBox)
        self.viewLayout.addLayout(layout)
