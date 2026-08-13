import random

from PySide6.QtCore import QEvent, QSize, Qt, QThreadPool, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QGraphicsDropShadowEffect, QVBoxLayout, QWidget

from libs.qfluentwidgets_pro import (
    Action,
    CommandBarView,
    FluentIcon,
    ScrollArea,
    SegmentedWidget,
    isDarkTheme,
)

from ..common.event_bus import event_bus
from ..common.task_status import TaskStatus, status_text
from ..common.text import Text
from .base_task_card import DeleteTaskDialog
from .empty_status_widget import EmptyStatusWidget


class TaskInterface(ScrollArea):
    """通用任务界面基类（对齐 Easy-FFmpeg：QThreadPool 调度 + event_bus 收口 + CommandBar 批量操作）

    子类需实现：
    - createTask(args) -> task：由参数创建任务对象（含输出路径修正）
    - createTaskCard(task, parent) -> 任务卡片
    - createWorker(task) -> Worker（QRunnable，通过 event_bus 上报进度/完成）
    - getTaskPath(task) -> str：任务去重路径

    可选扩展点：
    - getTaskTypeText() / getOutputName(task) / getFileName(task)：通知文案
    - _emitLegacyFinished(success, card)：向旧功能信号转发完成通知（如托盘）
    """

    returnTask = Signal(bool, list, bool)  # 是否重复 任务路径列表 是否发送消息

    _idle_statuses = (
        TaskStatus.Waiting,
        TaskStatus.Succeeded,
        TaskStatus.Failed,
        TaskStatus.Cancelled,
    )

    def __init__(
        self,
        max_concurrent_item=None,
        object_name="TaskInterface",
        parent=None,
    ):
        super().__init__(parent)
        self.view = QWidget(self)
        self.vBoxLayout = QVBoxLayout(self.view)
        self.globalText = Text()
        self.object_name = object_name

        self.segmentedWidget = SegmentedWidget(self)
        self.allTab = QWidget()
        self.processingTab = QWidget()
        self.completedTab = QWidget()
        self.failedTab = QWidget()
        self.taskListContainer = QWidget(self)
        self.taskListLayout = QVBoxLayout(self.taskListContainer)

        self.taskPool = QThreadPool()
        self.max_concurrent_item = max_concurrent_item
        # 默认串行（对齐旧 BaseTaskInterface 的 max_concurrent_tasks=1）；
        # 传入 cfg 配置项则按用户设置
        self.taskPool.setMaxThreadCount(
            max_concurrent_item.value if max_concurrent_item is not None else 1
        )
        self.cards = []
        self.cardMap = {}
        self.threadMap = {}
        self.selectionCount = 0
        self.isSelectionMode = False
        self.task_paths = []  # 任务去重路径列表（returnTask 用）
        self._input_paths = set()

        self.commandView = TaskCommandBarView(self)
        self.commandView.hide()

        self.emptyStatusIcons = [
            ":/app/images/logo/Face01.svg",
            ":/app/images/logo/Face02.svg",
            ":/app/images/logo/Face03.svg",
            ":/app/images/logo/Face04.svg",
            ":/app/images/logo/Face05.svg",
            ":/app/images/logo/Face06.svg",
            ":/app/images/logo/Face07.svg",
            ":/app/images/logo/Face08.svg",
            ":/app/images/logo/Face09.svg",
            ":/app/images/logo/Face10.svg",
        ]
        self.lastSelectedemptyStatusIcon = random.randint(
            0, len(self.emptyStatusIcons) - 1
        )
        self.emptyStatusWidget = EmptyStatusWidget(
            self.emptyStatusIcons[self.lastSelectedemptyStatusIcon],
            self.globalText.NoTasks,
            self,
        )

        self._initWidget()
        self._connectSignalToSlot()

    def createTask(self, args):
        """由参数创建任务对象（子类必须实现）"""
        raise NotImplementedError("子类必须实现 createTask 方法")

    def createTaskCard(self, task, parent):
        """创建任务卡片（子类必须实现）"""
        raise NotImplementedError("子类必须实现 createTaskCard 方法")

    def createWorker(self, task):
        """创建任务 Worker（子类必须实现，返回 QRunnable）"""
        raise NotImplementedError("子类必须实现 createWorker 方法")

    def getTaskPath(self, task) -> str:
        """获取任务去重路径（子类必须实现）"""
        raise NotImplementedError("子类必须实现 getTaskPath 方法")

    def getTaskTypeText(self) -> str:
        """任务类型文案（用于通知消息）"""
        return self.globalText.Task2

    def getOutputName(self, task) -> str:
        """输出文件名（用于成功通知）"""
        return getattr(task, "outputName", "")

    def getFileName(self, task) -> str:
        """输入文件名（用于失败通知）"""
        return getattr(task, "fileName", "")

    def getTaskGeneratedFiles(self, task) -> list:
        """任务生成的文件列表（勾选删除文件时一并删除，子类可覆盖扩展）"""
        output = getattr(task, "outputFile", None) or getattr(task, "output_path", None)
        return [output] if output else []

    def getLogName(self, task=None) -> str:
        """实时日志通道名（taskLogSignal 用；返回空则完成任务时不写日志框）"""
        return ""

    def _initWidget(self):
        self.setWidget(self.view)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setObjectName(self.object_name)
        self.enableTransparentBackground()

        self.segmentedWidget.addItem(
            self.allTab, self.globalText.All, lambda: self.filterTasks("all")
        )
        self.segmentedWidget.addItem(
            self.processingTab,
            status_text(TaskStatus.Processing),
            lambda: self.filterTasks(TaskStatus.Processing),
        )
        self.segmentedWidget.addItem(
            self.completedTab,
            status_text(TaskStatus.Succeeded),
            lambda: self.filterTasks(TaskStatus.Succeeded),
        )
        self.segmentedWidget.addItem(
            self.failedTab,
            status_text(TaskStatus.Failed),
            lambda: self.filterTasks(TaskStatus.Failed),
        )
        self.segmentedWidget.setCurrentItem(self.allTab)
        self.segmentedWidget.setMaximumHeight(30)

        self.taskListLayout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.vBoxLayout.addWidget(self.segmentedWidget)
        self.vBoxLayout.addWidget(self.taskListContainer)

        self.emptyStatusWidget.setMinimumWidth(200)
        self._updateEmptyStatus(False)

    def _connectSignalToSlot(self):
        event_bus.updateTaskStatusSig.connect(self._update_task_status)
        event_bus.finishTaskSig.connect(self._handle_task_finished)
        event_bus.deleteTaskSig.connect(self._handle_task_deleted)
        event_bus.cancelTaskSig.connect(self._handle_cancel_task)
        event_bus.retryTaskSig.connect(self._handle_retry_task)
        self.emptyStatusWidget.clicked.connect(self._updateEmptyStatus)
        self.commandView.redownloadAction.triggered.connect(self._restartSelectedTasks)
        self.commandView.deleteAction.triggered.connect(self._removeSelectedTasks)
        self.commandView.selectAllAction.triggered.connect(self.selectAll)
        self.commandView.cancelAction.triggered.connect(
            lambda: self.setSelectionMode(False)
        )
        if self.max_concurrent_item is not None:
            self.max_concurrent_item.valueChanged.connect(
                self._updateMaxConcurrentTasks
            )

    def _updateMaxConcurrentTasks(self, value):
        self.taskPool.setMaxThreadCount(value)

    def addTask(self, args):
        """添加任务"""
        task = self.createTask(args)
        if task is None:
            return
        task_path = self.getTaskPath(task)

        if task_path in self._input_paths:
            self.returnTask.emit(True, self.task_paths, True)
            return
        self._input_paths.add(task_path)
        self.task_paths.append(task_path)
        self.returnTask.emit(False, self.task_paths, True)

        card = self.createTaskCard(task, self.taskListContainer)
        card.checkedChanged.connect(self._onCardCheckedChanged)
        if self.isSelectionMode:
            card.setSelectionMode(True)
        self.taskListLayout.insertWidget(0, card, 0, Qt.AlignmentFlag.AlignTop)
        self.cards.insert(0, card)
        self.cardMap[task.task_id] = card
        self.threadMap[task.task_id] = self.createWorker(task)
        self.taskPool.start(self.threadMap[task.task_id])

        event_bus.taskCountChanged.emit(len(self.cards))
        self.filterTasks("all")

    def _removeCard(self, card, deleteFile=False):
        """从布局和列表中移除卡片"""
        task_id = card.task.task_id
        task_path = self.getTaskPath(card.task)
        # 断开信号，防止 deleteLater 时触发 checkedChanged
        try:
            card.checkedChanged.disconnect(self._onCardCheckedChanged)
        except (TypeError, RuntimeError):
            pass
        self.taskListLayout.removeWidget(card)
        self.cards.remove(card)
        self.cardMap.pop(task_id, None)
        self.threadMap.pop(task_id, None)
        self._input_paths.discard(task_path)
        if task_path in self.task_paths:
            self.task_paths.remove(task_path)
        # 按需删除任务输出及附加文件（子类可覆盖 getTaskGeneratedFiles 扩展）
        if deleteFile:
            for output_file in self.getTaskGeneratedFiles(card.task):
                if not output_file:
                    continue
                import os

                try:
                    if os.path.exists(output_file):
                        os.remove(output_file)
                except OSError:
                    pass
        # 删除日志文件（如存在）
        if getattr(card.task, "logPath", None):
            import os

            try:
                if os.path.exists(card.task.logPath):
                    os.remove(card.task.logPath)
            except OSError:
                pass
        # 被删卡片如果是选中状态，手动更新计数
        if card.isChecked():
            self.selectionCount = max(0, self.selectionCount - 1)
            if self.selectionCount == 0:
                self.setSelectionMode(False)
        card.hide()
        card.deleteLater()
        self._updateEmptyStatus(not self.cards)
        event_bus.taskCountChanged.emit(len(self.cards))

    def filterTasks(self, status):
        """根据任务状态过滤任务"""
        hasCard = False
        for card in self.cards.copy():
            if card.status == status or status == "all":
                card.setVisible(True)
                hasCard = True
            else:
                card.setVisible(False)
        self._updateEmptyStatus(not hasCard)

    def _updateEmptyStatus(self, show: bool = True):
        """更新空状态显示（无任务时显示表情并轮换图标）"""
        self.emptyStatusWidget.setVisible(show)
        if show:
            self.lastSelectedemptyStatusIcon = (
                self.lastSelectedemptyStatusIcon + 1
            ) % len(self.emptyStatusIcons)
            icon = self.emptyStatusIcons[self.lastSelectedemptyStatusIcon]
            self.emptyStatusWidget.setIcon(icon)

    def _onCardCheckedChanged(self, checked: bool):
        if checked:
            self.selectionCount += 1
            self.setSelectionMode(True)
        else:
            self.selectionCount = max(0, self.selectionCount - 1)
            if self.selectionCount == 0:
                self.setSelectionMode(False)

    def setSelectionMode(self, enter: bool):
        """进入/退出选择模式：控制卡片 checkbox 与 CommandBar 显隐"""
        if self.isSelectionMode == enter:
            return

        self.isSelectionMode = enter

        for card in self.cards:
            card.setSelectionMode(enter)

        if enter:
            self.commandView.setVisible(True)
            self.commandView.raise_()
        else:
            self.commandView.setVisible(False)
            self.selectionCount = 0

    def selectAll(self):
        for card in self.cards.copy():
            card.setChecked(True)

    def _is_idle_card(self, card):
        """是否可删除/可重试（非进行中的任务）"""
        return card.status in self._idle_statuses

    def _removeSelectedTasks(self):
        w = DeleteTaskDialog(self.window(), deleteOnClose=False)
        w.deleteFileCheckBox.setChecked(False)

        if w.exec():
            deleteFile = w.deleteFileCheckBox.isChecked()
            for card in self.cards.copy():
                if card.isChecked() and self._is_idle_card(card):
                    self._removeCard(card, deleteFile)

        w.deleteLater()
        self.setSelectionMode(False)

    def _restartSelectedTasks(self):
        for card in self.cards.copy():
            if card.isChecked() and self._is_idle_card(card):
                event_bus.retryTaskSig.emit(card.task.task_id)

    # ---------- 任务状态更新 ----------

    def _update_task_status(
        self, task_id, progress, status, size, time, bitrate, speed
    ):
        """更新任务状态（Worker 数值上报 → 卡片）"""
        card = self.cardMap.get(task_id)
        if card:
            card.updateTask(progress, status, size, time, bitrate, speed)

    def _handle_task_finished(self, task_id, success: bool, logPath: str):
        """任务完成"""
        card = self.cardMap.get(task_id)
        if not card or card.status in (TaskStatus.Cancelled, TaskStatus.Cancelling):
            return
        card.updateTask(
            status=TaskStatus.Succeeded if success else TaskStatus.Failed,
            progress=100 if success else 0,
        )
        # 任务完成/失败写入日志框（子类通过 getLogName 开启通道）
        log_name = self.getLogName()
        if log_name:
            event_bus.taskLogSignal.emit(
                log_name,
                self.globalText.TaskDone if success else self.globalText.TaskFailedLog,
                not success,
                False,
            )
        if success:
            event_bus.notification_service.show_success(
                self.globalText.Success,
                self.globalText.Completed.format(
                    self.getOutputName(card.task), self.getTaskTypeText()
                ),
            )
        else:
            event_bus.notification_service.show_error(
                self.globalText.Failed,
                self.globalText.TaskFailed.format(
                    self.getFileName(card.task), self.getTaskTypeText(), ""
                ),
            )
        # 兼容旧功能通知通道（托盘等），子类可覆盖
        self._emitLegacyFinished(success, card)

    def _emitLegacyFinished(self, success, card):
        """向旧功能信号转发完成通知（子类按需覆盖）"""

    def _handle_cancel_task(self, task_id):
        """取消任务"""
        card = self.cardMap.get(task_id)
        if not card or card.status in (TaskStatus.Cancelled, TaskStatus.Cancelling):
            return
        card.updateTask(status=TaskStatus.Cancelling)
        thread = self.threadMap.pop(task_id, None)
        if thread and hasattr(thread, "cancel"):
            # worker.cancel()：标记 + kill，且不 emit 完成信号
            thread.cancel()
        card.updateTask(status=TaskStatus.Cancelled)

    def _handle_retry_task(self, task_id):
        """重试任务（按当前设置重新构建 Worker）"""
        card = self.cardMap.get(task_id)
        if not card:
            return
        task = card.task
        # Worker 在内部重建命令与探测（线程内完成，不阻塞主线程）
        card.updateTask(status=TaskStatus.Waiting, progress=0)
        thread = self.createWorker(task)
        self.threadMap[task_id] = thread
        self.taskPool.start(thread)

    def _handle_task_deleted(self, task_id, deleteFile):
        """移除任务卡片"""
        for card in self.cards.copy():
            if hasattr(card, "task") and card.task.task_id == task_id:
                self._removeCard(card, deleteFile)
                break

    # ---------- 停止与布局 ----------

    def stopAll(self):
        """停止所有运行中的任务（关闭应用时调用）"""
        for thread in self.threadMap.values():
            if hasattr(thread, "cancel"):
                thread.cancel()
        self.threadMap.clear()
        self.taskPool.clear()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        # 定位基准 = commandView 的实际父级（interface 自身），保证坐标自洽
        parent = self.commandView.parentWidget()
        x = parent.width() // 2 - self.commandView.width() // 2
        y = parent.height() - self.commandView.sizeHint().height() - 20
        self.commandView.move(x, y)

        self.emptyStatusWidget.adjustSize()
        w, h = self.emptyStatusWidget.width(), self.emptyStatusWidget.height()
        self.emptyStatusWidget.move(
            int(self.width() / 2 - w / 2), int(self.height() / 2 - h / 2)
        )

    def showEvent(self, event: QEvent):
        """切换到本页面时刷新过滤"""
        super().showEvent(event)
        self.filterTasks("all")


class TaskCommandBarView(CommandBarView):
    """任务批量操作命令栏（对齐 Easy-FFmpeg）"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.redownloadAction = Action(
            FluentIcon.UPDATE, self.globalText.RetryAction, self
        )
        self.deleteAction = Action(
            FluentIcon.DELETE, self.globalText.DeleteAction, self
        )
        self.selectAllAction = Action(
            FluentIcon.SELECT, self.globalText.SelectAll, self
        )
        self.cancelAction = Action(
            FluentIcon.CLEAR_SELECTION, self.globalText.FWCancelSelect, self
        )

        self.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextUnderIcon)
        self.setIconSize(QSize(18, 18))
        self.addActions([self.redownloadAction, self.deleteAction])
        self.addSeparator()
        self.addActions([self.selectAllAction, self.cancelAction])
        self.resizeToSuitableWidth()
        self.setShadowEffect()

    def setShadowEffect(self, blurRadius=35, offset=(0, 8)):
        """add shadow to dialog"""
        color = QColor(0, 0, 0, 80 if isDarkTheme() else 30)
        self.shadowEffect = QGraphicsDropShadowEffect(self)
        self.shadowEffect.setBlurRadius(blurRadius)
        self.shadowEffect.setOffset(*offset)
        self.shadowEffect.setColor(color)
        self.setGraphicsEffect(None)
        self.setGraphicsEffect(self.shadowEffect)


# ==================== 旧版基类（Legacy） ====================
# 以下 BaseTaskInterface 为旧 QThread 信号直连模式的基类，
# 供尚未迁移的任务界面（OCR/翻译/Whisper/上传）使用；
# 待其 service 迁移到 QRunnable + event_bus 后一并删除。


class BaseTaskInterface(ScrollArea):
    """基础任务界面（旧版，待迁移）"""

    returnTask = Signal(bool, list, bool)  # 是否重复的任务 任务路径列表 是否发送消息

    def __init__(
        self,
        object_name="BaseTaskInterface",
        processing_text=None,
        task_type=None,
        max_concurrent_tasks=1,
        parent=None,
    ):
        super().__init__(parent)
        self.view = QWidget(self)
        self.vBoxLayout = QVBoxLayout(self.view)

        self.globalText = Text()

        # 配置项 - 子类可以在初始化后修改这些属性
        self.object_name = object_name
        self.processing_text = processing_text or self.globalText.Processing
        self.task_type = task_type or self.globalText.Task2  # 用于消息显示

        self.tasks = []  # 所有任务
        self.task_paths = []  # 所有任务文件路径
        self.active_threads = []  # 活跃的线程
        self.max_concurrent_tasks = max_concurrent_tasks

        self._initWidget()

    def _initWidget(self):
        self.setWidget(self.view)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setObjectName(self.object_name)
        self.enableTransparentBackground()

        # 创建分段控件
        self.segmentedWidget = SegmentedWidget(self)
        self.allTab = QWidget()
        self.processingTab = QWidget()
        self.completedTab = QWidget()
        self.failedTab = QWidget()

        self.segmentedWidget.addItem(
            self.allTab, self.globalText.All, lambda: self.filterTasks("all")
        )
        self.segmentedWidget.addItem(
            self.processingTab,
            status_text(TaskStatus.Processing, self.tr(self.processing_text)),
            lambda: self.filterTasks(TaskStatus.Processing),
        )
        self.segmentedWidget.addItem(
            self.completedTab,
            status_text(TaskStatus.Succeeded),
            lambda: self.filterTasks(TaskStatus.Succeeded),
        )
        self.segmentedWidget.addItem(
            self.failedTab,
            status_text(TaskStatus.Failed),
            lambda: self.filterTasks(TaskStatus.Failed),
        )

        self.segmentedWidget.setCurrentItem(self.allTab)
        self.segmentedWidget.setMaximumHeight(30)

        # 创建任务列表容器
        self.taskListContainer = QWidget(self)
        self.taskListLayout = QVBoxLayout(self.taskListContainer)
        self.taskListLayout.setAlignment(Qt.AlignTop)

        # 设置布局
        self.vBoxLayout.addWidget(self.segmentedWidget)
        self.vBoxLayout.addWidget(self.taskListContainer)

    def createTask(self, args):
        """创建任务对象 - 子类应该重写此方法"""
        raise NotImplementedError("子类必须实现 createTask 方法")

    def createTaskItem(self, task, parent):
        """创建任务项组件 - 子类应该重写此方法"""
        raise NotImplementedError("子类必须实现 createTaskItem 方法")

    def createTaskThread(self, task):
        """创建任务线程 - 子类应该重写此方法"""
        raise NotImplementedError("子类必须实现 createTaskThread 方法")

    def getTaskPath(self, task):
        """获取任务的路径 - 子类应该重写此方法"""
        raise NotImplementedError("子类必须实现 getTaskPath 方法")

    def getSuccessMessage(self, task_path):
        """获取成功消息"""
        return self.globalText.Completed.format(task_path, self.task_type)

    def getFailureMessage(self, task_path, message):
        """获取失败消息"""
        return self.globalText.TaskFailed.format(task_path, self.task_type, message)

    def _updateMaxConcurrentTasks(self, value):
        """更新最大并发任务数"""
        self.max_concurrent_tasks = value
        active_count = len([t for t in self.tasks if t.status == TaskStatus.Processing])

        if active_count > self.max_concurrent_tasks:
            excess_count = active_count - self.max_concurrent_tasks
            stopped = 0
            for task in reversed(self.tasks):
                if task.status == TaskStatus.Processing and stopped < excess_count:
                    for thread in self.active_threads:
                        if thread.task.id == task.id:
                            thread.cancel()
                            task.status = TaskStatus.Waiting
                            self.updateTaskUI(task.id)
                            stopped += 1
                            break

    def addTask(self, args):
        """添加任务"""
        task = self.createTask(args)
        if task is None:
            return
        task_path = self.getTaskPath(task)

        if task_path in self.task_paths:
            self.returnTask.emit(True, self.task_paths, True)
            return
        else:
            self.task_paths.append(task_path)
            self.returnTask.emit(False, self.task_paths, True)

        # 添加任务
        self.tasks.append(task)

        # 创建任务项
        task_item = self.createTaskItem(task, self.taskListContainer)
        self.taskListLayout.insertWidget(0, task_item)

        # 连接信号
        if hasattr(task_item, "removeTaskSignal"):
            task_item.removeTaskSignal.connect(self.removeTask)
        if hasattr(task_item, "retryTaskSignal"):
            task_item.retryTaskSignal.connect(self.retryTask)

        # 开始任务（如果没有超过最大并发数）
        self.startNextTask()

        # 更新过滤视图
        self.filterTasks(self._currentFilter())

    def startNextTask(self):
        """开始下一个任务"""
        active_count = len([t for t in self.tasks if t.status == TaskStatus.Processing])

        if active_count >= self.max_concurrent_tasks:
            return

        # 查找等待中的任务
        waiting_tasks = [t for t in self.tasks if t.status == TaskStatus.Waiting]

        if waiting_tasks:
            task = waiting_tasks[0]
            self.startTask(task)

    def startTask(self, task):
        """开始任务"""
        task_thread = self.createTaskThread(task)
        task_thread.finished_signal.connect(
            lambda success, message: self.onTaskFinished(task.id, success, message)
        )

        # 如果有进度信号，连接它
        if hasattr(task_thread, "progress_signal"):
            task_thread.progress_signal.connect(
                lambda progress, speed, filename: self.onTaskProgress(
                    task.id, progress, speed, filename
                )
            )

        # 如果有打印输出信号，连接它
        if hasattr(task_thread, "print_signal"):
            task_thread.print_signal.connect(
                lambda message: self.onPrintOutput(task.id, message)
            )

        # 如果有打印日志信号，连接它
        if hasattr(task_thread, "log_signal"):
            task_thread.log_signal.connect(
                lambda message, is_error, is_flush: self.onPrintLog(
                    task.id, message, is_error, is_flush
                )
            )

        # 存储线程引用到对应的任务项
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if (
                hasattr(widget, "task")
                and hasattr(widget.task, "id")
                and widget.task.id == task.id
            ):
                widget.task_thread = task_thread
                break

        # 存储线程引用
        self.active_threads.append(task_thread)

        # 更新任务状态
        task.status = TaskStatus.Processing

        # 更新UI
        self.updateTaskUI(task.id)

        # 开始任务
        task_thread.start()

    def onPrintOutput(self, task_id, message):
        """处理打印输出 - 子类可以重写此方法来实现特定的输出处理"""

    def onTaskProgress(self, task_id, progress, speed=None, filename=None):
        """任务进度更新"""
        for task in self.tasks:
            if task.id == task_id:
                if hasattr(task, "progress"):
                    task.progress = progress
                self.updateTaskUI(task_id)
                break

    def onTaskFinished(self, task_id, success, message):
        """任务完成"""
        from ..common.event_bus import event_bus

        for task in self.tasks:
            if task.id == task_id:
                task_path = self.getTaskPath(task)
                if success:
                    task.status = TaskStatus.Succeeded
                    event_bus.notification_service.show_success(
                        self.globalText.TextAuto006.format(self.task_type),
                        self.getSuccessMessage(task_path),
                    )
                else:
                    task.status = TaskStatus.Failed
                    if self.globalText.Cancel not in message:
                        event_bus.notification_service.show_error(
                            self.globalText.Failed2.format(self.task_type),
                            message.strip(),
                        )

                # 移除活跃线程
                for thread in self.active_threads[:]:
                    if thread.task.id == task_id:
                        self.active_threads.remove(thread)
                        break

                self.updateTaskUI(task_id)

                # 开始下一个任务
                self.startNextTask()
                break

    def updateTaskUI(self, task_id):
        """更新任务UI"""
        # 查找对应的任务项
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if (
                hasattr(widget, "task")
                and hasattr(widget.task, "id")
                and widget.task.id == task_id
            ):
                if hasattr(widget, "updateProgress") and hasattr(
                    widget.task, "progress"
                ):
                    progress = (
                        0
                        if widget.task.status == TaskStatus.Cancelled
                        else widget.task.progress
                    )
                    widget.updateProgress(progress, self.getTaskPath(widget.task))

                if hasattr(widget, "updateStatus"):
                    widget.updateStatus(widget.task.status)
                break

        # 更新过滤视图
        self.filterTasks(self._currentFilter())

    def _currentFilter(self):
        """根据当前选中的标签页返回对应的过滤条件"""
        current = self.segmentedWidget.currentItem()
        if current == self.allTab:
            return "all"
        elif current == self.processingTab:
            return TaskStatus.Processing
        elif current == self.completedTab:
            return TaskStatus.Succeeded
        elif current == self.failedTab:
            return TaskStatus.Failed
        return "all"

    def filterTasks(self, filter_type):
        """过滤任务显示"""
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if hasattr(widget, "task") and hasattr(widget.task, "status"):
                if filter_type == "all" or widget.task.status == filter_type:
                    widget.setVisible(True)
                else:
                    widget.setVisible(False)

    def retryTask(self, task_id):
        """重新执行任务"""
        for task in self.tasks:
            if task.id == task_id:
                task.status = TaskStatus.Waiting
                if hasattr(task, "progress"):
                    task.progress = 0
                if hasattr(task, "error_message"):
                    task.error_message = ""

                self.updateTaskUI(task_id)
                self.startNextTask()
                break

    def removeTask(self, task_id):
        """移除任务"""
        for num, task in enumerate(self.tasks[:]):
            if task.id == task_id:
                self.tasks.remove(task)
                if num < len(self.task_paths):
                    self.task_paths.pop(num)
                self.returnTask.emit(False, self.task_paths, False)
                break

        # 从UI中移除
        for i in range(self.taskListLayout.count()):
            widget = self.taskListLayout.itemAt(i).widget()
            if (
                hasattr(widget, "task")
                and hasattr(widget.task, "id")
                and widget.task.id == task_id
            ):
                self.taskListLayout.removeWidget(widget)
                widget.deleteLater()
                break
