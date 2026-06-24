from PySide6.QtCore import Qt
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QVBoxLayout, QWidget
from qfluentwidgets import FluentIcon as FIF
from qfluentwidgets import FluentWindow, PlainTextEdit, ScrollArea

from ..common.event_bus import event_bus
from ..common.text import Text


class LogWindow(FluentWindow):
    """Log window"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self._initWindow()

        self._initNavigation()

    def _initWindow(self):
        """初始化窗口"""
        self.setWindowTitle(self.globalText.LOG)
        self.setWindowIcon(QIcon(":/app/images/logo.png"))
        self.resize(700, 400)
        self.setMinimumSize(400, 250)

    def _initNavigation(self):
        """创建页面"""
        self.allLogInterface = LogInterface("allLog", self)
        self.projectLogInterface = LogInterface("projectLog", self)
        self.downloadLogInterface = LogInterface("downloadLog", self)
        self.videocrLogInterface = LogInterface("videocrLog", self)
        self.whisperLogInterface = LogInterface("whisperLog", self)
        self.aiLogInterface = LogInterface("aiLog", self)
        self.ffmpegLogInterface = LogInterface("ffmpegLog", self)

        self.addSubInterface(self.allLogInterface, FIF.HOME, self.globalText.AllLogs)
        self.addSubInterface(self.projectLogInterface, FIF.FOLDER, self.globalText.ProjectLogs)
        self.addSubInterface(
            self.downloadLogInterface, FIF.DOWNLOAD, self.globalText.DownloadLogs
        )
        self.addSubInterface(
            self.videocrLogInterface, FIF.VIDEO, self.globalText.SEL
        )
        self.addSubInterface(
            self.whisperLogInterface, FIF.MICROPHONE, self.globalText.SRL
        )
        self.addSubInterface(self.aiLogInterface, FIF.MESSAGE, self.globalText.AITranslationLogs)
        self.addSubInterface(
            self.ffmpegLogInterface, FIF.ZIP_FOLDER, self.globalText.FFmpegEncodingLogs
        )

    def setLog(
        self,
        allLogs,
        projectLogs,
        downloadLogs,
        videocrLogs,
        whisperLogs,
        aiLogs,
        ffmpegLogs,
    ):
        self.allLogInterface.setLog(allLogs)
        self.projectLogInterface.setLog(projectLogs)
        self.downloadLogInterface.setLog(downloadLogs)
        self.videocrLogInterface.setLog(videocrLogs)
        self.whisperLogInterface.setLog(whisperLogs)
        self.aiLogInterface.setLog(aiLogs)
        self.ffmpegLogInterface.setLog(ffmpegLogs)

    def closeEvent(self, event):
        """重写关闭事件"""
        event_bus.log_window_closed.emit()
        super().closeEvent(event)


class LogInterface(ScrollArea):
    """基础日志接口界面"""

    def __init__(self, text, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.view = QWidget(self)
        self.text = text

        self.vBoxLayout = QVBoxLayout(self.view)

        self._initWidget()
        # self._connectSignalToSlot()

    def _initWidget(self):
        self.setWidget(self.view)
        self.setAcceptDrops(True)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        self.setObjectName(self.text)
        self.enableTransparentBackground()

        self.logTextEdit = PlainTextEdit(self.view)
        self.logTextEdit.setReadOnly(True)
        self.logTextEdit.setPlaceholderText(self.globalText.LWBDH)

        self.vBoxLayout.addWidget(self.logTextEdit)

    def setLog(self, log):
        self.logTextEdit.setPlainText(log)
