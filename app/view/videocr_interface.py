# videocr_task_interface.py
# coding:utf-8


import os
import re

import cv2
from PySide6.QtCore import Qt, QTime
from PySide6.QtWidgets import (
    QHBoxLayout,
    QVBoxLayout,
)
from qfluentwidgets import (
    BodyLabel,
    CaptionLabel,
    Dialog,
    PushButton,
    SimpleCardWidget,
    Slider,
    StrongBodyLabel,
    TextEdit,
)
from qfluentwidgets import FluentIcon as FIF

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.text import Text
from ..components.base_function_interface import BaseFunctionInterface
from ..components.base_stacked_interface import BaseStackedInterfaces
from ..components.config_card import DictSettingCard, OCRSettingInterface
from ..service.video_service import VideoPreview
from ..view.videocr_task_interface import OcrTaskInterface


class VideocrStackedInterfaces(BaseStackedInterfaces):
    """视频OCR堆叠界面"""

    def __init__(self, parent=None):
        globalText = Text()
        super().__init__(
            parent=parent,
            main_interface_class=VideocrInterface,
            task_interface_class=OcrTaskInterface,
            setting_interface_class=OCRSettingInterface,
            interface_name=globalText.SubtitleExtraction,
        )
        self.globalText = globalText

        # 连接专用信号
        self.mainInterface.addTask.connect(self.taskInterface.addOcrTask)
        self.taskInterface.log_signal.connect(self.mainInterface._log_message)
        self.taskInterface.returnTask.connect(self.mainInterface.updateTask)
        self.settingInterface.changeSelectionSignal.connect(self.changeSelection)

    def changeSelection(self, isUseDualZone):
        if isUseDualZone:
            self.mainInterface.video_preview.set_max_crop_boxes(2)
        else:
            self.mainInterface.video_preview.set_max_crop_boxes(1)

    def onCurrentIndexChanged(self, index):
        widget = self.stackedWidget.widget(index)
        self.pivot.setCurrentItem(widget.objectName())
        self.mainInterface.refresh_start_btn()


class VideocrInterface(BaseFunctionInterface):
    """视频OCR字幕提取界面"""

    def __init__(self, parent=None):
        globalText = Text()
        self.video_capture = None
        self.current_frame = 0
        self.total_frames = 0
        self.fps = 0
        self.video_preview = None

        super().__init__(parent, globalText.ExtractSubtitles)
        self.globalText = globalText

        self.file_extension = "*.mp4;*.flv;*.mkv;*.avi;*.wmv;*.m2ts;*.ts;*.mov;*.webm"
        self.default_output_suffix = ""
        self.special_filename_mapping = {"生肉.mp4": "原文_OCR.srt"}

        self.logger = Logger("VideocrInterface", "videocr")

    def _generate_output_path(self, input_path):
        from pathlib import Path

        input_path = Path(input_path)
        for special_name, output_name in self.special_filename_mapping.items():
            if input_path.name == special_name:
                return input_path.parent / output_name
        return input_path.parent / "原文.srt"

    def get_input_icon(self):
        return FIF.VIDEO

    def _create_settings_cards(self):
        """创建OCR设置卡片"""
        # 语言设置卡片
        self.languageCard = DictSettingCard(
            configItem=cfg.ocr_lang,
            icon=FIF.LANGUAGE,
            title=self.globalText.RecognitionLanguage,
            content=self.globalText.SSTL,
            options_dict=self._get_ocr_language_options(),
        )
        self.settingsGroup.addSettingCard(self.languageCard)

        # 位置设置卡片
        # self.positionCard = ComboBoxSettingCard(
        #     configItem=cfg.ocr_position,
        #     icon=FIF.LAYOUT,
        #     title="文本对齐",
        #     content="指定字幕的对齐方式",
        #     texts=subtitle_positions_list.keys(),
        # )
        # self.settingsGroup.addSettingCard(self.positionCard)

    def _get_ocr_language_options(self):
        return {
            "ch": self.globalText.ChineseEnglish,
            "chinese_cht": self.globalText.TraditionalChinese,
            "en": self.globalText.English,
            "japan": self.globalText.Japanese,
            "korean": self.globalText.Korean,
            "fr": self.globalText.French,
            "german": self.globalText.German,
            "es": self.globalText.Spanish,
            "pt": self.globalText.Portuguese,
            "it": self.globalText.Italian,
            "ru": self.globalText.Russian,
            "ar": self.globalText.Arabic,
            "nl": self.globalText.Dutch,
            "el": self.globalText.Greek,
            "sv": self.globalText.Swedish,
            "no": self.globalText.Norwegian,
            "da": self.globalText.Danish,
            "fi": self.globalText.Finnish,
            "pl": self.globalText.Polish,
            "cs": self.globalText.Czech,
            "hu": self.globalText.Hungarian,
            "ro": self.globalText.Romanian,
            "bg": self.globalText.Bulgarian,
            "rs_cyrillic": self.globalText.SerbianCyrillic,
            "rs_latin": self.globalText.SerbianLatin,
            "hr": self.globalText.Croatian,
            "sk": self.globalText.Slovak,
            "sl": self.globalText.Slovenian,
            "uk": self.globalText.Ukrainian,
            "be": self.globalText.Belarusian,
            "sq": self.globalText.Albanian,
            "et": self.globalText.Estonian,
            "lv": self.globalText.Latvian,
            "lt": self.globalText.Lithuanian,
            "is": self.globalText.Icelandic,
            "ga": self.globalText.Irish,
            "cy": self.globalText.Welsh,
            "mt": self.globalText.Maltese,
            "hi": self.globalText.Hindi,
            "ur": self.globalText.Urdu,
            "bh": self.globalText.Bengali,
            "ta": self.globalText.Tamil,
            "te": self.globalText.Telugu,
            "mr": self.globalText.Marathi,
            "th": self.globalText.Thai,
            "vi": self.globalText.Vietnamese,
            "id": self.globalText.Indonesian,
            "ms": self.globalText.Malay,
            "tl": self.globalText.Filipino,
            "fa": self.globalText.Persian,
            "tr": self.globalText.Turkish,
            "he": self.globalText.Hebrew,
            "ne": self.globalText.Nepali,
            "si": self.globalText.Sinhala,
            "my": self.globalText.Burmese,
            "km": self.globalText.Khmer,
            "lo": self.globalText.Lao,
            "mn": self.globalText.Mongolian,
            "ug": self.globalText.Uyghur,
            "uz": self.globalText.Uzbek,
            "sw": self.globalText.Swahili,
            "af": self.globalText.Afrikaans,
            "la": self.globalText.Latin,
            "sa": self.globalText.Sanskrit,
            "mi": self.globalText.Maori,
            "abq": self.globalText.Abaza,
            "ady": self.globalText.Adyghe,
            "ang": self.globalText.Angika,
            "ava": self.globalText.Avar,
            "az": self.globalText.Azerbaijani,
            "bho": self.globalText.Bhojpuri,
            "bs": self.globalText.Bosnian,
            "che": self.globalText.Chechen,
            "dar": self.globalText.Dargwa,
            "gom": self.globalText.GoanKonkani,
            "bgc": self.globalText.Haryanvi,
            "inh": self.globalText.Ingush,
            "kbd": self.globalText.Kabardian,
            "ku": self.globalText.Kurdish,
            "lbe": self.globalText.Lak,
            "lez": self.globalText.Lezgi,
            "mah": self.globalText.Magahi,
            "mai": self.globalText.Maithili,
            "sck": self.globalText.Nagpuri,
            "new": self.globalText.Newar,
            "oc": self.globalText.Occitan,
            "pi": self.globalText.Pali,
            "tab": self.globalText.Tabassaran,
            "bal": self.globalText.Balochi,
            "ba": self.globalText.Bashkir,
            "eu": self.globalText.Basque,
            "bua": self.globalText.Buryat,
            "ca": self.globalText.Catalan,
            "gl": self.globalText.Galician,
            "ka": self.globalText.Georgian,
            "xal": self.globalText.Kalmyk,
            "kaa": self.globalText.Karakalpak,
            "kk": self.globalText.Kazakh,
            "kv": self.globalText.Komi,
            "ky": self.globalText.Kyrgyz,
            "lb": self.globalText.Luxembourgish,
            "mk": self.globalText.Macedonian,
            "mhr": self.globalText.MeadowMari,
            "mo": self.globalText.Moldovan,
            "os": self.globalText.Ossetic,
            "qu": self.globalText.Quechua,
            "rm": self.globalText.Romansh,
            "sd": self.globalText.Sindhi,
            "tg": self.globalText.Tajik,
            "tt": self.globalText.Tatar,
            "tyv": self.globalText.Tuva,
            "udm": self.globalText.Udmurt,
            "sah": self.globalText.SakhaYakut,
        }

    def create_preview_card(self):
        """创建视频预览卡片"""
        card = SimpleCardWidget()
        layout = QVBoxLayout(card)

        # 标题
        title_layout = QHBoxLayout()
        title_label = StrongBodyLabel(self.globalText.VideoPreview)
        title_layout.addWidget(title_label)
        title_layout.addStretch()
        layout.addLayout(title_layout)

        # 视频预览组件
        self.video_preview = VideoPreview()
        layout.addWidget(self.video_preview)

        # 进度条和控制
        control_layout = QHBoxLayout()
        self.progress_slider = Slider(Qt.Horizontal)
        self.progress_slider.setEnabled(False)

        self.frame_label = CaptionLabel(self.globalText.Frame)
        self.time_label = CaptionLabel(self.globalText.Time)

        control_layout.addWidget(self.progress_slider, 4)
        control_layout.addWidget(self.frame_label, 1)
        control_layout.addWidget(self.time_label, 1)

        self.log_text = TextEdit()
        self.log_text.setMinimumHeight(200)
        self.log_text.setReadOnly(True)
        self.log_text.setPlaceholderText(self.globalText.PLWBDH)

        hint_label = BodyLabel(self.globalText.ParameterAdjustmentT)
        hint_label.setWordWrap(True)

        layout.addLayout(control_layout)
        layout.addWidget(self.log_text)
        layout.addWidget(hint_label)

        # 连接信号
        self.progress_slider.valueChanged.connect(self._seek_video)
        self.video_preview.isCropChoose.connect(self._start_btn_enabled)

        return card

    def refresh_start_btn(self):
        """刷新开始按钮状态"""
        if self.video_preview.crop_boxes.__len__() >= self.video_preview.max_crop_boxes:
            self.start_btn.setEnabled(True)
        else:
            self.start_btn.setEnabled(False)

    def _create_button_layout(self, main_layout):
        """创建操作按钮区域"""
        super()._create_button_layout(main_layout)

        # 为OCR界面添加清空日志按钮
        self.clear_btn = PushButton(FIF.DELETE, self.globalText.ClearLogs)
        main_layout.itemAt(main_layout.count() - 1).layout().addWidget(self.clear_btn)
        self.clear_btn.clicked.connect(self._clear_log)

    def _connect_signals(self):
        """连接信号槽"""
        super()._connect_signals()
        event_bus.add_video_signal.connect(self.loadVideoFromProject)

    def _start_btn_enabled(self, enabled):
        """设置开始按钮可用性"""
        self.start_btn.setEnabled(enabled)

    def load_file_content(self, file_path):
        """加载视频文件"""
        self._load_video(file_path)

    def _load_video(self, video_path):
        """加载视频文件"""
        try:
            # 释放之前的视频捕获对象
            if self.video_capture:
                self.video_capture.release()

            self.video_capture = cv2.VideoCapture(video_path)
            if not self.video_capture.isOpened():
                raise Exception("无法打开视频文件")

            self.total_frames = int(self.video_capture.get(cv2.CAP_PROP_FRAME_COUNT))
            self.fps = self.video_capture.get(cv2.CAP_PROP_FPS)
            self.current_frame = 0
            self.video_preview.reset_selection_state()

            # 设置进度条
            self.progress_slider.setRange(0, self.total_frames - 1)
            self.progress_slider.setEnabled(True)

            # 显示第一帧
            self._update_video_frame(0)

            # 刷新按钮状态
            self.video_preview.refresh_select_btn()
            self.refresh_start_btn()

            self._log_message(self.globalText.VLS.format(video_path))
            self._log_message(
                self.globalText.TotalFramesFPS2f.format(self.total_frames, self.fps)
            )

        except Exception as e:
            self._log_message(
                self.globalText.FailedToLoadVideo.format(str(e)), is_error=True
            )

    def _update_video_frame(self, frame_number):
        """更新视频帧显示"""
        if self.video_capture and 0 <= frame_number < self.total_frames:
            self.video_capture.set(cv2.CAP_PROP_POS_FRAMES, frame_number)
            ret, frame = self.video_capture.read()

            if ret:
                self.video_preview.set_frame(frame)
                self.current_frame = frame_number

                # 更新帧和时间信息
                self.frame_label.setText(
                    self.globalText.Frame2.format(frame_number + 1, self.total_frames)
                )
                if self.fps > 0:
                    current_time = frame_number / self.fps
                    total_time = self.total_frames / self.fps
                    self.time_label.setText(
                        self.globalText.Time2.format(
                            self._format_time(current_time),
                            self._format_time(total_time),
                        )
                    )

    def _format_time(self, seconds):
        """格式化时间显示"""
        hours = int(seconds // 3600)
        minutes = int((seconds % 3600) // 60)
        seconds = int(seconds % 60)

        if hours > 0:
            return f"{hours:02d}:{minutes:02d}:{seconds:02d}"
        else:
            return f"{minutes:02d}:{seconds:02d}"

    def _seek_video(self, value):
        """跳转到指定帧"""
        if value != self.current_frame:
            self._update_video_frame(value)

    def _start_processing(self):
        """开始OCR处理"""
        # 检测paddleocr.exe是否存在
        paddleocr_path = cfg.get(cfg.paddleocrPath)
        if not os.path.exists(paddleocr_path):
            self.show_error_message(
                self.globalText.PaddleocrExeNotFound.format(paddleocr_path)
            )
            return

        # 检测paddleocr路径内是否有中文
        if re.search("[\u4e00-\u9fff\u3400-\u4dbf]", cfg.get(cfg.paddleocrPath)):
            dialog = Dialog(
                self.globalText.Warning,
                self.globalText.PPMNCCC.format(cfg.get(cfg.paddleocrPath)),
                self.window(),
            )
            dialog.yesButton.setText(self.globalText.OK2)
            dialog.cancelButton.setVisible(False)
            dialog.exec()
            return

        # 检测supportFiles路径是否有中文
        if re.search("[\u4e00-\u9fff\u3400-\u4dbf]", cfg.get(cfg.supportFilesPath)):
            dialog = Dialog(
                self.globalText.Warning,
                self.globalText.SFPMNCCC.format(cfg.get(cfg.supportFilesPath)),
                self.window(),
            )
            dialog.yesButton.setText(self.globalText.OK2)
            dialog.cancelButton.setVisible(False)
            dialog.exec()
            return

        # 检测临时文件夹路径是否有中文
        temp_path = cfg.get(cfg.tempDir)
        if re.search("[\u4e00-\u9fff\u3400-\u4dbf]", temp_path):
            dialog = Dialog(
                self.globalText.Warning,
                self.globalText.TFPMNCCC2.format(temp_path),
                self.window(),
            )
            dialog.yesButton.setText(self.globalText.OK2)
            dialog.cancelButton.setVisible(False)
            dialog.exec()
            return

        if not os.path.exists(cfg.get(cfg.paddleocrPath)):
            self.show_error_message(
                self.globalText.PPDNE.format(cfg.get(cfg.paddleocrPath))
            )
            return

        if not os.path.exists(cfg.get(cfg.supportFilesPath)):
            self.show_error_message(
                self.globalText.SFPDNE.format(cfg.get(cfg.supportFilesPath))
            )
            return

        if not self.file_path:
            self.show_error_message(self.globalText.PSAVFF)
            return

        if not self.outputFileCard.lineEdit.text():
            self.show_error_message(self.globalText.PSTOFP)
            return

        selection_rect = self.video_preview.get_selection_rect()
        if not selection_rect:
            self.show_error_message(self.globalText.PSSAF)
            return
        self._log_message(
            self.globalText.UsingCustomAreaX.format(
                selection_rect.x(),
                selection_rect.y(),
                selection_rect.width(),
                selection_rect.height(),
            )
        )

        # 组合参数发送信号
        args = self._get_args()
        self.addTask.emit(args)
        self.logger.info(
            f"开始OCR处理: -{self.inputFileCard.lineEdit.text()}- 参数: {args}"
        )

    def _get_args(self):
        """获取OCR参数"""
        args = {}
        args["video_path"] = self.inputFileCard.lineEdit.text()
        args["file_path"] = self.outputFileCard.lineEdit.text()
        args["temp_dir"] = cfg.get(cfg.tempDir)
        args["lang"] = cfg.get(cfg.ocr_lang)
        args["time_start"] = cfg.get(cfg.timeStart)
        args["time_end"] = cfg.get(cfg.timeEnd)
        args["sim_threshold"] = cfg.get(cfg.simThreshold)
        args["max_merge_gap_sec"] = cfg.get(cfg.maxMergeGap)
        args["use_fullframe"] = False
        args["use_gpu"] = cfg.get(cfg.useGpu)
        args["use_angle_cls"] = cfg.get(cfg.useAngleCls)
        args["use_server_model"] = cfg.get(cfg.useServerModel)
        # args["brightness_threshold"] = cfg.get(cfg.brightnessThreshold)
        args["ssim_threshold"] = cfg.get(cfg.ssimThreshold)
        args["subtitle_position"] = "center"
        args["frames_to_skip"] = cfg.get(cfg.framesToSkip)
        args["use_dual_zone"] = cfg.get(cfg.useDualZone)

        crop_zones = self.video_preview.crop_boxes
        print(crop_zones)
        args["--crop_x"] = crop_zones[0]["coords"]["crop_x"]
        args["--crop_y"] = crop_zones[0]["coords"]["crop_y"]
        args["--crop_width"] = crop_zones[0]["coords"]["crop_width"]
        args["--crop_height"] = crop_zones[0]["coords"]["crop_height"]
        if args["use_dual_zone"]:
            args["--crop_x2"] = crop_zones[1]["coords"]["crop_x"]
            args["--crop_y2"] = crop_zones[1]["coords"]["crop_y"]
            args["--crop_width2"] = crop_zones[1]["coords"]["crop_width"]
            args["--crop_height2"] = crop_zones[1]["coords"]["crop_height"]

        args["ocr_image_max_width"] = cfg.get(cfg.ocrImageMaxWidth)
        args["post_processing"] = cfg.get(cfg.postProcessing)
        args["min_subtitle_duration_sec"] = cfg.get(cfg.minSubtitleDuration)
        args["confidence_threshold"] = cfg.get(cfg.confidenceThreshold)
        args["paddleocr_path"] = cfg.get(cfg.paddleocrPath)
        args["supportFilesPath"] = cfg.get(cfg.supportFilesPath)

        return args

    def _clear_log(self):
        """清空日志"""
        if hasattr(self, "log_text"):
            self.log_text.clear()

    def _log_message(self, message, is_error=False, is_flush=False):
        """添加日志消息"""
        if not hasattr(self, "log_text"):
            return

        timestamp = QTime.currentTime().toString("hh:mm:ss")
        if is_error:
            formatted_message = f'<font color="red">[{timestamp}] {message}</font>'
        else:
            formatted_message = f"[{timestamp}] {message}"

        if is_flush:
            current_text = self.log_text.toPlainText()
            lines = current_text.split("\n")
            if lines:
                lines.pop()
            self.log_text.setPlainText("\n".join(lines))

        self.log_text.append(formatted_message)
        scrollbar = self.log_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def loadVideoFromProject(self, video_path):
        """从项目加载视频"""
        if video_path:
            self.file_path = video_path
            self.inputFileCard.lineEdit.setText(video_path)
            output_path = self._generate_output_path(video_path)
            self.outputFileCard.lineEdit.setText(str(output_path))
            self.load_file_content(video_path)
            self.video_preview.select_btn.setEnabled(True)
            self.video_preview.refresh_select_btn()
            self.refresh_start_btn()
            self.update_adjacent_button()

    def closeEvent(self, event):
        """关闭事件处理"""
        if self.video_capture:
            self.video_capture.release()
        super().closeEvent(event)
