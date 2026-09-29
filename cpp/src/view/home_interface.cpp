#include "view/home_interface.h"

#include <QIcon>
#include <QVBoxLayout>
#include <tuple>

#include "common/app_data.h"
#include "common/event_bus.h"
#include "components/floating_window.h"
#include "components/info_card.h"
#include "components/sample_card.h"

namespace fkw
{
    HomeInterface::HomeInterface(QWidget *parent) : qfw::ScrollArea(parent)
    {
        setObjectName(QStringLiteral("HomeInterface"));
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *view = new QWidget(this);
        auto *layout = new QVBoxLayout(view);
        layout->setSpacing(10);
        layout->setContentsMargins(0, 0, 10, 10);
        layout->setAlignment(Qt::AlignTop);
        setWidget(view);
        enableTransparentBackground();
        auto *info = new FairyKekkaiWorkshopInfoCard(view);
        layout->addWidget(info, 0, Qt::AlignTop);
        connect(info, &FairyKekkaiWorkshopInfoCard::logRequested,
                this, &HomeInterface::logRequested);
        connect(info, &FairyKekkaiWorkshopInfoCard::restartRequested,
                this, &HomeInterface::restartRequested);
        connect(info, &FairyKekkaiWorkshopInfoCard::floatingWindowRequested, this, []() {
            // 对齐 Python info_card.__onFloatingButtonClicked：每次新建一个无父窗口的
            // 置顶悬浮窗；窗口带 WA_DeleteOnClose，关闭后由 ocr_window_closed 恢复入口按钮。
            auto *floating = new FloatingWindow();
            floating->show();
            floating->raise();
            floating->activateWindow();
        });
        connect(info, &FairyKekkaiWorkshopInfoCard::updateRequested,
                &GlobalEventBus::instance(), &GlobalEventBus::checkUpdateSig);
        auto *features = new SampleCardView(trText("功能一览"), view);
        const struct
        {
            const char *icon;
            const char *title;
            const char *text;
            const char *route;
        } cards[] = {
            {":/app/images/controls/project.svg", "项目管理", "查看您的烤肉项目", "projects"},
            {":/app/images/controls/download.svg", "视频下载", "下载您相中的系列", "download"},
            {":/app/images/controls/subtitle.svg", "字幕提取", "使用PaddleOCR引擎提取字幕", "ocr"},
            {":/app/images/controls/whisper.svg", "语音识别", "提取视频内的人声", "whisper"},
            {":/app/images/controls/translate.svg", "翻译字幕", "翻译提取出的字幕文件", "translate"},
            {":/app/images/controls/video.svg", "视频压制", "压制烤制好的视频", "ffmpeg"},
            {":/app/images/controls/setting.svg", "软件设置", "设置软件的各项参数", "settings"},
        };
        for (const auto &item : cards)
            features->addSampleCard(QIcon(QString::fromUtf8(item.icon)), trText(item.title),
                                    trText(item.text), QString::fromUtf8(item.route));
        connect(features, &SampleCardView::routeRequested,
                this, &HomeInterface::routeRequested);
        layout->addWidget(features);

        auto *resources = new SampleCardView(trText("必要资源"), view);
        resources->addOpenUrlCard(QIcon(QStringLiteral(":/app/images/logo/FFmpeg.svg")),
                                  trText("FFmpeg (已内置)"),
                                  trText("FFmpeg下载地址，下载后可在设置里设定路径"),
                                  QUrl(QStringLiteral("https://ffmpeg.org/download.html")));
        resources->addOpenUrlCard(QIcon(QStringLiteral(":/app/images/logo/ytdlp.svg")),
                                  trText("yt-dlp (已内置)"),
                                  trText("yt-dlp下载地址，下载后可在设置里设定路径"),
                                  QUrl(QStringLiteral("https://github.com/yt-dlp/yt-dlp/releases/latest")));
        resources->addOpenUrlCard(qfw::FluentIcon(qfw::FluentIconEnum::LibraryFill).qicon(),
                                  trText("Whisper模型 (未内置)"),
                                  trText("Whisper模型下载地址，下载后可在设置里设定路径，目前软件内置的模型为small，对于油库里语音识别效果足够"),
                                  QUrl(QStringLiteral("https://pan.xunlei.com/s/VOu1R3aOfz05uqcbNUBSnEFSA1?pwd=62cr#")));
        layout->addWidget(resources);

        auto *api = new SampleCardView(trText("API平台"), view);
        for (const auto &item : {
                 std::tuple<const char *, const char *, const char *, const char *>{
                     ":/app/images/icons/hunyuan-turbos-latest.svg", "腾讯混元",
                     "腾讯混元(hunyuan-lite)API服务", "https://console.cloud.tencent.com/hunyuan-turbos"},
                 {":/app/images/icons/deepseek.svg", "Deepseek", "深度求索API服务",
                  "https://platform.deepseek.com/"},
                 {":/app/images/icons/gemini-3.5-flash.svg", "Google Gemini", "Gemini API服务",
                  "https://aistudio.google.com/app/api-keys"},
                 {":/app/images/icons/intern-latest.svg", "书生", "书生API服务",
                  "https://internlm.intern-ai.org.cn/api"},
                 {":/app/images/icons/glm-4.5-flash.svg", "智谱 AI", "GLM-4.5-FLASH API服务",
                  "https://www.bigmodel.cn/"},
                 {":/app/images/icons/spark-lite.svg", "讯飞星火", "Spark-Lite API服务",
                  "https://www.xfyun.cn/"},
                 {":/app/images/icons/ernie-speed-128k.svg", "百度千帆",
                  "ERNIE-Speed-128K API服务", "https://cloud.baidu.com/"}})
        {
            api->addOpenUrlCard(QIcon(QString::fromUtf8(std::get<0>(item))),
                                trText(std::get<1>(item)), trText(std::get<2>(item)),
                                QUrl(QString::fromUtf8(std::get<3>(item))));
        }
        layout->addWidget(api);
        auto *websites = new SampleCardView(trText("常用网站"), view);
        websites->addOpenUrlCard(QIcon(QStringLiteral(":/app/images/logo/bilibili.svg")),
                                 QStringLiteral("Bilibili"), trText("哔哩哔哩视频平台"),
                                 QUrl(QStringLiteral("https://www.bilibili.com/")));
        websites->addOpenUrlCard(QIcon(QStringLiteral(":/app/images/logo/youtube.svg")),
                                 QStringLiteral("YouTube"), trText("油管视频平台"),
                                 QUrl(QStringLiteral("https://www.youtube.com/")));
        websites->addOpenUrlCard(QIcon(QStringLiteral(":/app/images/icons/deepseek.svg")),
                                 QStringLiteral("Deepseek"), trText("Deepseek"),
                                 QUrl(QStringLiteral("https://www.deepseek.com/")));
        websites->addOpenUrlCard(qfw::FluentIcon(qfw::FluentIconEnum::Github).qicon(),
                                 QStringLiteral("GitHub"), trText("GitHub代码仓库"),
                                 QUrl(QStringLiteral("https://www.github.com/")));
        layout->addWidget(websites);
    }
}
