from PySide6.QtWidgets import (
    QHBoxLayout,
    QTableWidgetItem,
    QVBoxLayout,
)
from qfluentwidgets import (
    BodyLabel,
    CardWidget,
    ComboBoxSettingCard,
    SwitchSettingCard,
    TableWidget,
)
from qfluentwidgets import FluentIcon as FIF

from ..common.config import cfg
from ..common.event_bus import event_bus
from ..common.logger import Logger
from ..common.setting import translate_language_dict
from ..common.text import Text
from ..components.base_function_interface import BaseFunctionInterface
from ..components.base_stacked_interface import BaseStackedInterfaces
from ..components.config_card import DictSettingCard, TranslateSettingInterface
from ..service.srt_service import Srt


class TranslateStackedInterfaces(BaseStackedInterfaces):
    """翻译堆叠界面"""

    def __init__(self, parent=None):
        globalText = Text()
        from ..view.translate_task_interface import TranslateTaskInterface

        super().__init__(
            parent=parent,
            main_interface_class=TranslationInterface,
            task_interface_class=TranslateTaskInterface,
            setting_interface_class=TranslateSettingInterface,
            interface_name=globalText.TranslateSubtitles,
        )
        self.globalText = globalText

        # 连接专用信号
        self.mainInterface.addTask.connect(self.taskInterface.addTranslateTask)
        self.taskInterface.returnTranslateTask.connect(self.mainInterface.updateTask)


class TranslationInterface(BaseFunctionInterface):
    """SRT文件翻译界面"""

    def __init__(self, parent=None):
        globalText = Text()
        self.file_srt = None
        super().__init__(parent, globalText.Translate)
        self.globalText = globalText

        self.file_extension = "*.srt"
        self.default_output_suffix = "_translated.srt"
        self.special_filename_mapping = {"原文.srt": "译文.srt"}

        self.logger = Logger("TranslateInterface", "translate")

    def get_input_icon(self):
        return FIF.CALENDAR

    def _create_settings_cards(self):
        """创建翻译设置卡片"""
        # 原语言设置卡片
        self.origin_languageCard = DictSettingCard(
            configItem=cfg.origin_lang,
            icon=FIF.GLOBE,
            title=self.globalText.SourceLanguage,
            content=self.globalText.SSTL,
            options_dict=self._get_translate_language_options(),
        )
        self.settingsGroup.addSettingCard(self.origin_languageCard)

        # 翻译语言设置卡片
        self.target_languageCard = DictSettingCard(
            configItem=cfg.target_lang,
            icon=FIF.LANGUAGE,
            title=self.globalText.TargetLanguage,
            content=self.globalText.SelectTargetLanguage,
            options_dict=self._get_translate_language_options(),
        )
        self.settingsGroup.addSettingCard(self.target_languageCard)

        self.AI_modelCard = DictSettingCard(
            configItem=cfg.ai_model,
            icon=FIF.BOOK_SHELF,
            title=self.globalText.AIModel,
            content=self.globalText.SelectAIModel,
            options_dict=self._get_ai_model_options(),
        )
        self.settingsGroup.addSettingCard(self.AI_modelCard)

        # 上下文关联开关
        self.useTranslateContextCard = SwitchSettingCard(
            FIF.UNIT,
            self.globalText.EnableContext,
            self.globalText.WEATRSSWDEBTI,
            configItem=cfg.useTranslateContext,
        )
        self.settingsGroup.addSettingCard(self.useTranslateContextCard)

        # Deepseek 专属设置 - 模型选择
        self.deepseekModelCard = ComboBoxSettingCard(
            configItem=cfg.deepseekModel,
            icon=FIF.ROBOT,
            title=self.globalText.DeepseekModel,
            content=self.globalText.SDMV,
            texts=["deepseek-v4-flash", "deepseek-v4-pro"],
        )
        self.settingsGroup.addSettingCard(self.deepseekModelCard)

        # Deepseek 专属设置 - 深度思考模式
        self.deepseekReasoningCard = SwitchSettingCard(
            icon=FIF.IOT,
            title=self.globalText.DeepThinking,
            content=self.globalText.EDDTM,
            configItem=cfg.deepseekReasoning,
        )
        self.settingsGroup.addSettingCard(self.deepseekReasoningCard)

        cfg.ai_model.valueChanged.connect(self._onAIModelChanged)
        self._onAIModelChanged(cfg.get(cfg.ai_model))

    def _get_translate_language_options(self):
        return {
            "en": self.globalText.English,
            "zh": self.globalText.Chinese,
            "ja": self.globalText.Japanese,
            "ko": self.globalText.Korean,
            "fr": self.globalText.French,
            "de": self.globalText.German,
            "es": self.globalText.Spanish,
            "pt": self.globalText.Portuguese,
            "ru": self.globalText.Russian,
            "ar": self.globalText.Arabic,
            "it": self.globalText.Italian,
            "nl": self.globalText.Dutch,
            "hi": self.globalText.Hindi,
            "tr": self.globalText.Turkish,
            "vi": self.globalText.Vietnamese,
            "th": self.globalText.Thai,
            "id": self.globalText.Indonesian2,
            "sv": self.globalText.Swedish,
            "pl": self.globalText.Polish,
            "el": self.globalText.Greek,
            "cs": self.globalText.Czech,
            "da": self.globalText.Danish,
            "fi": self.globalText.Finnish,
            "no": self.globalText.Norwegian,
            "hu": self.globalText.Hungarian,
            "ro": self.globalText.Romanian,
            "uk": self.globalText.Ukrainian,
            "fa": self.globalText.Persian,
            "he": self.globalText.Hebrew,
        }

    def _get_ai_model_options(self):
        return {
            "hunyuan-turbos-latest": self.globalText.TencentHunyuan,
            "deepseek": self.globalText.Deepseek,
            "gemini-3.5-flash": self.globalText.Gemini3Flash,
            "intern-latest": self.globalText.InternLM,
            "glm-4.5-flash": self.globalText.GLM45FLASH,
            "spark-lite": self.globalText.SparkLite,
            "ernie-speed-128k": self.globalText.BaiduERNIESpeed128K,
            "custom-model": self.globalText.CustomModel,
        }

    def _onAIModelChanged(self, model_name: str):
        """根据选择的AI模型动态显示/隐藏专属设置"""
        # Deepseek 专属设置
        is_deepseek = model_name == "deepseek"
        self.deepseekModelCard.setVisible(is_deepseek)
        self.deepseekReasoningCard.setVisible(is_deepseek)

    def create_preview_card(self):
        """创建内容预览卡片"""
        card = CardWidget()
        card_layout = QVBoxLayout(card)
        card_layout.setSpacing(10)
        card_layout.setContentsMargins(20, 20, 20, 20)

        # 标题和统计信息
        preview_header = QHBoxLayout()

        title = BodyLabel(self.globalText.SRTContentPreview)
        title.setStyleSheet("font-weight: bold; font-size: 14px;")

        self.stats_label = BodyLabel(self.globalText.Text0SubtitlesTotal)
        self.stats_label.setStyleSheet("color: #666;")

        preview_header.addWidget(title)
        preview_header.addStretch(1)
        preview_header.addWidget(self.stats_label)

        card_layout.addLayout(preview_header)

        # 表格
        self.preview_table = TableWidget()
        self.preview_table.setColumnCount(3)
        self.preview_table.setHorizontalHeaderLabels(
            [self.globalText.No, self.globalText.Timeline, self.globalText.Content]
        )
        self.preview_table.horizontalHeader().setStretchLastSection(True)
        self.preview_table.setEditTriggers(TableWidget.NoEditTriggers)
        self.preview_table.setAlternatingRowColors(True)
        self.preview_table.setMinimumHeight(300)

        card_layout.addWidget(self.preview_table)

        return card

    def _connect_signals(self):
        """连接信号槽"""
        super()._connect_signals()
        event_bus.translate_requested.connect(self.addTranslateFromProject)

    def load_file_content(self, file_path):
        """加载SRT文件内容"""
        self.file_srt = Srt(file_path)
        self.update_preview_table(self.file_srt.subtitle_data)

    def _start_processing(self):
        """开始翻译"""
        if cfg.get(cfg.ai_model) == "deepseek" and not cfg.get(cfg.deepseekApiKey):
            self.show_error_message(self.globalText.PFIYDAKF)
            return

        elif cfg.get(cfg.ai_model) == "glm-4.5-flash" and not cfg.get(cfg.glmApiKey):
            self.show_error_message(self.globalText.PFIYG45FAKF)
            return

        elif cfg.get(cfg.ai_model) == "spark-lite" and not cfg.get(cfg.sparkApiKey):
            self.show_error_message(self.globalText.PFIYSLAKF)
            return

        elif cfg.get(cfg.ai_model) == "hunyuan-turbos-latest" and not cfg.get(
            cfg.hunyuanApiKey
        ):
            self.show_error_message(self.globalText.PFIYTHAKF)
            return

        elif cfg.get(cfg.ai_model) == "intern-latest" and not cfg.get(cfg.internApiKey):
            self.show_error_message(self.globalText.PFIYIAKF)
            return

        elif cfg.get(cfg.ai_model) == "ernie-speed-128k" and not cfg.get(
            cfg.ernieSpeedApiKey
        ):
            self.show_error_message(self.globalText.PFIYBES1AKF)
            return

        elif cfg.get(cfg.ai_model) == "gemini-3.5-flash" and not cfg.get(
            cfg.geminiApiKey
        ):
            self.show_error_message(self.globalText.PFIYG3FAKF)
            return

        elif cfg.get(cfg.ai_model) == "custom-model":
            if not cfg.get(cfg.customModelEnabled):
                self.show_error_message(self.globalText.PECMISF)
                return
            if not cfg.get(cfg.customModelApiKey):
                self.show_error_message(self.globalText.PFIYCMAKF)
                return
            if not cfg.get(cfg.customModelBaseUrl):
                self.show_error_message(self.globalText.PFIYCMABUF)
                return
            if not cfg.get(cfg.customModelName):
                self.show_error_message(self.globalText.PFIYCMNF)
                return

        if cfg.get(cfg.origin_lang) == cfg.get(cfg.target_lang):
            self.show_error_message(self.globalText.SATLATS)
            return

        args = self._get_args()
        self.addTask.emit(args)
        self.logger.info(
            f"开始翻译字幕: -{self.inputFileCard.lineEdit.text()}- 参数: {args}"
        )

    def _get_args(self):
        """获取翻译参数"""
        args = {}
        args["srt_path"] = str(self.file_srt.file_path)
        args["output_path"] = self.outputFileCard.lineEdit.text()
        origin_lang = cfg.get(cfg.origin_lang)
        target_lang = cfg.get(cfg.target_lang)
        args["origin_lang"] = translate_language_dict.get(origin_lang, origin_lang)
        args["target_lang"] = translate_language_dict.get(target_lang, target_lang)
        args["raw_content"] = self.file_srt.raw_content
        args["AI"] = cfg.get(cfg.ai_model)
        args["temperature"] = float(cfg.get(cfg.aiTemperature))

        # Deepseek 专属参数
        if cfg.get(cfg.ai_model) == "deepseek":
            args["deepseek_model"] = cfg.get(cfg.deepseekModel)
            args["deepseek_reasoning"] = cfg.get(cfg.deepseekReasoning)

        return args

    def update_preview_table(self, subtitles_data):
        """更新预览表格内容"""
        self.preview_table.setRowCount(0)  # 清空表格

        for row, (index, timestamp, text) in enumerate(subtitles_data):
            self.preview_table.insertRow(row)
            self.preview_table.setItem(row, 0, QTableWidgetItem(str(index)))
            self.preview_table.setItem(row, 1, QTableWidgetItem(timestamp))
            self.preview_table.setItem(
                row, 2, QTableWidgetItem(text.replace("\n", "\\n"))
            )

        # 调整列宽
        self.preview_table.resizeColumnsToContents()
        self.stats_label.setText(
            self.globalText.SubtitlesTotal.format(len(subtitles_data))
        )

    def addTranslateFromProject(self, file_path, output_path):
        """从项目界面添加翻译任务"""
        srt_file = Srt(file_path)
        self.file_srt = srt_file

        args = {}
        args["srt_path"] = file_path
        args["output_path"] = output_path
        origin_lang = cfg.get(cfg.origin_lang)
        target_lang = cfg.get(cfg.target_lang)
        args["origin_lang"] = translate_language_dict.get(origin_lang, origin_lang)
        args["target_lang"] = translate_language_dict.get(target_lang, target_lang)
        args["raw_content"] = srt_file.raw_content
        args["AI"] = cfg.get(cfg.ai_model)

        self.addTask.emit(args)
