import os
import re
import time

from PySide6.QtCore import QDateTime, QEventLoop, QProcess, QRunnable

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.task_status import TaskStatus

# 解析 ffmpeg 输出（对齐 Easy-FFmpeg）
DURATION_RE = re.compile(r"Duration: (\d{2}):(\d{2}):(\d{2}\.\d+)")
TIME_RE = re.compile(r"time=(\d{2}):(\d{2}):(\d{2}\.\d+)")
SIZE_RE = re.compile(r"size=\s*(\S+)")
BITRATE_RE = re.compile(r"bitrate=\s*(\S+)")
SPEED_RE = re.compile(r"speed=\s*([\d.]+)")


class FFmpegTask:
    """FFmpeg 压制任务纯数据类（字段对齐 Easy-FFmpeg）"""

    _id_counter = 0

    def __init__(self, video_path: str, output_path: str):
        FFmpegTask._id_counter += 1
        self.task_id = FFmpegTask._id_counter
        self.videoPath = video_path
        self.output_path = output_path
        self.saveFolder = os.path.dirname(output_path)
        self.outputName = os.path.basename(output_path)
        self.fileName = os.path.basename(video_path)
        self.logPath = None
        self.createTime = QDateTime.currentDateTime()


def probe_has_audio(video_path: str) -> bool:
    """检测输入文件是否有音频流"""
    try:
        process = QProcess()
        process.start(cfg.get(cfg.ffmpegPath), ["-i", video_path])
        process.waitForFinished(5000)
        stderr_output = (
            process.readAllStandardError().data().decode("utf-8", errors="ignore")
        )
        return "Audio:" in stderr_output
    except Exception:
        return False


def adjust_output_format(output_file: str) -> str:
    """根据配置修正输出文件扩展名（纯函数，无 I/O）"""
    output_format = cfg.ffmpegOutputFormat.value
    if output_format:
        base_name = os.path.splitext(output_file)[0]
        return f"{base_name}.{output_format}"
    return output_file


def build_ffmpeg_command(video_path: str, output_file: str, has_audio: bool):
    """根据配置构建 FFmpeg 命令（纯函数，不做任何探测）

    has_audio 必须由调用方显式传入（在 Worker 线程内探测），
    避免在主线程同步启动 ffmpeg 导致界面卡顿。

    Returns:
        (cmd, output_file)：cmd 为 ffmpeg 参数列表（不含可执行文件路径），
        output_file 可能因输出格式设置被修正扩展名。
    """
    cmd = []

    # 硬件加速
    if cfg.ffmpegUseHardwareAcceleration.value:
        accelerator = cfg.ffmpegHardwareAccelerator.value
        if accelerator != "auto":
            cmd.extend(["-hwaccel", accelerator])

    # 输入视频
    cmd.extend(["-i", video_path])

    # 视频编码参数
    cmd.extend(
        [
            "-c:v",
            cfg.ffmpegVideoCodec.value,
            "-crf",
            str(cfg.ffmpegCrf.value),
            "-preset",
            cfg.ffmpegPreset.value,
        ]
    )

    # x264高级参数（如果启用）
    if cfg.ffmpegUseAdvanced.value:
        x264_params = [
            f"ref={cfg.ffmpegRefFrames.value}",
            f"bframes={cfg.ffmpegBFrames.value}",
            f"keyint={cfg.ffmpegKeyint.value}",
            f"minkeyint={cfg.ffmpegMinkeyint.value}",
            f"scenecut={cfg.ffmpegScenecut.value}",
            f"qcomp={cfg.ffmpegQcomp.value}",
            f"psy-rd={cfg.ffmpegPsyRd.value}",
            f"aq-mode={cfg.ffmpegAqMode.value}",
            f"aq-strength={cfg.ffmpegAqStrength.value}",
        ]
        cmd.extend(["-x264-params", ":".join(x264_params)])

    # 音频处理
    audio_mode = cfg.ffmpegAudioMode.value
    if audio_mode == "none":
        cmd.extend(["-an"])  # 无音频
    elif audio_mode == "copy":
        cmd.extend(["-c:a", "copy"])  # 直接复制
    elif audio_mode in ("encode", "auto"):
        # 依据调用方传入的音频流探测结果决定是否编码音频（探测在 Worker 线程内完成）
        if has_audio:
            cmd.extend(
                [
                    "-c:a",
                    cfg.ffmpegAudioCodec.value,
                    "-b:a",
                    cfg.ffmpegAudioBitrate.value,
                ]
            )
        else:
            cmd.extend(["-an"])

    # 视频缩放
    scale_option = cfg.ffmpegScale.value
    if scale_option != "none":
        if scale_option == "custom":
            if cfg.ffmpegCustomScale.value:
                cmd.extend(["-vf", f"scale={cfg.ffmpegCustomScale.value}"])
        else:
            resolution_map = {
                "720p": "1280:720",
                "1080p": "1920:1080",
                "1440p": "2560:1440",
                "2160p": "3840:2160",
            }
            if scale_option in resolution_map:
                cmd.extend(["-vf", f"scale={resolution_map[scale_option]}"])

    # 帧率设置
    fps_option = cfg.ffmpegFps.value
    if fps_option != "source":
        cmd.extend(["-r", fps_option])

    # 视频码率限制
    if cfg.ffmpegVideoBitrate.value:
        cmd.extend(["-b:v", cfg.ffmpegVideoBitrate.value])

    # 输出格式（修正输出文件扩展名）
    output_file = adjust_output_format(output_file)

    # 覆盖输出文件
    if cfg.ffmpegOverwriteOutput.value:
        cmd.append("-y")
    else:
        cmd.append("-n")

    # 输出文件
    cmd.append(output_file)

    return cmd, output_file


class FFmpegWorker(QRunnable):
    """FFmpeg 压制执行引擎（对齐 Easy-FFmpeg：QEventLoop 同步化 + event_bus 上报）"""

    def __init__(self, task: FFmpegTask):
        super().__init__()
        self.task = task
        self.duration = 0.0
        self._last_emit = 0.0
        self.taskLogger = None
        self.process = None
        self._cancelled = False
        # 编码开始前累积 stderr，供时长解析反复求和所有 Duration
        self._stderr_buffer = ""
        self._duration_frozen = False

    def run(self):
        currentTime = self.task.createTime.toString("yyyy-MM-dd_hh-mm-ss")
        self.taskLogger = Logger(
            "Tasks/" + currentTime + f"_taskID-{self.task.task_id}", "ffmpeg"
        )
        self.task.logPath = str(self.taskLogger.logFile.absolute())

        # 在 Worker 线程内探测音频流并构建命令（不阻塞 UI 主线程）
        has_audio = probe_has_audio(self.task.videoPath)
        cmd, _ = build_ffmpeg_command(
            self.task.videoPath, self.task.output_path, has_audio
        )
        self.taskLogger.info(f"args: {cfg.get(cfg.ffmpegPath)} {' '.join(cmd)}")

        event_bus.updateTaskStatusSig.emit(
            self.task.task_id,
            0,
            TaskStatus.Processing,
            "0KiB",
            0.0,
            "0kbits/s",
            0.0,
        )

        if not self._run_stage(cmd):
            self._finish(False)
            return
        success = self.process.exitCode() == 0 and not self._cancelled
        self._finish(success)

    def _run_stage(self, args) -> bool:
        """执行单阶段 ffmpeg，返回是否成功启动

        QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失。
        self.process 始终指向当前进程，便于取消时 kill。
        """
        self.duration = 0.0
        self._last_emit = 0.0
        self._stderr_buffer = ""
        self._duration_frozen = False
        self.process = QProcess()
        self.process.readyReadStandardError.connect(self._handle_stderr)
        loop = QEventLoop()
        self.process.finished.connect(loop.quit)
        self.process.start(cfg.get(cfg.ffmpegPath), args)
        if not self.process.waitForStarted():
            return False
        loop.exec()
        return True

    def cancel(self):
        """取消任务：标记并 kill 当前进程

        取消的任务不 emit finishTaskSig（状态已由任务界面设为 Cancelled）。
        """
        self._cancelled = True
        if self.process:
            self.process.kill()

    def _try_parse_duration(self, data: str):
        """解析视频总时长：多输入时累加所有 Duration 求和，time= 出现后冻结"""
        matches = DURATION_RE.findall(data)
        if matches:
            self.duration = sum(
                int(h) * 3600 + int(m) * 60 + float(s) for h, m, s in matches
            )
        if TIME_RE.search(data):
            self._duration_frozen = True

    def _parse_progress(self, data: str):
        """解析当前压制进度，节流到每秒最多 4 次"""
        if self.duration <= 0:
            return
        now = time.time()
        if now - self._last_emit < 0.25:
            return
        match = TIME_RE.search(data)
        if not match:
            return

        self._last_emit = now
        hours = int(match.group(1))
        minutes = int(match.group(2))
        seconds = float(match.group(3))
        current = round(hours * 3600 + minutes * 60 + seconds, 2)
        progress = min(100, int(current / self.duration * 100))

        size = ""
        size_match = SIZE_RE.search(data)
        if size_match:
            size = size_match.group(1)

        bitrate = ""
        bitrate_match = BITRATE_RE.search(data)
        if bitrate_match:
            bitrate = bitrate_match.group(1)

        speed = 0.0
        speed_match = SPEED_RE.search(data)
        if speed_match:
            speed = round(float(speed_match.group(1)), 2)

        event_bus.updateTaskStatusSig.emit(
            self.task.task_id,
            progress,
            TaskStatus.Processing,
            size,
            current,
            bitrate,
            speed,
        )

    def _handle_stderr(self):
        """ffmpeg 全部输出到 stderr"""
        data = (
            self.process.readAllStandardError().data().decode("utf-8", errors="ignore")
        )
        # 编码开始前累积 stderr 用于时长解析；time= 出现后停止累积防内存增长
        if not self._duration_frozen:
            self._stderr_buffer += data
            self._try_parse_duration(self._stderr_buffer)
        self._parse_progress(data)
        self.taskLogger.info(data)

    def _finish(self, success: bool):
        """任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）"""
        if self.taskLogger:
            self.taskLogger.close()
        if self._cancelled:
            return
        event_bus.finishTaskSig.emit(self.task.task_id, success, self.task.logPath)
