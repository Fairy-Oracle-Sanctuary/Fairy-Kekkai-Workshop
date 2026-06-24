# coding:utf-8
"""任务状态枚举与本地化显示文本。

状态值用于程序内部逻辑判断，必须保持稳定（不随语言变化）。
显示文本通过 status_text() 统一翻译。
"""

from enum import Enum

from PySide6.QtCore import QObject

from .text import Text


class TaskStatus(Enum):
    """任务状态（内部稳定标识，不随语言变化）"""

    WAITING = "waiting"
    PROCESSING = "processing"
    DONE = "done"
    FAILED = "failed"
    CANCELLING = "cancelling"
    CANCELLED = "cancelled"


class _TaskStatusTranslator(QObject):
    """用于 lupdate 提取翻译字符串的辅助类"""

    def __init__(self):
        super().__init__()
        self.globalText = Text()

    def tr_waiting(self):
        return self.globalText.Waiting

    def tr_processing(self):
        return self.globalText.Processing

    def tr_done(self):
        return self.globalText.TextAuto005

    def tr_failed(self):
        return self.globalText.Failed3

    def tr_cancelling(self):
        return self.globalText.Cancelling

    def tr_cancelled(self):
        return self.globalText.Cancelled


def status_text(status, processing_text=None):
    """返回任务状态的本地化显示文本。

    Args:
        status: TaskStatus 枚举成员。
        processing_text: PROCESSING 状态使用的已翻译显示文本
            （如"下载中"/"提取中"）。为空时使用通用"处理中"。
    """
    translator = _TaskStatusTranslator()
    if status == TaskStatus.WAITING:
        return translator.tr_waiting()
    if status == TaskStatus.PROCESSING:
        return processing_text or translator.tr_processing()
    if status == TaskStatus.DONE:
        return translator.tr_done()
    if status == TaskStatus.FAILED:
        return translator.tr_failed()
    if status == TaskStatus.CANCELLING:
        return translator.tr_cancelling()
    if status == TaskStatus.CANCELLED:
        return translator.tr_cancelled()
    return str(status)
