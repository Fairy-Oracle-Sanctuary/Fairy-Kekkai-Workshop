import os
import re

from PySide6.QtCore import QDateTime, QEventLoop, QProcess, QRunnable

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.task_status import TaskStatus

# 进度解析正则：时间戳行，如 [00:00:01.480 --> 00:00:03.200]
TIMESTAMP_RE = re.compile(r"\[(\d+:\d+:\d+\.\d+)\s*-->\s*(\d+:\d+:\d+\.\d+)\]")
DURATION_RE = re.compile(r"Duration:\s*(\d+):(\d+):(\d+\.\d+)")


def get_whisper_cli_path():
    """获取 whisper main.exe 路径"""
    custom_path = cfg.get(cfg.whisperCliPath)
    if custom_path and os.path.exists(custom_path):
        return custom_path

    return os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
        "tools",
        "whisper",
        "main.exe",
    )


class WhisperTask:
    """Whisper 转录任务纯数据类（对齐 Easy-FFmpeg 链路）"""

    _id_counter = 0

    def __init__(self, args):
        WhisperTask._id_counter += 1
        self.task_id = WhisperTask._id_counter
        self.args = args
        self.input_file = args["video_path"]
        self.output_file = args["output_path"]
        self.model_file = args.get("model", "")
        self.language = args.get("language", "")
        self.format = args.get("format", "srt")
        self.gpu = args.get("gpu", "")
        self.fileName = os.path.basename(self.input_file) if self.input_file else ""
        self.outputName = os.path.basename(self.output_file) if self.output_file else ""
        self.logPath = None
        self.createTime = QDateTime.currentDateTime()
        self.duration = 0.0


class WhisperWorker(QRunnable):
    """Whisper 转录执行引擎（对齐 Easy-FFmpeg：QEventLoop 同步化 + event_bus 上报）

    取消使用 process.kill() 直接终止；取消的任务不 emit 完成信号。
    """

    def __init__(self, task: WhisperTask):
        super().__init__()
        self.task = task
        self.process = None
        self._cancelled = False
        self.output_lines = []  # 存储输出用于错误诊断
        self.taskLogger = None
        self._last_progress = -1

    def run(self):
        currentTime = self.task.createTime.toString("yyyy-MM-dd_hh-mm-ss")
        self.taskLogger = Logger(
            "Tasks/" + currentTime + f"_taskID-{self.task.task_id}", "whisper"
        )
        self.task.logPath = str(self.taskLogger.logFile.absolute())

        try:
            cli_path = get_whisper_cli_path()
            if not os.path.exists(cli_path):
                self.taskLogger.error(f"main.exe 不存在: {cli_path}")
                self._finish(False)
                return
            if self.task.model_file and not os.path.exists(self.task.model_file):
                self.taskLogger.error(f"模型文件不存在: {self.task.model_file}")
                self._finish(False)
                return

            output_dir = os.path.dirname(self.task.output_file)
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)

            # 获取视频总时长用于进度计算
            self.task.duration = self._get_video_duration()

            cmd = self.build_whisper_command()
            self.taskLogger.info(f"args: {cli_path} {' '.join(cmd[1:])}")

            event_bus.updateTaskStatusSig.emit(
                self.task.task_id, 0, TaskStatus.Processing, "", 0.0, "", 0.0
            )

            # QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失
            self.process = QProcess()
            self.process.setProcessChannelMode(QProcess.MergedChannels)
            self.process.readyReadStandardOutput.connect(self._handle_stdout)
            loop = QEventLoop()
            self.process.finished.connect(loop.quit)
            self.process.setProgram(cli_path)
            self.process.setArguments(cmd[1:])
            # 设置工作目录为 whisper 目录，方便加载相对路径的模型
            self.process.setWorkingDirectory(os.path.dirname(cli_path))
            self.process.start()
            if not self.process.waitForStarted():
                self._finish(False)
                return
            loop.exec()

            if self._cancelled:
                return  # 取消的任务不 emit 完成信号

            success = self.process.exitCode() == 0
            if success:
                self._activate_output()
                self.taskLogger.info(
                    f"Whisper转录完成: -{self.task.input_file}- 输出: {self.task.output_file}"
                )
            else:
                error_msg = f"Whisper转录失败，错误码: {self.process.exitCode()}"
                if self.output_lines:
                    error_msg += "\n最后输出:\n" + "\n".join(self.output_lines[-5:])
                self.taskLogger.error(error_msg)
            self._finish(success)
        except Exception as e:
            self.taskLogger.error(f"Whisper转录失败: {e!s}")
            self._finish(False)

    def build_whisper_command(self):
        """构建 Whisper main.exe 命令"""
        cmd = [get_whisper_cli_path()]
        cmd.extend(["-f", self.task.input_file])

        # 仅在指定了有效语言时传递 -l 参数，否则让 whisper 自动检测
        if self.task.language and self.task.language != "auto":
            cmd.extend(["-l", self.task.language])

        # 输出格式
        if self.task.format == "srt":
            cmd.append("-osrt")
        elif self.task.format == "txt":
            cmd.append("-otxt")
        elif self.task.format == "vtt":
            cmd.append("-ovtt")

        # 当 GPU 不为空时添加 GPU 参数（包括"自动检测"）
        if self.task.gpu:
            cmd.append("-gpu")

        # 模型路径
        if self.task.model_file:
            cmd.extend(["-m", self.task.model_file])

        return cmd

    def _get_video_duration(self):
        """获取视频总时长（秒），失败返回 0"""
        try:
            ffmpeg_path = cfg.get(cfg.ffmpegPath)
            if not ffmpeg_path or not os.path.exists(ffmpeg_path):
                return 0

            process = QProcess()
            process.start(ffmpeg_path, ["-i", self.task.input_file])
            process.waitForFinished(5000)
            stderr_output = (
                process.readAllStandardError().data().decode("utf-8", errors="ignore")
            )

            # 从 FFmpeg 输出中解析时长信息: Duration: 00:00:05.03
            duration_match = DURATION_RE.search(stderr_output)
            if duration_match:
                hours = int(duration_match.group(1))
                minutes = int(duration_match.group(2))
                seconds = float(duration_match.group(3))
                total_seconds = hours * 3600 + minutes * 60 + seconds
                self.taskLogger.info(f"获取视频时长成功: {total_seconds}秒")
                return total_seconds
        except Exception as e:
            self.taskLogger.error(f"获取视频时长失败: {e!s}")
        return 0

    @staticmethod
    def _timestamp_to_seconds(timestamp_str):
        """将时间戳字符串转换为秒数
        格式: 00:00:01.480 或 00:01:30
        """
        try:
            parts = timestamp_str.strip().split(":")
            if len(parts) == 3:
                hours = int(parts[0])
                minutes = int(parts[1])
                seconds = float(parts[2])
                return hours * 3600 + minutes * 60 + seconds
            elif len(parts) == 2:
                minutes = int(parts[0])
                seconds = float(parts[1])
                return minutes * 60 + seconds
        except (ValueError, IndexError):
            pass
        return 0

    def _activate_output(self):
        """Whisper CLI 无 -o 参数：输出自动生成在输入文件旁，重命名为任务输出，再复制为 原文.srt"""
        input_dir = os.path.dirname(self.task.input_file)
        input_stem = os.path.splitext(os.path.basename(self.task.input_file))[0]
        possible_outputs = [
            os.path.join(input_dir, f"{input_stem}.mp4.{self.task.format}"),
            os.path.join(input_dir, f"{input_stem}.{self.task.format}"),
        ]
        for candidate in possible_outputs:
            if os.path.exists(candidate) and candidate != self.task.output_file:
                try:
                    os.replace(candidate, self.task.output_file)
                    break
                except Exception:
                    pass
        # 自动复制到 原文.srt（设为当前活动原文）
        self._activate_as_current(self.task.output_file, self.task.input_file)

    @staticmethod
    def _activate_as_current(output_file: str, input_file: str = None):
        """将提取结果复制为 原文.srt（作为当前活动原文）"""
        import shutil

        parent_dir = os.path.dirname(output_file)
        current_file = os.path.join(parent_dir, "原文.srt")

        # 如果输出文件已经是 原文.srt，则直接返回
        if os.path.abspath(output_file) == os.path.abspath(current_file):
            return

        # 只有当视频名为 生肉.mp4 且 原文.srt 不存在时才复制
        if input_file:
            input_filename = os.path.basename(input_file)
            if input_filename != "生肉.mp4":
                return

        # 如果 原文.srt 已存在，则不复制
        if os.path.exists(current_file):
            return

        try:
            if os.path.exists(output_file) and os.path.getsize(output_file) > 0:
                shutil.copy2(output_file, current_file)
        except Exception:
            pass

    def _handle_stdout(self):
        """处理合并后的标准输出（进度 + 日志）"""
        if not self.process:
            return

        data = (
            self.process.readAllStandardOutput().data().decode("utf-8", errors="ignore")
        )
        lines = data.replace("\r\n", "\n").replace("\r", "\n").split("\n")

        for line in lines:
            line = line.strip()
            if not line:
                continue
            self.output_lines.append(line)
            self.taskLogger.info(f"Whisper: {line}")

            if self._cancelled:
                continue

            # 进度行 → 刷新式日志（覆盖上一行）+ 进度上报
            progress = self._parse_progress(line)
            if progress is not None:
                if progress != self._last_progress:
                    self._last_progress = progress
                    event_bus.updateTaskStatusSig.emit(
                        self.task.task_id,
                        progress,
                        TaskStatus.Processing,
                        "",
                        0.0,
                        "",
                        0.0,
                    )
                event_bus.taskLogSignal.emit("whisper", line, False, True)
                continue

            # 错误行 → 红色错误日志
            if self._is_error_line(line):
                event_bus.taskLogSignal.emit("whisper", line, True, False)
                continue

            # 普通日志
            event_bus.taskLogSignal.emit("whisper", line, False, False)

    @staticmethod
    def _is_error_line(line: str) -> bool:
        """判断是否为错误输出行"""
        return "error" in line.lower() or "failed" in line.lower() or "错误" in line

    def _parse_progress(self, line: str):
        """解析时间戳行计算进度（0-100）；完成标志返回 100"""
        timestamp_match = TIMESTAMP_RE.search(line)
        if timestamp_match:
            current_seconds = self._timestamp_to_seconds(timestamp_match.group(2))
            if self.task.duration > 0 and current_seconds > 0:
                return min(100, int((current_seconds / self.task.duration) * 100))
        # 检测完成标志
        if "LoadModel" in line and "RunComplete" in line:
            return 100
        return None

    def cancel(self):
        """取消转录：标记并 kill 进程（取消的任务不 emit 完成信号）"""
        self._cancelled = True
        if self.process:
            self.process.kill()

    def _finish(self, success: bool):
        """任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）"""
        if self.taskLogger:
            self.taskLogger.close()
        if self._cancelled:
            return
        event_bus.finishTaskSig.emit(self.task.task_id, success, self.task.logPath)
