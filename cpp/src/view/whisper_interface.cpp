#include "view/whisper_interface.h"
#include "view/whisper_task_interface.h"
#include "common/app_data.h"
#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/config_card.h"

#include <QFileInfo>
#include <QVBoxLayout>

namespace fkw {
WhisperInterface::WhisperInterface(QWidget* parent)
    : BaseFunctionInterface(trText("识别"), qfw::FluentIconEnum::Microphone, parent) {
    setObjectName(QStringLiteral("whisperInterface"));
    setOutputSuffix(QStringLiteral("_Whisper.srt"));
    setFileFilter(QStringLiteral(
        "*.mp4;*.flv;*.mkv;*.avi;*.wmv;*.mpg;*.mov;*.wav;*.mp3;*.flac"));
    setSpecialFilenameMapping(
        {{QStringLiteral("生肉.mp4"), QStringLiteral("原文_Whisper.srt")}});
    connect(&GlobalEventBus::instance(), &GlobalEventBus::whisper_requested,
            this, [this](const QString& input, const QString& output) {
        emit taskRequested(input, output);
    });
    // 从项目详情加载视频到识别界面（对齐 Python loadVideoFromProject：
    // 回填输入路径并生成输出路径，即基类 setInputPath 行为）
    connect(&GlobalEventBus::instance(), &GlobalEventBus::whisper_video_load_signal,
            this, [this](const QString& videoPath) {
        if (!videoPath.isEmpty()) setInputPath(videoPath);
    });

    const auto& t = Text::instance();
    // 语言 / 输出格式下拉均绑定 config，config 存语言代码与格式标识
    // （labels 对齐 Python languageCard texts，values 对齐 whisperLanguage options）
    boundChoice(this, settingsGroup_, ConfigKeys::whisperLanguage,
                qfw::FluentIconEnum::Language, t.RecognitionLanguage, t.SLTR,
                {t.AutoDetect, t.Chinese, t.Japanese, t.English, t.Korean,
                 t.French, t.German, t.Spanish},
                {QStringLiteral("auto"), QStringLiteral("zh"),
                 QStringLiteral("ja"), QStringLiteral("en"),
                 QStringLiteral("ko"), QStringLiteral("fr"),
                 QStringLiteral("de"), QStringLiteral("es")});
    boundChoice(this, settingsGroup_, ConfigKeys::whisperOutputFormat,
                qfw::FluentIconEnum::Document, t.OutputFormat, t.SSOF,
                {QStringLiteral("srt"), QStringLiteral("txt"),
                 QStringLiteral("vtt")},
                {QStringLiteral("srt"), QStringLiteral("txt"),
                 QStringLiteral("vtt")});

    // 模型说明提示（对齐 Python：加入组外层布局而非卡片布局——
    // ExpandLayout 按控件当前高度排版，多行 wordWrap 文本会被裁断）
    auto* hint = new qfw::BodyLabel(t.BeforeUsingThisFeatu, settingsGroup_);
    hint->setWordWrap(true);
    hint->setOpenExternalLinks(true);
    if (auto* groupLayout = qobject_cast<QVBoxLayout*>(settingsGroup_->layout())) {
        groupLayout->insertSpacing(-1, 8);
        groupLayout->addWidget(hint);
        groupLayout->insertSpacing(-1, 12);
    }
}

bool WhisperInterface::validateBeforeStart(QString* errorMessage) {
    // 对齐 Python _start_processing：CLI 与模型路径存在性校验
    const auto& cfg = AppConfig::instance();
    const auto& t = Text::instance();
    const QString cliPath = cfg.value(ConfigKeys::whisperCliPath).toString();
    if (cliPath.isEmpty() || !QFileInfo::exists(cliPath)) {
        *errorMessage = formatText(t.WCPDNE, {cliPath});
        return false;
    }
    const QString modelPath = cfg.value(ConfigKeys::whisperModelPath).toString();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        *errorMessage = formatText(t.WMPDNE, {modelPath});
        return false;
    }
    return true;
}

WhisperStackedInterfaces::WhisperStackedInterfaces(QWidget* parent)
    : BaseStackedInterfaces(parent) {
    setObjectName(QStringLiteral("WhisperStackedInterfaces"));
    auto* main = new WhisperInterface(this);
    auto* tasks = new WhisperTaskInterface(this);
    addSubInterface(main, QStringLiteral("mainInterface"), trText("语音识别"));
    addSubInterface(tasks, QStringLiteral("taskInterface"),
                    joinTranslatedLabel(trText("语音识别"), trText("任务")));
    addSubInterface(new WhisperSettingInterface(this), QStringLiteral("settingInterface"),
                    trText("高级设置"));
    connect(main, &BaseFunctionInterface::taskRequested, tasks,
            [tasks](const QString& input, const QString& output) {
        tasks->addTask(input, output);
    });
    connect(tasks, &BaseTaskInterface::returnTask, main,
            [main](bool duplicated, const QStringList& paths, bool notify) {
        main->updateTask(duplicated, paths, notify);
    });
}
}
