"""任务状态枚举与本地化显示文本。

状态值用于程序内部逻辑判断，必须保持稳定（不随语言变化）。
显示文本通过 status_text() 统一翻译。

枚举成员与 Easy-FFmpeg 对齐（int 值 0-6，首字母大写），
便于任务卡片按 status.value 索引状态文本数组。
"""

from enum import Enum

from PySide6.QtCore import QObject

from .text import Text


class TaskStatus(Enum):
    Waiting = 0
    Pending = 1
    Processing = 2
    Failed = 3
    Succeeded = 4
    Cancelling = 5
    Cancelled = 6


class _TaskStatusTranslator(QObject):
    """用于 lupdate 提取翻译字符串的辅助类"""

    def __init__(self):
        super().__init__()
        self.globalText = Text()

    def tr_waiting(self):
        return self.globalText.Waiting

    def tr_pending(self):
        # Fairy 流程未使用 Pending，暂用"等待"文案兜底
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
        processing_text: Processing 状态使用的已翻译显示文本
            （如"下载中"/"提取中"）。为空时使用通用"处理中"。
    """
    translator = _TaskStatusTranslator()
    if status == TaskStatus.Waiting:
        return translator.tr_waiting()
    if status == TaskStatus.Pending:
        return translator.tr_pending()
    if status == TaskStatus.Processing:
        return processing_text or translator.tr_processing()
    if status == TaskStatus.Succeeded:
        return translator.tr_done()
    if status == TaskStatus.Failed:
        return translator.tr_failed()
    if status == TaskStatus.Cancelling:
        return translator.tr_cancelling()
    if status == TaskStatus.Cancelled:
        return translator.tr_cancelled()
    return str(status)
