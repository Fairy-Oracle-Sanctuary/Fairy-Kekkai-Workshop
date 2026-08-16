# ocr_service.py

import os
import re
import shutil
import subprocess
import tempfile

from PySide6.QtCore import QDateTime, QEventLoop, QProcess, QRunnable, QThread

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.paddleocr import resolve_model_dirs
from ..common.task_status import TaskStatus

NO_WINDOW_KWARGS = (
    {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
)

# 进度解析正则
STEP1_CURRENT_RE = re.compile(r"Current:\s+(\d+:\d+:\d+)\s+/\s+(\d+:\d+:\d+)")
STEP2_DETECT_RE = re.compile(r"Performing Text-Detection on image\s+(\d+)\s+of\s+(\d+)")
ANALYZE_FRAME_RE = re.compile(r"Analyzing frame\s+(\d+)\s+of\s+(\d+)")
STEP3_OCR_RE = re.compile(r"Performing OCR on image\s+(\d+)\s+of\s+(\d+)")


def _time_to_seconds(time_str: str) -> int:
    """将 HH:MM:SS / MM:SS 时间字符串转换为秒"""
    parts = time_str.split(":")
    try:
        if len(parts) == 3:
            return int(parts[0]) * 3600 + int(parts[1]) * 60 + int(parts[2])
        elif len(parts) == 2:
            return int(parts[0]) * 60 + int(parts[1])
    except ValueError:
        return 0
    return 0


class OCRTask:
    """OCR 字幕提取任务（纯数据类）"""

    _id_counter = 0

    def __init__(self, args):
        OCRTask._id_counter += 1
        self.task_id = OCRTask._id_counter
        self.args = args
        self.videoPath = args.get("video_path")
        self.outputFile = args.get("file_path")
        self.temp_dir = args.get("temp_dir")
        self.fileName = os.path.basename(self.videoPath) if self.videoPath else ""
        self.outputName = os.path.basename(self.outputFile) if self.outputFile else ""
        self.logPath = None
        self.createTime = QDateTime.currentDateTime()


class OCRWorker(QRunnable):
    """OCR 字幕提取 Worker（对齐 Easy-FFmpeg：QEventLoop 同步化 + event_bus 上报）

    取消使用 taskkill 强杀进程树（videocr-cli 会派生子进程）。
    """

    def __init__(self, task: OCRTask):
        super().__init__()
        self.task = task
        self.process = None
        self._cancelled = False
        self.output_lines = []  # 存储输出用于错误诊断
        self.taskLogger = None
        self._last_step1_progress = None
        self._last_progress = -1

    def run(self):
        currentTime = self.task.createTime.toString("yyyy-MM-dd_hh-mm-ss")
        self.taskLogger = Logger(
            "Tasks/" + currentTime + f"_taskID-{self.task.task_id}", "videocr"
        )
        self.task.logPath = str(self.taskLogger.logFile.absolute())

        try:
            # 清理临时目录
            if self.task.temp_dir and os.path.exists(self.task.temp_dir):
                try:
                    shutil.rmtree(self.task.temp_dir)
                except Exception as e:
                    self.taskLogger.error(f"清理临时目录失败: {e!s}")

            # 构建命令并检查可执行文件
            cmd_path, cmd_args = self.build_ocr_command()
            if not os.path.exists(cmd_path):
                self.taskLogger.error(f"videocr-cli.exe 不存在: {cmd_path}")
                self._finish(False)
                return

            # 确保输出目录存在
            output_dir = os.path.dirname(self.task.outputFile)
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)

            self.taskLogger.info(f"args: {cmd_path} {' '.join(cmd_args)}")

            event_bus.updateTaskStatusSig.emit(
                self.task.task_id, 0, TaskStatus.Processing, "", 0.0, "", 0.0
            )

            # QEventLoop 阻塞同步化，规避 QRunnable 线程内 QProcess 信号投递丢失
            self.process = QProcess()
            self.process.setProcessChannelMode(QProcess.MergedChannels)
            self.process.readyReadStandardOutput.connect(self._handle_stdout)
            loop = QEventLoop()
            self.process.finished.connect(loop.quit)
            self.process.start(cmd_path, cmd_args)
            if not self.process.waitForStarted():
                self._finish(False)
                return
            loop.exec()

            if self._cancelled:
                return  # 取消的任务不 emit 完成信号

            success = self.process.exitCode() == 0
            if success:
                # 自动复制到 原文.srt（设为当前活动原文）
                self._activate_as_current(self.task.outputFile, self.task.videoPath)
                self.taskLogger.info(
                    f"OCR处理完成: -{self.task.videoPath}- 输出: {self.task.outputFile}"
                )
            else:
                error_msg = f"OCR处理失败，错误码: {self.process.exitCode()}"
                if self.output_lines:
                    error_msg += "\n最后输出:\n" + "\n".join(self.output_lines[-5:])
                self.taskLogger.error(f"{error_msg}")
            self._finish(success)
        except Exception as e:
            self.taskLogger.error(f"OCR处理失败: {e!s}")
            self._finish(False)

    def build_ocr_command(self):
        """根据配置构建 videocr-cli 命令"""
        args = self.task.args
        cmd_path = cfg.get(cfg.videocrCliPath)

        cmd_args = []
        cmd_args.extend(["--video_path", args["video_path"]])
        cmd_args.extend(["--output", args["file_path"]])
        cmd_args.extend(["--lang", args["lang"]])
        cmd_args.extend(["--time_start", args["time_start"]])
        if args["time_end"]:
            cmd_args.extend(["--time_end", args["time_end"]])
        cmd_args.extend(["--sim_threshold", str(args["sim_threshold"])])
        cmd_args.extend(["--max_merge_gap", str(args["max_merge_gap_sec"])])
        cmd_args.extend(["--use_fullframe", str(args["use_fullframe"]).lower()])
        cmd_args.extend(["--use_gpu", str(args["use_gpu"]).lower()])
        cmd_args.extend(["--use_angle_cls", str(args["use_angle_cls"]).lower()])
        cmd_args.extend(["--use_server_model", str(args["use_server_model"]).lower()])
        cmd_args.extend(["--ssim_threshold", str(args["ssim_threshold"])])
        cmd_args.extend(["--subtitle_position", args["subtitle_position"]])
        cmd_args.extend(["--frames_to_skip", str(args["frames_to_skip"])])
        cmd_args.extend(["--ocr_image_max_width", str(args["ocr_image_max_width"])])
        cmd_args.extend(["--post_processing", str(args["post_processing"]).lower()])
        cmd_args.extend(
            ["--min_subtitle_duration", str(args["min_subtitle_duration_sec"])]
        )
        cmd_args.extend(["--conf_threshold", str(args["confidence_threshold"])])

        if args.get("paddleocr_path"):
            cmd_args.extend(["--paddleocr_path", args["paddleocr_path"]])
        if args.get("supportFilesPath"):
            cmd_args.extend(["--supportFilesPath", args["supportFilesPath"]])
        if args.get("temp_dir"):
            cmd_args.extend(["--tempDir", args["temp_dir"]])

        # 裁剪区域
        cmd_args.extend(["--crop_x", str(args["--crop_x"])])
        cmd_args.extend(["--crop_y", str(args["--crop_y"])])
        cmd_args.extend(["--crop_width", str(args["--crop_width"])])
        cmd_args.extend(["--crop_height", str(args["--crop_height"])])
        if args["use_dual_zone"]:
            cmd_args.extend(["--crop_x2", str(args["--crop_x2"])])
            cmd_args.extend(["--crop_y2", str(args["--crop_y2"])])
            cmd_args.extend(["--crop_width2", str(args["--crop_width2"])])
            cmd_args.extend(["--crop_height2", str(args["--crop_height2"])])

        return cmd_path, cmd_args

    @staticmethod
    def _activate_as_current(output_file: str, input_file: str = None):
        """将提取结果复制为 原文.srt（作为当前活动原文）"""
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

    def _should_emit_line(self, line: str) -> bool:
        """Step 1/3 相同进度行去重"""
        if "Step 1/3" in line and "Current:" in line:
            current_part = line.split("Current:", 1)[-1].split("/", 1)[0].strip()
            if current_part == self._last_step1_progress:
                return False
            self._last_step1_progress = current_part
        return True

    def _handle_stdout(self):
        """处理标准输出（进度 + 日志）"""
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
            self.taskLogger.info(line)

            if self._cancelled:
                continue
            if not self._should_emit_line(line):
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
                event_bus.taskLogSignal.emit("videocr", line, False, True)
                continue

            # 错误行 → 红色错误日志
            if self._is_error_line(line):
                event_bus.taskLogSignal.emit("videocr", line, True, False)
                continue

            # 普通日志
            event_bus.taskLogSignal.emit("videocr", line, False, False)

    @staticmethod
    def _is_error_line(line: str) -> bool:
        """判断是否为错误输出行"""
        return any(
            k in line
            for k in (
                "找不到PaddleOCR路径",
                "无法找到PaddleOCR可执行文件",
                "Error: PaddleOCR failed",
            )
        )

    def _parse_progress(self, line: str):
        """解析 OCR 三段式输出并映射为 0-100 进度"""
        # Step 1/3: Processing video... Current: HH:MM:SS / HH:MM:SS (0-33)
        if "Step 1/3" in line:
            m = STEP1_CURRENT_RE.search(line)
            if m:
                total = _time_to_seconds(m.group(2))
                if total > 0:
                    current = _time_to_seconds(m.group(1))
                    return min((current / total) * 33, 33)

        # Step 2/3: Text-Detection (33-53)
        elif "Step 2/3" in line and "Text-Detection" in line:
            m = STEP2_DETECT_RE.search(line)
            if m:
                total = int(m.group(2))
                if total > 0:
                    current = int(m.group(1))
                    return min(33 + (current / total) * 20, 53)

        # Analyzing frame (53-66)
        elif "Analyzing frame" in line:
            m = ANALYZE_FRAME_RE.search(line)
            if m:
                total = int(m.group(2))
                if total > 0:
                    current = int(m.group(1))
                    return min(53 + ((current - 1) / total) * 13, 66)

        # Step 3/3: Performing OCR (66-100)
        elif "Step 3/3" in line and "Performing OCR" in line:
            m = STEP3_OCR_RE.search(line)
            if m:
                total = int(m.group(2))
                if total > 0:
                    current = int(m.group(1))
                    return min(66 + (current / total) * 34, 100)

        return None

    def cancel(self):
        """取消 OCR 处理：标记 + taskkill 强杀进程树（videocr 有子进程）"""
        self._cancelled = True
        if self.process and self.process.state() == QProcess.Running:
            pid = self.process.processId()
            if os.name == "nt":
                try:
                    subprocess.run(
                        ["taskkill", "/F", "/T", "/PID", str(pid)],
                        capture_output=True,
                        timeout=2,
                        **NO_WINDOW_KWARGS,
                    )
                except Exception:
                    self.process.kill()
            else:
                self.process.kill()
            self.process.waitForFinished(2000)

    def _finish(self, success: bool):
        """任务结束统一处理：关日志、emit 完成信号（取消的任务不 emit）"""
        if self.taskLogger:
            self.taskLogger.close()
        if self._cancelled:
            return
        event_bus.finishTaskSig.emit(self.task.task_id, success, self.task.logPath)


class ScreenOCRThread(QThread):
    """屏幕区域 OCR 线程

    截取屏幕指定区域 → 保存临时图片 → 调用 paddleocr.exe ocr → 解析 JSON → 通过 event_bus 通知
    """

    def __init__(self, rect, parent=None):
        super().__init__(parent)
        self.rect = rect  # QRect (屏幕全局坐标)
        self.logger = Logger("ScreenOCRThread", "screen_ocr")
        self._cancelled = False

    def run(self):
        try:
            lang = cfg.get(cfg.ocr_lang)
            use_gpu = cfg.get(cfg.useGpu)
            use_angle_cls = cfg.get(cfg.useAngleCls)
            use_server_model = cfg.get(cfg.useServerModel)

            event_bus.screen_ocr_started.emit()
            event_bus.screen_ocr_log.emit("正在截取屏幕区域...")

            # 1. 截取屏幕区域
            from PySide6.QtWidgets import QApplication

            screen = QApplication.primaryScreen()
            pixmap = screen.grabWindow(
                0, self.rect.x(), self.rect.y(), self.rect.width(), self.rect.height()
            )
            if pixmap.isNull():
                event_bus.screen_ocr_finished.emit(False, "截取屏幕失败")
                return

            # 2. 保存为临时图片
            temp_dir = tempfile.mkdtemp(prefix="screen_ocr_")
            img_path = os.path.join(temp_dir, "capture.png")
            pixmap.save(img_path, "PNG")

            event_bus.screen_ocr_log.emit(f"已保存截图: {img_path}")

            # 3. 构建 paddleocr 命令
            paddleocr_path = cfg.get(cfg.paddleocrPath)
            if not os.path.exists(paddleocr_path):
                event_bus.screen_ocr_finished.emit(
                    False, f"paddleocr 不存在: {paddleocr_path}"
                )
                return

            support_files_path = cfg.get(cfg.supportFilesPath)
            if support_files_path:
                support_files_path = os.path.normpath(support_files_path)

            # 解析模型目录
            det_model_dir, rec_model_dir, cls_model_dir = resolve_model_dirs(
                lang, use_server_model, support_files_path
            )

            cmd_args = [
                paddleocr_path,
                "ocr",
                "--input",
                temp_dir,
                "--device",
                "gpu" if use_gpu else "cpu",
                "--use_textline_orientation",
                "true" if use_angle_cls else "false",
                "--use_doc_orientation_classify",
                "false",
                "--use_doc_unwarping",
                "false",
                "--lang",
                lang,
                "--text_detection_model_dir",
                det_model_dir,
                "--text_detection_model_name",
                os.path.basename(det_model_dir),
                "--text_recognition_model_dir",
                rec_model_dir,
                "--text_recognition_model_name",
                os.path.basename(rec_model_dir),
            ]

            if use_angle_cls:
                cmd_args += ["--textline_orientation_model_dir", cls_model_dir]
                cmd_args += [
                    "--textline_orientation_model_name",
                    os.path.basename(cls_model_dir),
                ]

            event_bus.screen_ocr_log.emit("启动 PaddleOCR...")

            # 4. 执行 CLI 进程
            cli_env = os.environ.copy()
            cli_env["PYTHONIOENCODING"] = "utf-8"
            cli_env["PYTHONUNBUFFERED"] = "1"

            process = subprocess.Popen(
                cmd_args,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                env=cli_env,
                bufsize=1,
                **NO_WINDOW_KWARGS,
            )

            stdout_lines = []
            for line in iter(process.stdout.readline, ""):
                line = line.strip()
                if not line:
                    continue
                stdout_lines.append(line)
                if "ppocr INFO:" in line:
                    event_bus.screen_ocr_log.emit(line)
                if self._cancelled:
                    process.kill()
                    break

            process.wait()

            if self._cancelled:
                event_bus.screen_ocr_finished.emit(False, "已取消")
                return

            if process.returncode != 0:
                stderr_data = process.stderr.read() if process.stderr else ""
                last_stdout = "\n".join(stdout_lines[-10:]) if stdout_lines else ""
                diag = f"PaddleOCR 失败 (code={process.returncode})\n命令: {' '.join(cmd_args)}\nstderr:\n{stderr_data}\nstdout(最后10行):\n{last_stdout}"
                print(diag)
                event_bus.screen_ocr_finished.emit(
                    False,
                    f"PaddleOCR 失败 (code={process.returncode}): {stderr_data[:500]}",
                )
                return

            # 5. 解析 ppocr INFO 输出，提取文本
            import ast

            texts = []
            for line in stdout_lines:
                line = line.strip()
                # 匹配 ppocr INFO: [[[x,y],...], ('text', score)] 格式
                m = re.search(r"ppocr INFO:\s*(\[.*\])\s*$", line)
                if not m:
                    continue
                try:
                    parsed = ast.literal_eval(m.group(1))
                    # parsed 格式: [[[x1,y1],[x2,y2],[x3,y3],[x4,y4]], ('text', confidence)]
                    if isinstance(parsed, list) and len(parsed) == 2:
                        text = parsed[1][0]
                        if isinstance(text, str) and text.strip():
                            texts.append(text.strip())
                except (ValueError, SyntaxError, IndexError):
                    continue

            result_text = "\n".join(texts)
            event_bus.screen_ocr_log.emit(f"识别完成，共 {len(texts)} 行文本")
            event_bus.screen_ocr_finished.emit(True, result_text)

        except Exception as e:
            self.logger.error(f"屏幕OCR失败: {e!s}")
            event_bus.screen_ocr_finished.emit(False, f"屏幕OCR失败: {e!s}")
        finally:
            # 清理临时目录
            try:
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir, ignore_errors=True)
            except Exception:
                pass

    def cancel(self):
        self._cancelled = True
