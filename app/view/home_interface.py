# coding:utf-8
import sys

from PySide6.QtCore import Qt
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QVBoxLayout, QWidget
from qfluentwidgets import FluentIcon as FIF
from qfluentwidgets import ScrollArea

from ..common.event_bus import event_bus
from ..components.info_card import FairyKekkaiWorkshopInfoCard
from ..components.sample_card import SampleCardView
from ..common.text import Text


class HomeInterface(ScrollArea):
    """Home interface"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.view = QWidget(self)
        self.loadProgressInfoBar = None
        self.installProgressInfoBar = None

        self.fairyKekkaiWorkshopInfoCard = FairyKekkaiWorkshopInfoCard(self.view)

        self.vBoxLayout = QVBoxLayout(self.view)

        self._initWidget()
        self.loadSamples()
        self._connectSignalToSlot()

    def _initWidget(self):
        self.setWidget(self.view)
        self.setAcceptDrops(True)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        self.vBoxLayout.setSpacing(10)
        self.vBoxLayout.setContentsMargins(0, 0, 10, 10)
        self.vBoxLayout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.vBoxLayout.addWidget(
            self.fairyKekkaiWorkshopInfoCard, 0, Qt.AlignmentFlag.AlignTop
        )

        self.resize(780, 800)
        self.setObjectName("HomeInterface")
        self.enableTransparentBackground()

    def loadSamples(self):
        """load samples"""
        # basic input samples
        basicInputView = SampleCardView(self.globalText.FeatureOverview, self.view)
        basicInputView.addSampleCard(
            icon=QIcon(":/app/images/controls/project.svg"),
            title=self.globalText.ProjectManagement,
            content=self.globalText.VYTP,
            routeKey="ProjectStackedInterface",
            index=1,
        )
        basicInputView.addSampleCard(
            icon=QIcon(":/app/images/controls/download.svg"),
            title=self.globalText.VideoDownload,
            content=self.globalText.DSYL,
            routeKey="VideocrStackedInterfaces",
            index=2,
        )
        if sys.platform == "win32":
            basicInputView.addSampleCard(
                icon=QIcon(":/app/images/controls/subtitle.svg"),
                title=self.globalText.SubtitleExtraction,
                content=self.globalText.ESUPE,
                routeKey="DownloadStackedInterfaces",
                index=3,
            )
            basicInputView.addSampleCard(
                icon=QIcon(":/app/images/controls/whisper.svg"),
                title=self.globalText.SpeechRecognition,
                content=self.globalText.ESFV,
                routeKey="WhisperStackedInterfaces",
                index=4,
            )
        basicInputView.addSampleCard(
            icon=QIcon(":/app/images/controls/translate.svg"),
            title=self.globalText.TranslateSubtitles,
            content=self.globalText.TESF,
            routeKey="TranslationStackedInterface",
            index=5 if sys.platform == "win32" else 3,
        )
        basicInputView.addSampleCard(
            icon=QIcon(":/app/images/controls/video.svg"),
            title=self.globalText.VideoEncoding,
            content=self.globalText.ETV,
            routeKey="FFmpegStackedInterface",
            index=6 if sys.platform == "win32" else 4,
        )
        basicInputView.addSampleCard(
            icon=QIcon(":/app/images/controls/setting.svg"),
            title=self.globalText.Settings2,
            content=self.globalText.CAP,
            routeKey="settingInterface",
            index=-1,
        )

        # url sameples
        urlSamepleView = SampleCardView(self.globalText.RequiredResources, self.view)
        urlSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/logo/FFmpeg.svg"),
            title=self.globalText.FFmpegBuiltIn,
            content=self.globalText.FDLSPISAD,
            url="https://ffmpeg.org/download.html",
        )
        urlSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/logo/ytdlp.svg"),
            title=self.globalText.YtDlpBuiltIn,
            content=self.globalText.YDDLSPISAD,
            url="https://github.com/yt-dlp/yt-dlp/releases/latest",
        )
        if sys.platform == "win32":
            urlSamepleView.addOpenUrlCard(
                icon=FIF.LIBRARY_FILL,
                title=self.globalText.WMNBI,
                content=self.globalText.WMDLSPISBIMISSFBSR,
                url="https://pan.xunlei.com/s/VOu1R3aOfz05uqcbNUBSnEFSA1?pwd=62cr#",
            )
        # urlSamepleView.addOpenUrlCard(
        #     icon=QIcon(":/app/images/logo/Paddle.svg"),
        #     title="PaddleOCR (已内置)",
        #     content="PaddleOCR下载地址，根据您的硬件下载\n对应版本后设置其路径",
        #     url="https://github.com/timminator/PaddleOCR-Standalone/releases/latest",
        # )

        # urlSamepleView.addOpenUrlCard(
        #     icon=QIcon(":/app/images/logo/Paddle.svg"),
        #     title="PaddleOCRv5.support.files (已内置)",
        #     content="PaddleOCR支持文件下载地址\n下载后设置其路径",
        #     url="https://github.com/timminator/PaddleOCR-Standalone/releases/download/v1.4.0/PaddleOCR.PP-OCRv5.support.files.VideOCR.7z",
        # )

        """
        AI_model_dict = {
        "腾讯混元": "hunyuan-turbos-latest",
        "Deepseek": "deepseek",
        "Gemini 3 Flash": "gemini-3-flash-preview",
        "书生": "intern-latest",
        "GLM-4.5-FLASH": "glm-4.5-flash",
        "Spark-Lite": "spark-lite",
        "百度ERNIE-Speed-128K": "ernie-speed-128k",}
        """
        apiSamepleView = SampleCardView(self.globalText.APIPlatforms, self.view)
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/hunyuan-turbos-latest.svg"),
            title=self.globalText.TencentHunyuan,
            content=self.globalText.THHLAS,
            url="https://console.cloud.tencent.com/hunyuan-turbos",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/deepseek.svg"),
            title="Deepseek",
            content=self.globalText.DeepSeekAPIService,
            url="https://platform.deepseek.com/",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/gemini-3-flash-preview.svg"),
            title="Google Gemini",
            content=self.globalText.GeminiAPIService,
            url="https://aistudio.google.com/app/api-keys",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/intern-latest.svg"),
            title=self.globalText.InternLM,
            content=self.globalText.InternLMAPIService,
            url="https://internlm.intern-ai.org.cn/api",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/glm-4.5-flash.svg"),
            title=self.globalText.ZhipuAI,
            content=self.globalText.GLM45FLASHAPIService,
            url="https://www.bigmodel.cn/",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/spark-lite.svg"),
            title=self.globalText.IFlytekSpark,
            content=self.globalText.SparkLiteAPIService,
            url="https://www.xfyun.cn/",
        )
        apiSamepleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/ernie-speed-128k.svg"),
            title=self.globalText.BaiduQianfan,
            content=self.globalText.ES1AS,
            url="https://cloud.baidu.com/",
        )

        webSampleView = SampleCardView(self.globalText.UsefulWebsites, self.view)
        webSampleView.addOpenUrlCard(
            icon=QIcon(":/app/images/logo/bilibili.svg"),
            title="Bilibili",
            content=self.globalText.BVP,
            url="https://www.bilibili.com/",
        )
        webSampleView.addOpenUrlCard(
            icon=QIcon(":/app/images/logo/youtube.svg"),
            title="YouTube",
            content=self.globalText.YouTubePlatform,
            url="https://www.youtube.com/",
        )
        webSampleView.addOpenUrlCard(
            icon=QIcon(":/app/images/icons/deepseek.svg"),
            title="Deepseek",
            content=self.globalText.DeepSeek,
            url="https://www.deepseek.com/",
        )
        webSampleView.addOpenUrlCard(
            icon=FIF.GITHUB,
            title="GitHub",
            content=self.globalText.GitHubRepository,
            url="https://www.github.com/",
        )

        self.vBoxLayout.addWidget(basicInputView)
        self.vBoxLayout.addWidget(urlSamepleView)
        self.vBoxLayout.addWidget(apiSamepleView)
        self.vBoxLayout.addWidget(webSampleView)

    def _connectSignalToSlot(self):
        # 检查更新
        self.fairyKekkaiWorkshopInfoCard.updateButton.clicked.connect(
            event_bus.checkUpdateSig
        )
