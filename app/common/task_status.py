# coding:utf-8
"""任务状态枚举与本地化显示文本。

状态值用于程序内部逻辑判断，必须保持稳定（不随语言变化）。
显示文本通过 status_text() 统一翻译。
"""

from enum import Enum

from PySide6.QtCore import QObject


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

    def tr_waiting(self):
        return self.tr("等待中")

    def tr_processing(self):
        return self.tr("处理中")

    def tr_done(self):
        return self.tr("已完成")

    def tr_failed(self):
        return self.tr("失败")

    def tr_cancelling(self):
        return self.tr("正在取消...")

    def tr_cancelled(self):
        return self.tr("已取消")


_translator = _TaskStatusTranslator()


def status_text(status, processing_text=None):
    """返回任务状态的本地化显示文本。

    Args:
        status: TaskStatus 枚举成员。
        processing_text: PROCESSING 状态使用的已翻译显示文本
            （如"下载中"/"提取中"）。为空时使用通用"处理中"。
    """
    if status == TaskStatus.WAITING:
        return _translator.tr_waiting()
    if status == TaskStatus.PROCESSING:
        return processing_text or _translator.tr_processing()
    if status == TaskStatus.DONE:
        return _translator.tr_done()
    if status == TaskStatus.FAILED:
        return _translator.tr_failed()
    if status == TaskStatus.CANCELLING:
        return _translator.tr_cancelling()
    if status == TaskStatus.CANCELLED:
        return _translator.tr_cancelled()
    return str(status)
