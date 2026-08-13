from PySide6.QtCore import QDateTime, QFileInfo, Qt, QUrl
from PySide6.QtGui import QDesktopServices, QFont, QIcon
from PySide6.QtWidgets import QFileIconProvider, QHBoxLayout, QVBoxLayout

from libs.qfluentwidgets_pro import (
    BodyLabel,
    CaptionLabel,
    FilledToolButton,
    FluentIcon,
    IconWidget,
    ImageLabel,
    ProgressBar,
    ToolButton,
    ToolTipFilter,
    setFont,
)

from ..common.event_bus import event_bus
from ..common.task_status import TaskStatus, status_text
from ..common.text import Text
from ..common.utils import showInFolder
from ..service.ffmpeg_service import FFmpegTask
from .base_task_card import BaseItemWidget, TaskCardBase
from .dialog import (
    FFmpegProgressDialog,
    ReleaseProgressDialog,
    TranslateProgressDialog,
    WhisperProgressDialog,
)


class TranslateItemWidget(BaseItemWidget):
    """翻译任务项组件"""

    def __init__(
        self,
        task,
        progressBar_type="determinate",
        task_type=None,
        ai_model=None,
        parent=None,
    ):
        globalText = Text()
        super().__init__(
            task, progressBar_type, task_type or globalText.Translate, parent
        )
        self.globalText = globalText
        self.ai_model = ai_model
        self.setImage(ai_model)
        self.clicked.connect(self.handleClick)

    def setImage(self, ai_model):
        """设置图标"""
        self.imageLabel.setImage(
            QIcon(f":/app/images/icons/{ai_model}.svg").pixmap(32, 32)
        )

    def handleClick(self):
        """处理点击事件"""
        dialog = TranslateProgressDialog(task=self.task, parent=self.window())
        dialog.exec()


class FFmpegItemWidget(BaseItemWidget):
    """压制任务项组件"""

    def __init__(self, task, progressBar_type="common", task_type=None, parent=None):
        globalText = Text()
        super().__init__(task, progressBar_type, task_type or globalText.Encode, parent)
        self.globalText = globalText
        self.clicked.connect(self.handleClick)

    def handleClick(self):
        """处理点击事件"""
        dialog = FFmpegProgressDialog(task=self.task, parent=self.window())
        dialog.exec()


class ReleaseItemWidget(BaseItemWidget):
    """B站上传任务项组件"""

    def __init__(self, task, progressBar_type="common", task_type=None, parent=None):
        globalText = Text()
        super().__init__(task, progressBar_type, task_type or globalText.Upload, parent)
        self.globalText = globalText
        self.setImage()
        self.clicked.connect(self.handleClick)

    def setImage(self):
        """设置图标"""
        self.imageLabel.setImage(QIcon(":/app/images/logo/bilibili.svg").pixmap(32, 32))

    def handleClick(self):
        """处理点击事件"""
        dialog = ReleaseProgressDialog(task=self.task, parent=self.window())
        dialog.exec()


class WhisperItemWidget(BaseItemWidget):
    """语音识别任务项组件"""

    def __init__(self, task, progressBar_type="common", task_type=None, parent=None):
        globalText = Text()
        super().__init__(
            task, progressBar_type, task_type or globalText.Recognize, parent
        )
        self.globalText = globalText
        self.clicked.connect(self.handleClick)

    def handleClick(self):
        """处理点击事件"""
        dialog = WhisperProgressDialog(task=self.task, parent=self.window())
        dialog.exec()


class FFmpegTaskCard(TaskCardBase):
    """FFmpeg 压制任务卡片（对齐 Easy-FFmpeg：状态按钮矩阵 + 压制实时信息）"""

    def __init__(self, task: FFmpegTask, parent=None):
        super().__init__(parent=parent)
        self.globalText = Text()
        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()
        self.infoLayout = QHBoxLayout()

        self.task = task
        self.task_id = task.task_id
        self.status = TaskStatus.Waiting
        # two-pass 阶段文案（预留，当前流程未启用）
        self.stageText = ""
        self.imageLabel = ImageLabel()
        self.fileNameLabel = BodyLabel(task.fileName)
        self.progressBar = ProgressBar()

        self.statusIcon = IconWidget(FluentIcon.TAG)
        self.statusLabel = CaptionLabel(status_text(self.status))

        self.sizeIcon = IconWidget(FluentIcon.BOOK_SHELF)
        self.sizeLabel = CaptionLabel("0MB")
        self.timeIcon = IconWidget(FluentIcon.STOP_WATCH)
        self.timeLabel = CaptionLabel("0.0s")
        self.bitrateIcon = IconWidget(FluentIcon.IOT)
        self.bitrateLabel = CaptionLabel("0kbits/s")
        self.speedIcon = IconWidget(FluentIcon.SPEED_HIGH)
        self.speedLabel = CaptionLabel("0x")
        self.finishTimeIcon = IconWidget(FluentIcon.CALENDAR)
        self.finishTimeLabel = CaptionLabel("")

        self.openFolderButton = ToolButton(FluentIcon.FOLDER)
        self.deleteButton = FilledToolButton(FluentIcon.DELETE)

        self._initWidget()

    def _initWidget(self):
        self.imageLabel.setImage(
            QFileIconProvider().icon(QFileInfo(self.task.videoPath)).pixmap(32, 32)
        )
        for icon in (
            self.statusIcon,
            self.sizeIcon,
            self.timeIcon,
            self.bitrateIcon,
            self.speedIcon,
            self.finishTimeIcon,
        ):
            icon.setFixedSize(16, 16)

        self.openFolderButton.setToolTip(self.globalText.ShowInFolder)
        self.openFolderButton.setToolTipDuration(3000)
        self.openFolderButton.installEventFilter(ToolTipFilter(self.openFolderButton))
        self.cancelButton = ToolButton(FluentIcon.CLOSE)
        self.cancelButton.setToolTip(self.globalText.CancelTask)
        self.cancelButton.setToolTipDuration(3000)
        self.cancelButton.installEventFilter(ToolTipFilter(self.cancelButton))
        self.retryButton = ToolButton(FluentIcon.SYNC)
        self.retryButton.setToolTip(self.globalText.RetryTask)
        self.retryButton.setToolTipDuration(3000)
        self.retryButton.installEventFilter(ToolTipFilter(self.retryButton))
        self.logButton = ToolButton(FluentIcon.COMMAND_PROMPT)
        self.logButton.setToolTip(self.globalText.ViewLog)
        self.logButton.setToolTipDuration(3000)
        self.logButton.installEventFilter(ToolTipFilter(self.logButton))
        self.deleteButton.setColorScheme("error")
        self.deleteButton.setToolTip(self.globalText.RemoveTask)
        self.deleteButton.setToolTipDuration(3000)
        self.deleteButton.installEventFilter(ToolTipFilter(self.deleteButton))

        setFont(self.fileNameLabel, 18, QFont.Weight.Bold)
        self.fileNameLabel.setWordWrap(True)

        self._initLayout()
        self._connectSignalToSlot()
        self._updateStatus()

    def _initLayout(self):
        self.hBoxLayout.setContentsMargins(20, 11, 20, 11)
        self.hBoxLayout.addWidget(self.checkBox)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addWidget(self.imageLabel)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        self.hBoxLayout.addSpacing(20)
        self.hBoxLayout.addWidget(self.openFolderButton)
        self.hBoxLayout.addWidget(self.cancelButton)
        self.hBoxLayout.addWidget(self.retryButton)
        self.hBoxLayout.addWidget(self.logButton)
        self.hBoxLayout.addWidget(self.deleteButton)

        self.vBoxLayout.setSpacing(5)
        self.vBoxLayout.setContentsMargins(0, 0, 0, 0)
        self.vBoxLayout.addWidget(self.fileNameLabel)
        self.vBoxLayout.addLayout(self.infoLayout)
        self.vBoxLayout.addWidget(self.progressBar)

        self.infoLayout.setContentsMargins(0, 0, 0, 0)
        self.infoLayout.setSpacing(3)
        self.infoLayout.addWidget(self.statusIcon)
        self.infoLayout.addWidget(self.statusLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addSpacing(5)
        self.infoLayout.addWidget(self.sizeIcon)
        self.infoLayout.addWidget(self.sizeLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addSpacing(5)
        self.infoLayout.addWidget(self.timeIcon)
        self.infoLayout.addWidget(self.timeLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addSpacing(5)
        self.infoLayout.addWidget(self.bitrateIcon)
        self.infoLayout.addWidget(self.bitrateLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addSpacing(5)
        self.infoLayout.addWidget(self.speedIcon)
        self.infoLayout.addWidget(self.speedLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addSpacing(5)
        self.infoLayout.addWidget(self.finishTimeIcon)
        self.infoLayout.addWidget(self.finishTimeLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addStretch(1)

    def _connectSignalToSlot(self):
        self.openFolderButton.clicked.connect(self._onOpenButtonClicked)
        self.cancelButton.clicked.connect(self._onCancelButtonClicked)
        self.retryButton.clicked.connect(self._onRetryButtonClicked)
        self.logButton.clicked.connect(self._onLogButtonClicked)
        self.deleteButton.clicked.connect(self._onDeleteButtonClicked)

    def _updateStatus(self, status: TaskStatus = TaskStatus.Waiting):
        self.status = status
        text = status_text(status)
        # two-pass 阶段文案仅在压制中显示
        if status == TaskStatus.Processing and self.stageText:
            text = f"{text} · {self.stageText}"
        self.statusLabel.setText(text)
        if status == TaskStatus.Waiting:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status in (TaskStatus.Pending, TaskStatus.Processing):
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(True)
        elif status == TaskStatus.Cancelling:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Cancelled:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Failed:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(True)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Succeeded:
            self.openFolderButton.setVisible(True)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)

    def _updateInfo(self, size, time, bitrate, speed):
        self.sizeLabel.setText(str(size))
        self.timeLabel.setText(str(time) + "s")
        self.bitrateLabel.setText(str(bitrate))
        self.speedLabel.setText(str(speed) + "x")

    def _updateInfoVisible(self, visible: bool):
        self.sizeIcon.setVisible(visible)
        self.sizeLabel.setVisible(visible)
        self.timeIcon.setVisible(visible)
        self.timeLabel.setVisible(visible)
        self.bitrateIcon.setVisible(visible)
        self.bitrateLabel.setVisible(visible)
        self.speedIcon.setVisible(visible)
        self.speedLabel.setVisible(visible)
        self.finishTimeIcon.setVisible(not visible)
        self.finishTimeLabel.setVisible(not visible)
        if not visible:
            self.finishTimeLabel.setText(
                QDateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            )

    def updateTask(
        self,
        progress=0,
        status=TaskStatus.Waiting,
        size="0KiB",
        time=0.0,
        bitrate="0kbits/s",
        speed=0.0,
    ):
        self.progressBar.setValue(progress)
        self._updateStatus(status)
        self._updateInfo(size, time, bitrate, speed)

    def updateStage(self, stage_text: str):
        """更新 two-pass 阶段文案（当前流程未启用，保留接口）"""
        self.stageText = stage_text
        if self.status == TaskStatus.Processing and stage_text:
            self.statusLabel.setText(f"{status_text(self.status)} · {stage_text}")
        else:
            self.statusLabel.setText(status_text(self.status))

    def removeTask(self, deleteFile=False):
        event_bus.deleteTaskSig.emit(self.task.task_id, deleteFile)

    def _open_folder(self):
        """在文件管理器中显示输出文件（对齐 Easy-FFmpeg）"""
        showInFolder(self.task.output_path)

    def _onOpenButtonClicked(self):
        self._open_folder()

    def _onCancelButtonClicked(self):
        event_bus.cancelTaskSig.emit(self.task.task_id)

    def _onLogButtonClicked(self):
        if self.task.logPath:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self.task.logPath))

    def _onRetryButtonClicked(self):
        event_bus.retryTaskSig.emit(self.task.task_id)


class OcrTaskCard(TaskCardBase):
    """OCR 字幕提取任务卡片（简化版：状态 + 进度 + 完成时间）"""

    def __init__(self, task, parent=None):
        super().__init__(parent=parent)
        self.globalText = Text()
        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()
        self.infoLayout = QHBoxLayout()

        self.task = task
        self.task_id = task.task_id
        self.status = TaskStatus.Waiting
        self.imageLabel = ImageLabel()
        self.fileNameLabel = BodyLabel(task.fileName)
        self.progressBar = ProgressBar()

        self.statusIcon = IconWidget(FluentIcon.TAG)
        self.statusLabel = CaptionLabel(status_text(self.status))
        self.finishTimeIcon = IconWidget(FluentIcon.CALENDAR)
        self.finishTimeLabel = CaptionLabel("")

        self.openFolderButton = ToolButton(FluentIcon.FOLDER)
        self.deleteButton = FilledToolButton(FluentIcon.DELETE)

        self._initWidget()

    def _initWidget(self):
        self.imageLabel.setImage(
            QFileIconProvider().icon(QFileInfo(self.task.videoPath)).pixmap(32, 32)
        )
        for icon in (self.statusIcon, self.finishTimeIcon):
            icon.setFixedSize(16, 16)

        self.openFolderButton.setToolTip(self.globalText.ShowInFolder)
        self.openFolderButton.setToolTipDuration(3000)
        self.openFolderButton.installEventFilter(ToolTipFilter(self.openFolderButton))
        self.cancelButton = ToolButton(FluentIcon.CLOSE)
        self.cancelButton.setToolTip(self.globalText.CancelTask)
        self.cancelButton.setToolTipDuration(3000)
        self.cancelButton.installEventFilter(ToolTipFilter(self.cancelButton))
        self.retryButton = ToolButton(FluentIcon.SYNC)
        self.retryButton.setToolTip(self.globalText.RetryTask)
        self.retryButton.setToolTipDuration(3000)
        self.retryButton.installEventFilter(ToolTipFilter(self.retryButton))
        self.logButton = ToolButton(FluentIcon.COMMAND_PROMPT)
        self.logButton.setToolTip(self.globalText.ViewLog)
        self.logButton.setToolTipDuration(3000)
        self.logButton.installEventFilter(ToolTipFilter(self.logButton))
        self.deleteButton.setColorScheme("error")
        self.deleteButton.setToolTip(self.globalText.RemoveTask)
        self.deleteButton.setToolTipDuration(3000)
        self.deleteButton.installEventFilter(ToolTipFilter(self.deleteButton))

        setFont(self.fileNameLabel, 18, QFont.Weight.Bold)
        self.fileNameLabel.setWordWrap(True)

        self._initLayout()
        self._connectSignalToSlot()
        self._updateStatus()

    def _initLayout(self):
        self.hBoxLayout.setContentsMargins(20, 11, 20, 11)
        self.hBoxLayout.addWidget(self.checkBox)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addWidget(self.imageLabel)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        self.hBoxLayout.addSpacing(20)
        self.hBoxLayout.addWidget(self.openFolderButton)
        self.hBoxLayout.addWidget(self.cancelButton)
        self.hBoxLayout.addWidget(self.retryButton)
        self.hBoxLayout.addWidget(self.logButton)
        self.hBoxLayout.addWidget(self.deleteButton)

        self.vBoxLayout.setSpacing(5)
        self.vBoxLayout.setContentsMargins(0, 0, 0, 0)
        self.vBoxLayout.addWidget(self.fileNameLabel)
        self.vBoxLayout.addLayout(self.infoLayout)
        self.vBoxLayout.addWidget(self.progressBar)

        self.infoLayout.setContentsMargins(0, 0, 0, 0)
        self.infoLayout.addWidget(self.statusIcon)
        self.infoLayout.addWidget(self.statusLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addStretch(1)
        self.infoLayout.addWidget(self.finishTimeIcon)
        self.infoLayout.addWidget(self.finishTimeLabel, 0, Qt.AlignmentFlag.AlignLeft)

    def _connectSignalToSlot(self):
        self.openFolderButton.clicked.connect(self._onOpenButtonClicked)
        self.cancelButton.clicked.connect(self._onCancelButtonClicked)
        self.retryButton.clicked.connect(self._onRetryButtonClicked)
        self.logButton.clicked.connect(self._onLogButtonClicked)
        self.deleteButton.clicked.connect(self._onDeleteButtonClicked)

    def _updateStatus(self, status: TaskStatus = TaskStatus.Waiting):
        self.status = status
        self.statusLabel.setText(status_text(status))
        if status == TaskStatus.Waiting:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status in (TaskStatus.Pending, TaskStatus.Processing):
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(True)
        elif status == TaskStatus.Cancelling:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Cancelled:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Failed:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(True)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Succeeded:
            self.openFolderButton.setVisible(True)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)

    def _updateInfoVisible(self, visible: bool):
        self.finishTimeIcon.setVisible(not visible)
        self.finishTimeLabel.setVisible(not visible)
        if not visible:
            self.finishTimeLabel.setText(
                QDateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            )

    def updateTask(
        self,
        progress=0,
        status=TaskStatus.Waiting,
        size="",
        time=0.0,
        bitrate="",
        speed=0.0,
    ):
        """更新任务状态（兼容 TaskInterface 的 7 参上报；OCR 无压制实时信息）"""
        self.progressBar.setValue(progress)
        self._updateStatus(status)

    def removeTask(self, deleteFile=False):
        event_bus.deleteTaskSig.emit(self.task.task_id, deleteFile)

    def _open_folder(self):
        """在文件管理器中显示输出文件（对齐 Easy-FFmpeg）"""
        showInFolder(self.task.outputFile)

    def _onOpenButtonClicked(self):
        self._open_folder()

    def _onCancelButtonClicked(self):
        event_bus.cancelTaskSig.emit(self.task.task_id)

    def _onLogButtonClicked(self):
        if self.task.logPath:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self.task.logPath))

    def _onRetryButtonClicked(self):
        event_bus.retryTaskSig.emit(self.task.task_id)


class WhisperTaskCard(TaskCardBase):
    """Whisper 语音识别任务卡片（简化版：状态 + 进度 + 完成时间）"""

    def __init__(self, task, parent=None):
        super().__init__(parent=parent)
        self.globalText = Text()
        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()
        self.infoLayout = QHBoxLayout()

        self.task = task
        self.task_id = task.task_id
        self.status = TaskStatus.Waiting
        self.imageLabel = ImageLabel()
        self.fileNameLabel = BodyLabel(task.fileName)
        self.progressBar = ProgressBar()

        self.statusIcon = IconWidget(FluentIcon.TAG)
        self.statusLabel = CaptionLabel(status_text(self.status))
        self.finishTimeIcon = IconWidget(FluentIcon.CALENDAR)
        self.finishTimeLabel = CaptionLabel("")

        self.openFolderButton = ToolButton(FluentIcon.FOLDER)
        self.deleteButton = FilledToolButton(FluentIcon.DELETE)

        self._initWidget()

    def _initWidget(self):
        self.imageLabel.setImage(
            QFileIconProvider().icon(QFileInfo(self.task.input_file)).pixmap(32, 32)
        )
        for icon in (self.statusIcon, self.finishTimeIcon):
            icon.setFixedSize(16, 16)

        self.openFolderButton.setToolTip(self.globalText.ShowInFolder)
        self.openFolderButton.setToolTipDuration(3000)
        self.openFolderButton.installEventFilter(ToolTipFilter(self.openFolderButton))
        self.cancelButton = ToolButton(FluentIcon.CLOSE)
        self.cancelButton.setToolTip(self.globalText.CancelTask)
        self.cancelButton.setToolTipDuration(3000)
        self.cancelButton.installEventFilter(ToolTipFilter(self.cancelButton))
        self.retryButton = ToolButton(FluentIcon.SYNC)
        self.retryButton.setToolTip(self.globalText.RetryTask)
        self.retryButton.setToolTipDuration(3000)
        self.retryButton.installEventFilter(ToolTipFilter(self.retryButton))
        self.logButton = ToolButton(FluentIcon.COMMAND_PROMPT)
        self.logButton.setToolTip(self.globalText.ViewLog)
        self.logButton.setToolTipDuration(3000)
        self.logButton.installEventFilter(ToolTipFilter(self.logButton))
        self.deleteButton.setColorScheme("error")
        self.deleteButton.setToolTip(self.globalText.RemoveTask)
        self.deleteButton.setToolTipDuration(3000)
        self.deleteButton.installEventFilter(ToolTipFilter(self.deleteButton))

        setFont(self.fileNameLabel, 18, QFont.Weight.Bold)
        self.fileNameLabel.setWordWrap(True)

        self._initLayout()
        self._connectSignalToSlot()
        self._updateStatus()

    def _initLayout(self):
        self.hBoxLayout.setContentsMargins(20, 11, 20, 11)
        self.hBoxLayout.addWidget(self.checkBox)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addWidget(self.imageLabel)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        self.hBoxLayout.addSpacing(20)
        self.hBoxLayout.addWidget(self.openFolderButton)
        self.hBoxLayout.addWidget(self.cancelButton)
        self.hBoxLayout.addWidget(self.retryButton)
        self.hBoxLayout.addWidget(self.logButton)
        self.hBoxLayout.addWidget(self.deleteButton)

        self.vBoxLayout.setSpacing(5)
        self.vBoxLayout.setContentsMargins(0, 0, 0, 0)
        self.vBoxLayout.addWidget(self.fileNameLabel)
        self.vBoxLayout.addLayout(self.infoLayout)
        self.vBoxLayout.addWidget(self.progressBar)

        self.infoLayout.setContentsMargins(0, 0, 0, 0)
        self.infoLayout.addWidget(self.statusIcon)
        self.infoLayout.addWidget(self.statusLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addStretch(1)
        self.infoLayout.addWidget(self.finishTimeIcon)
        self.infoLayout.addWidget(self.finishTimeLabel, 0, Qt.AlignmentFlag.AlignLeft)

    def _connectSignalToSlot(self):
        self.openFolderButton.clicked.connect(self._onOpenButtonClicked)
        self.cancelButton.clicked.connect(self._onCancelButtonClicked)
        self.retryButton.clicked.connect(self._onRetryButtonClicked)
        self.logButton.clicked.connect(self._onLogButtonClicked)
        self.deleteButton.clicked.connect(self._onDeleteButtonClicked)

    def _updateStatus(self, status: TaskStatus = TaskStatus.Waiting):
        self.status = status
        self.statusLabel.setText(status_text(status))
        if status == TaskStatus.Waiting:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status in (TaskStatus.Pending, TaskStatus.Processing):
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(True)
        elif status == TaskStatus.Cancelling:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Cancelled:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Failed:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(True)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Succeeded:
            self.openFolderButton.setVisible(True)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)

    def _updateInfoVisible(self, visible: bool):
        self.finishTimeIcon.setVisible(not visible)
        self.finishTimeLabel.setVisible(not visible)
        if not visible:
            self.finishTimeLabel.setText(
                QDateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            )

    def updateTask(
        self,
        progress=0,
        status=TaskStatus.Waiting,
        size="",
        time=0.0,
        bitrate="",
        speed=0.0,
    ):
        """更新任务状态（兼容 TaskInterface 的 7 参上报；Whisper 无压制实时信息）"""
        self.progressBar.setValue(progress)
        self._updateStatus(status)

    def removeTask(self, deleteFile=False):
        event_bus.deleteTaskSig.emit(self.task.task_id, deleteFile)

    def _open_folder(self):
        """在文件管理器中显示输出文件（对齐 Easy-FFmpeg）"""
        showInFolder(self.task.output_file)

    def _onOpenButtonClicked(self):
        self._open_folder()

    def _onCancelButtonClicked(self):
        event_bus.cancelTaskSig.emit(self.task.task_id)

    def _onLogButtonClicked(self):
        if self.task.logPath:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self.task.logPath))

    def _onRetryButtonClicked(self):
        event_bus.retryTaskSig.emit(self.task.task_id)


class TranslateTaskCard(TaskCardBase):
    """翻译任务卡片（简化版：状态 + 进度 + 完成时间）"""

    def __init__(self, task, parent=None):
        super().__init__(parent=parent)
        self.globalText = Text()
        self.hBoxLayout = QHBoxLayout(self)
        self.vBoxLayout = QVBoxLayout()
        self.infoLayout = QHBoxLayout()

        self.task = task
        self.task_id = task.task_id
        self.status = TaskStatus.Waiting
        self.imageLabel = ImageLabel()
        self.fileNameLabel = BodyLabel(task.fileName)
        self.progressBar = ProgressBar()

        self.statusIcon = IconWidget(FluentIcon.TAG)
        self.statusLabel = CaptionLabel(status_text(self.status))
        self.finishTimeIcon = IconWidget(FluentIcon.CALENDAR)
        self.finishTimeLabel = CaptionLabel("")

        self.openFolderButton = ToolButton(FluentIcon.FOLDER)
        self.deleteButton = FilledToolButton(FluentIcon.DELETE)

        self._initWidget()

    def _initWidget(self):
        self.imageLabel.setImage(
            QIcon(f":/app/images/icons/{self.task.AI}.svg").pixmap(32, 32)
        )
        for icon in (self.statusIcon, self.finishTimeIcon):
            icon.setFixedSize(16, 16)

        self.openFolderButton.setToolTip(self.globalText.ShowInFolder)
        self.openFolderButton.setToolTipDuration(3000)
        self.openFolderButton.installEventFilter(ToolTipFilter(self.openFolderButton))
        self.cancelButton = ToolButton(FluentIcon.CLOSE)
        self.cancelButton.setToolTip(self.globalText.CancelTask)
        self.cancelButton.setToolTipDuration(3000)
        self.cancelButton.installEventFilter(ToolTipFilter(self.cancelButton))
        self.retryButton = ToolButton(FluentIcon.SYNC)
        self.retryButton.setToolTip(self.globalText.RetryTask)
        self.retryButton.setToolTipDuration(3000)
        self.retryButton.installEventFilter(ToolTipFilter(self.retryButton))
        self.logButton = ToolButton(FluentIcon.COMMAND_PROMPT)
        self.logButton.setToolTip(self.globalText.ViewLog)
        self.logButton.setToolTipDuration(3000)
        self.logButton.installEventFilter(ToolTipFilter(self.logButton))
        self.deleteButton.setColorScheme("error")
        self.deleteButton.setToolTip(self.globalText.RemoveTask)
        self.deleteButton.setToolTipDuration(3000)
        self.deleteButton.installEventFilter(ToolTipFilter(self.deleteButton))

        setFont(self.fileNameLabel, 18, QFont.Weight.Bold)
        self.fileNameLabel.setWordWrap(True)

        self._initLayout()
        self._connectSignalToSlot()
        self._updateStatus()

    def _initLayout(self):
        self.hBoxLayout.setContentsMargins(20, 11, 20, 11)
        self.hBoxLayout.addWidget(self.checkBox)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addWidget(self.imageLabel)
        self.hBoxLayout.addSpacing(5)
        self.hBoxLayout.addLayout(self.vBoxLayout)
        self.hBoxLayout.addSpacing(20)
        self.hBoxLayout.addWidget(self.openFolderButton)
        self.hBoxLayout.addWidget(self.cancelButton)
        self.hBoxLayout.addWidget(self.retryButton)
        self.hBoxLayout.addWidget(self.logButton)
        self.hBoxLayout.addWidget(self.deleteButton)

        self.vBoxLayout.setSpacing(5)
        self.vBoxLayout.setContentsMargins(0, 0, 0, 0)
        self.vBoxLayout.addWidget(self.fileNameLabel)
        self.vBoxLayout.addLayout(self.infoLayout)
        self.vBoxLayout.addWidget(self.progressBar)

        self.infoLayout.setContentsMargins(0, 0, 0, 0)
        self.infoLayout.addWidget(self.statusIcon)
        self.infoLayout.addWidget(self.statusLabel, 0, Qt.AlignmentFlag.AlignLeft)
        self.infoLayout.addStretch(1)
        self.infoLayout.addWidget(self.finishTimeIcon)
        self.infoLayout.addWidget(self.finishTimeLabel, 0, Qt.AlignmentFlag.AlignLeft)

    def _connectSignalToSlot(self):
        self.openFolderButton.clicked.connect(self._onOpenButtonClicked)
        self.cancelButton.clicked.connect(self._onCancelButtonClicked)
        self.retryButton.clicked.connect(self._onRetryButtonClicked)
        self.logButton.clicked.connect(self._onLogButtonClicked)
        self.deleteButton.clicked.connect(self._onDeleteButtonClicked)

    def _updateStatus(self, status: TaskStatus = TaskStatus.Waiting):
        self.status = status
        self.statusLabel.setText(status_text(status))
        if status == TaskStatus.Waiting:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status in (TaskStatus.Pending, TaskStatus.Processing):
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(True)
        elif status == TaskStatus.Cancelling:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(True)
            self.cancelButton.setEnabled(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(False)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Cancelled:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.cancelButton.setEnabled(True)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Failed:
            self.openFolderButton.setVisible(False)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(True)
            self.logButton.setVisible(True)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)
        elif status == TaskStatus.Succeeded:
            self.openFolderButton.setVisible(True)
            self.cancelButton.setVisible(False)
            self.retryButton.setVisible(False)
            self.logButton.setVisible(False)
            self.deleteButton.setVisible(True)
            self._updateInfoVisible(False)

    def _updateInfoVisible(self, visible: bool):
        self.finishTimeIcon.setVisible(not visible)
        self.finishTimeLabel.setVisible(not visible)
        if not visible:
            self.finishTimeLabel.setText(
                QDateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            )

    def updateTask(
        self,
        progress=0,
        status=TaskStatus.Waiting,
        size="",
        time=0.0,
        bitrate="",
        speed=0.0,
    ):
        """更新任务状态（兼容 TaskInterface 的 7 参上报；翻译无压制实时信息）"""
        self.progressBar.setValue(progress)
        self._updateStatus(status)

    def removeTask(self, deleteFile=False):
        event_bus.deleteTaskSig.emit(self.task.task_id, deleteFile)

    def _open_folder(self):
        """在文件管理器中显示输出文件（对齐 Easy-FFmpeg）"""
        showInFolder(self.task.output_file)

    def _onOpenButtonClicked(self):
        self._open_folder()

    def _onCancelButtonClicked(self):
        event_bus.cancelTaskSig.emit(self.task.task_id)

    def _onLogButtonClicked(self):
        if self.task.logPath:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self.task.logPath))

    def _onRetryButtonClicked(self):
        event_bus.retryTaskSig.emit(self.task.task_id)
