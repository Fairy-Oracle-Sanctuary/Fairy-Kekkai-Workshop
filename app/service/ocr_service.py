# ocr_service.py

import os
import subprocess
import tempfile

from PySide6.QtCore import QObject, QProcess, QThread, Signal

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.task_status import TaskStatus
from ..common.text import Text

NO_WINDOW_KWARGS = (
    {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
)


class OCRTask:
    """OCR任务类"""

    _id_counter = 0

    def __init__(self, args):
        self.args = args
        self.status = TaskStatus.WAITING
        self.progress = 0
        self.error_message = ""
        self.input_file = args.get("video_path")
        self.output_file = args.get("file_path")
        self.temp_dir = args.get("temp_dir")

        OCRTask._id_counter += 1
        self.id = OCRTask._id_counter


class OCRProcess(QObject):
    """OCR处理进程"""

    progress_signal = Signal(int, int, int)  # 进度百分比, 当前帧, 总帧数
    finished_signal = Signal(bool, str)  # 成功/失败, 消息
    log_signal = Signal(str, bool, bool)  # 日志信息
    print_signal = Signal(str)  # 捕获print输出
    cancelled_signal = Signal()  # 取消完成信号

    def __init__(self, task: OCRTask):
        super().__init__()
        self.globalText = Text()
        self.logger = Logger("OCRProcess", "videocr")
        self.task = task
        self.is_cancelled = False
        self.process = None
        self.output_lines = []  # 存储输出用于错误诊断
        self._cancellation_timer = None
        self._last_step1_progress = None

    def cleanup(self):
        """清理子进程资源，由各完成/错误/取消路径显式调用"""
        if self.process:
            if self.process.state() == QProcess.Running:
                self.process.kill()
                self.process.waitForFinished(1000)
            self.process = None
        if self._cancellation_timer:
            self._cancellation_timer.stop()
            self._cancellation_timer = None

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

    def build_ocr_command(self):
        """根据配置构建 ocr 命令"""
        args = self.task.args

        cmd_path = cfg.get(cfg.videocrCliPath)

        # 构建命令参数
        cmd_args = []

        # 添加参数
        cmd_args.extend(["--video_path", args["video_path"]])
        cmd_args.extend(["--output", args["file_path"]])
        # cmd_args.extend(["--ocr_engine", args["paddleocr"]])
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

        # # 处理paddleocr_path参数
        if "paddleocr_path" in args and args["paddleocr_path"]:
            cmd_args.extend(["--paddleocr_path", args["paddleocr_path"]])

        # # 处理supportFilesPath参数
        if "supportFilesPath" in args and args["supportFilesPath"]:
            cmd_args.extend(["--supportFilesPath", args["supportFilesPath"]])

        # 处理tempDir参数
        if "temp_dir" in args and args["temp_dir"]:
            cmd_args.extend(["--tempDir", args["temp_dir"]])

        # 处理crop_zones参数
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

    def start(self):
        """启动OCR处理进程"""
        self.task.status = TaskStatus.PROCESSING

        try:
            # 清理临时目录
            if self.task.temp_dir and os.path.exists(self.task.temp_dir):
                import shutil

                try:
                    shutil.rmtree(self.task.temp_dir)
                    self.log_signal.emit(
                        self.globalText.TextAuto024.format(self.task.temp_dir),
                        False,
                        False,
                    )
                except Exception as e:
                    self.log_signal.emit(
                        self.globalText.TextAuto026.format(str(e)), True, False
                    )

            # 获取videocr-cli.exe路径
            cmd_path, cmd_args = self.build_ocr_command()

            if not os.path.exists(cmd_path):
                error_msg = f"videocr-cli.exe不存在: {cmd_path}"
                self.task.status = TaskStatus.FAILED
                self.task.error_message = error_msg
                self.finished_signal.emit(False, error_msg)
                event_bus.ocr_finished_signal.emit(False, error_msg)
                return

            # 确保输出目录存在
            output_dir = os.path.dirname(self.task.output_file)
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)

            print(f"执行OCR命令: {cmd_path} {' '.join(cmd_args)}")

            # 创建QProcess
            self.process = QProcess()

            # 合并 stdout 和 stderr 到一个通道，确保所有输出都能被实时捕获
            self.process.setProcessChannelMode(QProcess.MergedChannels)

            # 连接信号
            self.process.readyReadStandardOutput.connect(self.handle_stdout)
            self.process.finished.connect(self.handle_finished)
            self.process.errorOccurred.connect(self.handle_error)

            # 设置程序和参数
            self.process.setProgram(cmd_path)
            self.process.setArguments(cmd_args)

            # 启动进程
            self.process.start()

        except Exception as e:
            if not self.is_cancelled:
                error_msg = f"OCR处理失败: {str(e)}"
                print(error_msg)
                self.task.status = TaskStatus.FAILED
                self.task.error_message = error_msg
                self.finished_signal.emit(False, error_msg)
                event_bus.ocr_finished_signal.emit(False, error_msg)
                self.logger.error(
                    f"OCR处理失败: -{self.task.input_file}- 错误信息: {str(e)}"
                )

    def _should_emit_line(self, line):
        if "Step 1/3" in line and "Current:" in line:
            current_part = line.split("Current:", 1)[-1].split("/", 1)[0].strip()
            if current_part == self._last_step1_progress:
                return False
            self._last_step1_progress = current_part
        return True

    def handle_stdout(self):
        """处理标准输出"""
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

            # 如果已取消，不再发送print信号
            if self.is_cancelled:
                continue

            if not self._should_emit_line(line):
                continue

            # 发射print信号，由videocr_task_interface.py中的onPrintOutput处理
            self.print_signal.emit(line)

    def handle_stderr(self):
        """处理标准错误"""
        if not self.process:
            return

        data = (
            self.process.readAllStandardError().data().decode("utf-8", errors="ignore")
        )
        lines = data.replace("\r\n", "\n").replace("\r", "\n").split("\n")

        for line in lines:
            line = line.strip()
            if not line:
                continue

            self.output_lines.append(line)

            # 如果已取消，不再发送print信号
            if self.is_cancelled:
                continue

            if not self._should_emit_line(line):
                continue

            # 发射print信号，由videocr_task_interface.py中的onPrintOutput处理
            self.print_signal.emit(line)

    def handle_finished(self, exit_code, exit_status):
        """进程完成处理"""
        if self.is_cancelled:
            self.task.status = TaskStatus.CANCELLED
            self.finished_signal.emit(False, self.globalText.TextAuto020)
            self.cancelled_signal.emit()
            self.logger.info(f"OCR处理已取消: -{self.task.input_file}-")
        elif exit_code == 0:
            self.task.status = TaskStatus.DONE
            self.task.progress = 100

            # 检查输出文件是否存在
            if os.path.exists(self.task.output_file):
                file_size = os.path.getsize(self.task.output_file) / (1024 * 1024)  # MB
                success_msg = f"OCR处理完成 - 文件大小: {file_size:.2f}MB"
            else:
                success_msg = "OCR处理完成"

            # 自动复制到 原文.srt（设为当前活动原文）
            self._activate_as_current(self.task.output_file, self.task.input_file)

            self.finished_signal.emit(True, success_msg)
            self.log_signal.emit(self.globalText.TextAuto021, False, False)
            event_bus.ocr_finished_signal.emit(True, str(self.task.output_file))
            self.logger.info(
                f"OCR处理完成: -{self.task.input_file}- 输出文件: {self.task.output_file}"
            )
        else:
            error_message = f"OCR处理失败，错误码: {exit_code}"

            # 添加最后几行输出作为调试信息
            if self.output_lines:
                last_lines = "\n".join(self.output_lines[-5:])
                error_message += f"\n最后输出:\n{last_lines}"

            self.task.status = TaskStatus.FAILED
            self.task.error_message = error_message
            self.finished_signal.emit(False, error_message)
            self.log_signal.emit(
                self.globalText.TextAuto023.format(error_message), False, False
            )
            event_bus.ocr_finished_signal.emit(False, error_message)
            self.logger.error(
                f"OCR处理失败: -{self.task.input_file}- 错误信息: {error_message}"
            )

    def handle_error(self, error):
        """处理进程错误"""
        if self.is_cancelled:
            return

        error_map = {
            QProcess.FailedToStart: "进程启动失败",
            QProcess.Crashed: "进程崩溃",
            QProcess.Timedout: "进程超时",
            QProcess.WriteError: "写入错误",
            QProcess.ReadError: "读取错误",
            QProcess.UnknownError: "未知错误",
        }

        error_msg = error_map.get(error, f"进程错误: {error}")
        self.finished_signal.emit(False, error_msg)

    def cancel(self):
        """取消OCR处理 - 异步非阻塞版本"""
        if self.is_cancelled:
            return

        self.is_cancelled = True

        # 立即发送取消日志，不等待进程结束
        self.log_signal.emit(self.globalText.TextAuto019, False, False)

        if self.process and self.process.state() == QProcess.Running:
            # 获取进程ID
            pid = self.process.processId()

            if os.name == "nt":
                # 使用taskkill强制终止进程树（包括子进程）
                try:
                    subprocess.run(
                        ["taskkill", "/F", "/T", "/PID", str(pid)],
                        capture_output=True,
                        timeout=2,
                        **NO_WINDOW_KWARGS,
                    )
                    self.log_signal.emit(self.globalText.TextAuto022, False, False)
                except Exception as e:
                    self.log_signal.emit(
                        self.globalText.TextAuto025.format(str(e)), True, False
                    )
                    # 如果taskkill失败，尝试使用QProcess的kill
                    self.process.kill()
            else:
                self.process.kill()

            # 等待进程结束
            if self.process.waitForFinished(2000):
                self.cancelled_signal.emit()
            else:
                self.cancelled_signal.emit()
        else:
            # 如果没有进程在运行，直接发送取消完成信号
            self.cancelled_signal.emit()


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

            # 解析模型目录
            from .CLI.videocr import utils

            det_model_dir, rec_model_dir, cls_model_dir = utils.resolve_model_dirs(
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
            import re

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
            self.logger.error(f"屏幕OCR失败: {str(e)}")
            event_bus.screen_ocr_finished.emit(False, f"屏幕OCR失败: {str(e)}")
        finally:
            # 清理临时目录
            try:
                import shutil

                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir, ignore_errors=True)
            except Exception:
                pass

    def cancel(self):
        self._cancelled = True
