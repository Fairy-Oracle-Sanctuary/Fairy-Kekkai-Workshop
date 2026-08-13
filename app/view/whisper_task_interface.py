from ..common.event_bus import event_bus
from ..common.text import Text
from ..components.base_task_interface import TaskInterface
from ..components.task_card import WhisperTaskCard
from ..service.whisper_service import WhisperTask, WhisperWorker


class WhisperTaskInterface(TaskInterface):
    """Whisper 语音识别任务界面（继承通用 TaskInterface，对齐 OCR/FFmpeg 链路）"""

    def __init__(self, parent=None):
        super().__init__(object_name="whisperTaskInterface", parent=parent)
        self.globalText = Text()

    # ---------- 子类扩展点 ----------

    def createTask(self, args):
        return WhisperTask(args)

    def getTaskPath(self, task):
        return task.input_file

    def createTaskCard(self, task, parent):
        return WhisperTaskCard(task, parent)

    def createWorker(self, task):
        return WhisperWorker(task)

    def getTaskTypeText(self):
        return self.globalText.Recognize

    def getTaskGeneratedFiles(self, task):
        """任务生成的文件：Whisper 输出 + 激活为当前原文的 原文.srt"""
        files = super().getTaskGeneratedFiles(task)
        if task.output_file:
            import os

            files.append(os.path.join(os.path.dirname(task.output_file), "原文.srt"))
        return files

    def getLogName(self, task=None):
        """实时日志通道名（对齐 Whisper Worker 的 whisper 通道）"""
        return "whisper"

    def _emitLegacyFinished(self, success, card):
        """兼容旧托盘通知通道"""
        event_bus.whisper_finished_signal.emit(
            success, card.task.output_file if success else ""
        )

    def addWhisperTask(self, args):
        self.addTask(args)
