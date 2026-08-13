from ..common.event_bus import event_bus
from ..common.text import Text
from ..components.base_task_interface import TaskInterface
from ..components.task_card import OcrTaskCard
from ..service.ocr_service import OCRTask, OCRWorker


class OcrTaskInterface(TaskInterface):
    """OCR 字幕提取任务界面（继承通用 TaskInterface）

    并发默认串行（对齐旧行为 max_concurrent_tasks=1，避免 OCR 引擎冲突）。
    """

    def __init__(self, parent=None):
        super().__init__(object_name="ocrTaskInterface", parent=parent)
        self.globalText = Text()

    def createTask(self, args):
        return OCRTask(args)

    def getTaskPath(self, task):
        return task.videoPath

    def createTaskCard(self, task, parent):
        return OcrTaskCard(task, parent)

    def createWorker(self, task):
        return OCRWorker(task)

    def getTaskTypeText(self):
        return self.globalText.Extract

    def getTaskGeneratedFiles(self, task):
        """任务生成的文件：OCR 输出 + 激活为当前原文的 原文.srt"""
        files = super().getTaskGeneratedFiles(task)
        if task.outputFile:
            import os

            files.append(os.path.join(os.path.dirname(task.outputFile), "原文.srt"))
        return files

    def getLogName(self, task=None):
        """实时日志通道名（对齐 OCR Worker 的 videocr 通道）"""
        return "videocr"

    def _emitLegacyFinished(self, success, card):
        """兼容旧托盘通知通道"""
        event_bus.ocr_finished_signal.emit(
            success, card.task.outputFile if success else ""
        )
