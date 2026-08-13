from ..common.event_bus import event_bus
from ..common.text import Text
from ..components.base_task_interface import TaskInterface
from ..components.task_card import TranslateTaskCard
from ..service.translate_service import TranslateTask, TranslateWorker


class TranslateTaskInterface(TaskInterface):
    """翻译任务界面（继承通用 TaskInterface，对齐 OCR/FFmpeg/Whisper 链路）"""

    def __init__(self, parent=None):
        super().__init__(object_name="translateTaskInterface", parent=parent)
        self.globalText = Text()

    # ---------- 子类扩展点 ----------

    def createTask(self, args):
        return TranslateTask(args)

    def getTaskPath(self, task):
        return task.input_file

    def createTaskCard(self, task, parent):
        return TranslateTaskCard(task, parent)

    def createWorker(self, task):
        return TranslateWorker(task)

    def getTaskTypeText(self):
        return self.globalText.Translate

    def getTaskGeneratedFiles(self, task):
        """任务生成的文件：翻译输出（译文.srt）"""
        return [task.output_file] if task.output_file else []

    def getLogName(self, task=None):
        """实时日志通道名（对齐 TranslateWorker 的 translate 通道）"""
        return "translate"

    def _emitLegacyFinished(self, success, card):
        """兼容旧托盘通知通道"""
        event_bus.translate_finished_signal.emit(
            success, [card.task.output_file] if success else [""]
        )

    def addTranslateTask(self, args):
        self.addTask(args)
