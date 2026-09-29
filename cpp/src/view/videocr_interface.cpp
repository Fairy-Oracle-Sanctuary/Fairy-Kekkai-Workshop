#include "view/videocr_interface.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"
#include "service/video_frame_service.h"
#include "service/video_preview.h"
#include "view/videocr_task_interface.h"

namespace fkw {
namespace {
QString formatVideoTime(double seconds) {
    const int value = qMax(0, static_cast<int>(seconds));
    const int hours = value / 3600;
    const int minutes = (value / 60) % 60;
    const int rest = value % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3").arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(rest, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(rest, 2, 10, QLatin1Char('0'));
}

// 路径中是否含中日韩统一表意文字（对齐 Python re.search("[\u4e00-\u9fff\u3400-\u4dbf]")）
bool containsChinese(const QString& path) {
    for (const QChar ch : path) {
        const ushort code = ch.unicode();
        if ((code >= 0x4e00 && code <= 0x9fff) ||
            (code >= 0x3400 && code <= 0x4dbf))
            return true;
    }
    return false;
}

// 仅有一个“确定”按钮的警告弹窗（对齐 Python Dialog(...) + yesButton=OK2 + 隐藏取消）
void showWarningDialog(QWidget* parent, const QString& content) {
    qfw::MessageDialog dialog(Text::instance().Warning, content, parent);
    dialog.yesButton->setText(Text::instance().OK2);
    dialog.cancelButton->hide();
    dialog.exec();
}
}
VideocrInterface::VideocrInterface(QWidget* parent)
    : BaseFunctionInterface(trText("提取字幕"), qfw::FluentIconEnum::Video, parent) {
    setObjectName(QStringLiteral("videocrInterface"));
    setOutputSuffix(QStringLiteral(".srt"));
    setFileFilter(QStringLiteral(
        "*.mp4;*.flv;*.mkv;*.avi;*.wmv;*.m2ts;*.ts;*.mov;*.webm"));
    setSpecialFilenameMapping(
        {{QStringLiteral("生肉.mp4"), QStringLiteral("原文_OCR.srt")}});
    const QStringList languageCodes = {
        QStringLiteral("ch"),
        QStringLiteral("chinese_cht"),
        QStringLiteral("en"),
        QStringLiteral("japan"),
        QStringLiteral("korean"),
        QStringLiteral("fr"),
        QStringLiteral("german"),
        QStringLiteral("es"),
        QStringLiteral("pt"),
        QStringLiteral("it"),
        QStringLiteral("ru"),
        QStringLiteral("ar"),
        QStringLiteral("nl"),
        QStringLiteral("el"),
        QStringLiteral("sv"),
        QStringLiteral("no"),
        QStringLiteral("da"),
        QStringLiteral("fi"),
        QStringLiteral("pl"),
        QStringLiteral("cs"),
        QStringLiteral("hu"),
        QStringLiteral("ro"),
        QStringLiteral("bg"),
        QStringLiteral("rs_cyrillic"),
        QStringLiteral("rs_latin"),
        QStringLiteral("hr"),
        QStringLiteral("sk"),
        QStringLiteral("sl"),
        QStringLiteral("uk"),
        QStringLiteral("be"),
        QStringLiteral("sq"),
        QStringLiteral("et"),
        QStringLiteral("lv"),
        QStringLiteral("lt"),
        QStringLiteral("is"),
        QStringLiteral("ga"),
        QStringLiteral("cy"),
        QStringLiteral("mt"),
        QStringLiteral("hi"),
        QStringLiteral("ur"),
        QStringLiteral("bh"),
        QStringLiteral("ta"),
        QStringLiteral("te"),
        QStringLiteral("mr"),
        QStringLiteral("th"),
        QStringLiteral("vi"),
        QStringLiteral("id"),
        QStringLiteral("ms"),
        QStringLiteral("tl"),
        QStringLiteral("fa"),
        QStringLiteral("tr"),
        QStringLiteral("he"),
        QStringLiteral("ne"),
        QStringLiteral("si"),
        QStringLiteral("my"),
        QStringLiteral("km"),
        QStringLiteral("lo"),
        QStringLiteral("mn"),
        QStringLiteral("ug"),
        QStringLiteral("uz"),
        QStringLiteral("sw"),
        QStringLiteral("af"),
        QStringLiteral("la"),
        QStringLiteral("sa"),
        QStringLiteral("mi"),
        QStringLiteral("abq"),
        QStringLiteral("ady"),
        QStringLiteral("ang"),
        QStringLiteral("ava"),
        QStringLiteral("az"),
        QStringLiteral("bho"),
        QStringLiteral("bs"),
        QStringLiteral("che"),
        QStringLiteral("dar"),
        QStringLiteral("gom"),
        QStringLiteral("bgc"),
        QStringLiteral("inh"),
        QStringLiteral("kbd"),
        QStringLiteral("ku"),
        QStringLiteral("lbe"),
        QStringLiteral("lez"),
        QStringLiteral("mah"),
        QStringLiteral("mai"),
        QStringLiteral("sck"),
        QStringLiteral("new"),
        QStringLiteral("oc"),
        QStringLiteral("pi"),
        QStringLiteral("tab"),
        QStringLiteral("bal"),
        QStringLiteral("ba"),
        QStringLiteral("eu"),
        QStringLiteral("bua"),
        QStringLiteral("ca"),
        QStringLiteral("gl"),
        QStringLiteral("ka"),
        QStringLiteral("xal"),
        QStringLiteral("kaa"),
        QStringLiteral("kk"),
        QStringLiteral("kv"),
        QStringLiteral("ky"),
        QStringLiteral("lb"),
        QStringLiteral("mk"),
        QStringLiteral("mhr"),
        QStringLiteral("mo"),
        QStringLiteral("os"),
        QStringLiteral("qu"),
        QStringLiteral("rm"),
        QStringLiteral("sd"),
        QStringLiteral("tg"),
        QStringLiteral("tt"),
        QStringLiteral("tyv"),
        QStringLiteral("udm"),
        QStringLiteral("sah"),
    };
    auto* languageCard = new DictSettingCard(qfw::FluentIconEnum::Language,
        Text::instance().RecognitionLanguage, Text::instance().SSTL, {
            Text::instance().ChineseEnglish,
            Text::instance().TraditionalChinese,
            Text::instance().English,
            Text::instance().Japanese,
            Text::instance().Korean,
            Text::instance().French,
            Text::instance().German,
            Text::instance().Spanish,
            Text::instance().Portuguese,
            Text::instance().Italian,
            Text::instance().Russian,
            Text::instance().Arabic,
            Text::instance().Dutch,
            Text::instance().Greek,
            Text::instance().Swedish,
            Text::instance().Norwegian,
            Text::instance().Danish,
            Text::instance().Finnish,
            Text::instance().Polish,
            Text::instance().Czech,
            Text::instance().Hungarian,
            Text::instance().Romanian,
            Text::instance().Bulgarian,
            Text::instance().SerbianCyrillic,
            Text::instance().SerbianLatin,
            Text::instance().Croatian,
            Text::instance().Slovak,
            Text::instance().Slovenian,
            Text::instance().Ukrainian,
            Text::instance().Belarusian,
            Text::instance().Albanian,
            Text::instance().Estonian,
            Text::instance().Latvian,
            Text::instance().Lithuanian,
            Text::instance().Icelandic,
            Text::instance().Irish,
            Text::instance().Welsh,
            Text::instance().Maltese,
            Text::instance().Hindi,
            Text::instance().Urdu,
            Text::instance().Bengali,
            Text::instance().Tamil,
            Text::instance().Telugu,
            Text::instance().Marathi,
            Text::instance().Thai,
            Text::instance().Vietnamese,
            Text::instance().Indonesian,
            Text::instance().Malay,
            Text::instance().Filipino,
            Text::instance().Persian,
            Text::instance().Turkish,
            Text::instance().Hebrew,
            Text::instance().Nepali,
            Text::instance().Sinhala,
            Text::instance().Burmese,
            Text::instance().Khmer,
            Text::instance().Lao,
            Text::instance().Mongolian,
            Text::instance().Uyghur,
            Text::instance().Uzbek,
            Text::instance().Swahili,
            Text::instance().Afrikaans,
            Text::instance().Latin,
            Text::instance().Sanskrit,
            Text::instance().Maori,
            Text::instance().Abaza,
            Text::instance().Adyghe,
            Text::instance().Angika,
            Text::instance().Avar,
            Text::instance().Azerbaijani,
            Text::instance().Bhojpuri,
            Text::instance().Bosnian,
            Text::instance().Chechen,
            Text::instance().Dargwa,
            Text::instance().GoanKonkani,
            Text::instance().Haryanvi,
            Text::instance().Ingush,
            Text::instance().Kabardian,
            Text::instance().Kurdish,
            Text::instance().Lak,
            Text::instance().Lezgi,
            Text::instance().Magahi,
            Text::instance().Maithili,
            Text::instance().Nagpuri,
            Text::instance().Newar,
            Text::instance().Occitan,
            Text::instance().Pali,
            Text::instance().Tabassaran,
            Text::instance().Balochi,
            Text::instance().Bashkir,
            Text::instance().Basque,
            Text::instance().Buryat,
            Text::instance().Catalan,
            Text::instance().Galician,
            Text::instance().Georgian,
            Text::instance().Kalmyk,
            Text::instance().Karakalpak,
            Text::instance().Kazakh,
            Text::instance().Komi,
            Text::instance().Kyrgyz,
            Text::instance().Luxembourgish,
            Text::instance().Macedonian,
            Text::instance().MeadowMari,
            Text::instance().Moldovan,
            Text::instance().Ossetic,
            Text::instance().Quechua,
            Text::instance().Romansh,
            Text::instance().Sindhi,
            Text::instance().Tajik,
            Text::instance().Tatar,
            Text::instance().Tuva,
            Text::instance().Udmurt,
            Text::instance().SakhaYakut,
        }, settingsGroup_);
    settingsGroup_->addSettingCard(languageCard);
    const int selectedLanguage = languageCodes.indexOf(
        AppConfig::instance().value(ConfigKeys::ocr_lang, QStringLiteral("ch")).toString());
    languageCard->comboBox->setCurrentIndex(std::max(0, selectedLanguage));
    connect(languageCard->comboBox, &qfw::ComboBox::currentIndexChanged, this,
            [languageCodes](int index) {
        if (index >= 0 && index < languageCodes.size())
            AppConfig::instance().set(ConfigKeys::ocr_lang, languageCodes[index]);
    });
    connect(&AppConfig::instance(), &AppConfig::valueChanged, languageCard,
            [languageCard, languageCodes](const QString& group, const QString& key, const QJsonValue& value) {
        if (group != QLatin1String(ConfigKeys::ocr_lang.group)
            || key != QLatin1String(ConfigKeys::ocr_lang.name)) return;
        const int index = languageCodes.indexOf(value.toString());
        if (index >= 0 && index != languageCard->comboBox->currentIndex())
            languageCard->comboBox->setCurrentIndex(index);
    });
    auto* card = new qfw::SimpleCardWidget(this);
    auto* layout = new QVBoxLayout(card);
    auto* heading = new qfw::StrongBodyLabel(Text::instance().VideoPreview, card);
    layout->addWidget(heading);
    auto* preview = new VideoPreview(card);
    preview_ = preview;
    preview_->setMaxCropBoxes(AppConfig::instance().value(ConfigKeys::useDualZone).toBool() ? 2 : 1);
    connect(&AppConfig::instance(), &AppConfig::valueChanged, this,
            [preview](const QString& group, const QString& key, const QJsonValue& value) {
        if (group == QLatin1String(ConfigKeys::useDualZone.group)
            && key == QLatin1String(ConfigKeys::useDualZone.name))
            preview->setMaxCropBoxes(value.toBool() ? 2 : 1);
    });
    layout->addWidget(preview);
    auto* controls = new QHBoxLayout();
    auto* slider = new qfw::Slider(Qt::Horizontal, card);
    slider->setEnabled(false);
    controls->addWidget(slider, 4);
    auto* frameLabel = new qfw::CaptionLabel(Text::instance().Frame, card);
    auto* timeLabel = new qfw::CaptionLabel(Text::instance().Time, card);
    controls->addWidget(frameLabel, 1);
    controls->addWidget(timeLabel, 1);
    layout->addLayout(controls);
    // 日志框（对齐 Python VideocrInterface.log_text）：富文本以支持错误红字
    logText_ = new qfw::TextBrowser(card);
    logText_->setReadOnly(true);
    logText_->setMinimumHeight(200);
    logText_->setPlaceholderText(Text::instance().PLWBDH);
    layout->addWidget(logText_);
    auto* hint = new qfw::BodyLabel(Text::instance().ParameterAdjustmentT, card);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    insertPreview(card);
    auto* clearLogs = new qfw::PushButton(
        qfw::FluentIcon(qfw::FluentIconEnum::Delete).qicon(),
        Text::instance().ClearLogs, this);
    mainLayout_->itemAt(mainLayout_->count() - 2)->layout()->addWidget(clearLogs);
    connect(clearLogs, &QPushButton::clicked, this, &VideocrInterface::clearLog);
    auto* frames = new VideoFrameService(this);
    auto* pathDelay = new QTimer(this);
    pathDelay->setSingleShot(true);
    pathDelay->setInterval(250);
    auto* seekDelay = new QTimer(this);
    seekDelay->setSingleShot(true);
    seekDelay->setInterval(120);

    connect(inputFileCard_->lineEdit, &QLineEdit::textChanged, this,
            [this, frames, preview, slider, frameLabel, timeLabel, pathDelay, seekDelay](const QString&) {
        pathDelay->stop();
        seekDelay->stop();
        frames->open({});
        preview->reset();
        const QSignalBlocker blocker(slider);
        slider->setValue(0);
        slider->setEnabled(false);
        frameLabel->setText(Text::instance().Frame);
        timeLabel->setText(Text::instance().Time);
        startButton_->setEnabled(false);
        pathDelay->start();
    });
    connect(pathDelay, &QTimer::timeout, this,
            [frames, input = inputFileCard_->lineEdit]() {
        const QString path = input->text().trimmed();
        if (QFileInfo(path).isFile()) frames->open(path);
    });
    connect(inputFileCard_->lineEdit, &QLineEdit::editingFinished, this,
            [frames, input = inputFileCard_->lineEdit]() {
        const QString path = input->text().trimmed();
        if (!path.isEmpty() && !QFileInfo(path).isFile()) frames->open(path);
    });
    connect(frames, &VideoFrameService::videoOpened, this,
            [this, slider, frameLabel, timeLabel, input = inputFileCard_->lineEdit]
            (int count, double fps, double duration) {
        const QSignalBlocker blocker(slider);
        slider->setRange(0, qMax(0, count - 1));
        slider->setValue(0);
        slider->setEnabled(count > 1);
        frameLabel->setText(count > 0
            ? formatText(Text::instance().Frame2, {QStringLiteral("1"), QString::number(count)})
            : QStringLiteral("帧: -/-"));
        timeLabel->setText(formatText(Text::instance().Time2,
            {formatVideoTime(0), formatVideoTime(duration)}));
        logMessage(formatText(Text::instance().VLS, {input->text()}));
        QString frameSummary = Text::instance().TotalFramesFPS2f;
        frameSummary.replace(QStringLiteral("{:.2f}"), QStringLiteral("{}"));
        logMessage(formatText(frameSummary,
            {QString::number(count), QString::number(fps, 'f', 2)}));
    });
    connect(slider, &QSlider::valueChanged, seekDelay,
            [seekDelay](int) { seekDelay->start(); });
    connect(seekDelay, &QTimer::timeout, this,
            [frames, slider]() { frames->requestFrame(slider->value()); });
    connect(frames, &VideoFrameService::frameReady, this,
            [frames, preview, frameLabel, timeLabel](int index, const QImage& image) {
        preview->setFrame(image);
        frameLabel->setText(formatText(Text::instance().Frame2,
            {QString::number(index + 1), QString::number(frames->totalFrames())}));
        timeLabel->setText(formatText(Text::instance().Time2,
            {formatVideoTime(frames->fps() > 0 ? index / frames->fps() : 0),
             formatVideoTime(frames->duration())}));
    });
    connect(frames, &VideoFrameService::failed, this,
            [this, preview](const QString& error) {
        if (!preview->hasFrame()) preview->setError(error);
        const QString message = formatText(Text::instance().FailedToLoadVideo, {error});
        logMessage(message, true);
        NotificationService::error(Text::instance().Error, message, this);
        if (!preview->hasFrame()) startButton_->setEnabled(false);
    });
    connect(preview, &VideoPreview::cropSelected, startButton_, &QWidget::setEnabled);
    startButton_->setEnabled(false);
    // 开始前记录框选区域（对齐 Python _start_processing 的 UsingCustomAreaX 日志）；
    // 该连接先于堆叠界面的 addTask 派发建立，保证日志先于任务入队
    connect(this, &BaseFunctionInterface::taskRequested, this,
            [this](const QString&, const QString&) {
        const QVector<QRect> rects = cropRects();
        if (rects.isEmpty()) return;
        const QRect rect = rects.first();
        logMessage(formatText(Text::instance().UsingCustomAreaX,
                              {QString::number(rect.x()), QString::number(rect.y()),
                               QString::number(rect.width()),
                               QString::number(rect.height())}));
    });
}

void VideocrInterface::logMessage(const QString& message, bool isError,
                                  bool isFlush) {
    // 对齐 Python VideocrInterface._log_message
    if (!logText_) return;
    const QString timestamp =
        QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));
    QString formatted = QStringLiteral("[%1] %2").arg(timestamp, message);
    if (isError)
        formatted = QStringLiteral("<font color=\"red\">%1</font>").arg(formatted);
    if (isFlush) {
        // 进度行刷新：先去掉上一行再追加（对齐 Python is_flush 分支）
        QStringList lines = logText_->toPlainText().split(QLatin1Char('\n'));
        if (!lines.isEmpty()) lines.removeLast();
        logText_->setPlainText(lines.join(QLatin1Char('\n')));
    }
    logText_->append(formatted);
    QScrollBar* bar = logText_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void VideocrInterface::clearLog() {
    if (logText_) logText_->clear();
}

QVector<QRect> VideocrInterface::cropRects() const {
    return preview_ ? preview_->selectionRects() : QVector<QRect>{};
}

bool VideocrInterface::validateBeforeStart(QString* errorMessage) {
    // 对齐 Python OcrInterface._start_processing 的前置校验
    const auto& cfg = AppConfig::instance();
    const auto& t = Text::instance();
    const QString paddleocrPath = cfg.value(ConfigKeys::paddleocrPath).toString();
    const QString supportFilesPath =
        cfg.value(ConfigKeys::supportFilesPath).toString();
    const QString tempDir = cfg.value(ConfigKeys::tempDir).toString();

    if (paddleocrPath.isEmpty() || !QFileInfo::exists(paddleocrPath)) {
        *errorMessage = formatText(t.PaddleocrExeNotFound, {paddleocrPath});
        return false;
    }
    // 三个路径都不允许含中文（PaddleOCR 命令行无法处理）
    if (containsChinese(paddleocrPath)) {
        showWarningDialog(this, formatText(t.PPMNCCC, {paddleocrPath}));
        return false;
    }
    if (containsChinese(supportFilesPath)) {
        showWarningDialog(this, formatText(t.SFPMNCCC, {supportFilesPath}));
        return false;
    }
    if (containsChinese(tempDir)) {
        showWarningDialog(this, formatText(t.TFPMNCCC2, {tempDir}));
        return false;
    }
    if (!QFileInfo::exists(supportFilesPath)) {
        *errorMessage = formatText(t.SFPDNE, {supportFilesPath});
        return false;
    }
    if (inputPath().isEmpty()) {
        *errorMessage = t.PSAVFF;
        return false;
    }
    if (outputPath().isEmpty()) {
        *errorMessage = t.PSTOFP;
        return false;
    }
    // 必须已在视频预览中框选裁剪区域
    if (cropRects().isEmpty()) {
        *errorMessage = t.PSSAF;
        return false;
    }
    return true;
}

void VideocrInterface::setInputPath(const QString& path) {
    BaseFunctionInterface::setInputPath(path);
    const QFileInfo file(path);
    if (file.isFile())
        outputFileCard_->lineEdit->setText(file.dir().filePath(
            file.fileName() == QStringLiteral("生肉.mp4")
                ? QStringLiteral("原文_OCR.srt") : QStringLiteral("原文.srt")));
    startButton_->setEnabled(false);
}

void VideocrInterface::showEvent(QShowEvent* event) {
    BaseFunctionInterface::showEvent(event);
    if (preview_)
        preview_->setMaxCropBoxes(AppConfig::instance().value(ConfigKeys::useDualZone).toBool() ? 2 : 1);
}

VideocrStackedInterfaces::VideocrStackedInterfaces(QWidget* parent)
    : BaseStackedInterfaces(parent) {
    setObjectName(QStringLiteral("VideocrStackedInterfaces"));
    auto* main = new VideocrInterface(this);
    auto* tasks = new OcrTaskInterface(this);
    addSubInterface(main, QStringLiteral("mainInterface"), trText("字幕提取"));
    addSubInterface(tasks, QStringLiteral("taskInterface"), joinTranslatedLabel(trText("字幕提取"), trText("任务")));
    addSubInterface(new OCRSettingInterface(this), QStringLiteral("settingInterface"),
                    trText("高级设置"));
    connect(main, &BaseFunctionInterface::taskRequested, tasks,
            [tasks, main](const QString& input, const QString& output) {
                // 对齐 Python _get_args：裁剪坐标取自视频预览的框选结果
                tasks->setCropRects(main->cropRects());
                tasks->addTask(input, output);
            });
    connect(tasks, &BaseTaskInterface::returnTask, main,
            [main](bool duplicated, const QStringList& paths, bool notify) {
                main->updateTask(duplicated, paths, notify);
            });
    // OCR Worker 通过 taskLogSignal 输出带语义的日志（进度行刷新/错误红色），
    // 转发到主界面日志框（对齐 Python VideocrStackedInterfaces._forward_ocr_log）
    connect(&GlobalEventBus::instance(), &GlobalEventBus::taskLogSignal, main,
            [main](const QString& logName, const QString& message, bool isError,
                   bool isFlush) {
                if (logName == QStringLiteral("videocr"))
                    main->logMessage(message, isError, isFlush);
            });
}
}
