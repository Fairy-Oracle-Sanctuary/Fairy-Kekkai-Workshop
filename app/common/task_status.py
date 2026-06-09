# coding:utf-8
"""任务状态枚举与本地化显示文本。

状态值用于程序内部逻辑判断，必须保持稳定（不随语言变化）。
显示文本通过 status_text() 统一翻译，使用 QCoreApplication.translate 以便 lupdate 提取。
"""

from enum import Enum

from PySide6.QtCore import QCoreApplication


class TaskStatus(Enum):
    """任务状态（内部稳定标识，不随语言变化）"""

    WAITING = "waiting"
    PROCESSING = "processing"
    DONE = "done"
    FAILED = "failed"
    CANCELLING = "cancelling"
    CANCELLED = "cancelled"


_CTX = "TaskStatus"


def status_text(status, processing_text=None):
    """返回任务状态的本地化显示文本。

    Args:
        status: TaskStatus 枚举成员。
        processing_text: PROCESSING 状态使用的已翻译显示文本
            （如"下载中"/"提取中"）。为空时使用通用"处理中"。
    """
    tr = QCoreApplication.translate

    if status == TaskStatus.WAITING:
        return tr(_CTX, "等待中")
    if status == TaskStatus.PROCESSING:
        return processing_text or tr(_CTX, "处理中")
    if status == TaskStatus.DONE:
        return tr(_CTX, "已完成")
    if status == TaskStatus.FAILED:
        return tr(_CTX, "失败")
    if status == TaskStatus.CANCELLING:
        return tr(_CTX, "正在取消...")
    if status == TaskStatus.CANCELLED:
        return tr(_CTX, "已取消")
    return str(status)
