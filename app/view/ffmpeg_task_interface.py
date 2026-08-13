from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.text import Text
from ..components.base_task_interface import TaskInterface
from ..components.task_card import FFmpegTaskCard
from ..service.ffmpeg_service import FFmpegTask, FFmpegWorker, adjust_output_format


class FFmpegTaskInterface(TaskInterface):
    """FFmpeg 压制任务界面（继承通用 TaskInterface，配置 FFmpeg 专属部分）"""

    def __init__(self, parent=None):
        super().__init__(
            max_concurrent_item=cfg.concurrentEncodes,
            object_name="ffmpegTaskInterface",
            parent=parent,
        )
        self.globalText = Text()

    # ---------- 子类扩展点 ----------

    def createTask(self, args):
        """由参数创建压制任务（修正输出扩展名；音频流探测在 Worker 线程内完成）"""
        video_path = args["video_path"]
        output_path = args["output_path"]
        return FFmpegTask(video_path, adjust_output_format(output_path))

    def getTaskPath(self, task):
        return task.videoPath

    def createTaskCard(self, task, parent):
        return FFmpegTaskCard(task, parent)

    def createWorker(self, task):
        return FFmpegWorker(task)

    def getTaskTypeText(self):
        return self.globalText.Encode

    def _emitLegacyFinished(self, success, card):
        """兼容旧托盘通知通道"""
        event_bus.ffmpeg_finished_signal.emit(
            success, card.task.output_path if success else ""
        )
