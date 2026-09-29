#include "components/config_card.h"

#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QIntValidator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSpacerItem>
#include <QUrl>
#include <memory>

#include "common/app_data.h"
#include "common/config.h"
#include "common/setting.h"
#include "common/text.h"

namespace fkw {
ChooseFileSettingCard::ChooseFileSettingCard(qfw::FluentIconEnum icon,
                                             const QString& title, const QString& content,
                                             const QString& placeholder, QWidget* parent)
    : qfw::SettingCard(icon, title, content, parent) {
    lineEdit = new qfw::LineEdit(this);
    lineEdit->setReadOnly(true);
    lineEdit->setPlaceholderText(placeholder);
    browseBtn = new qfw::PushButton(trText("浏览文件"), this);
    for (int i = hBoxLayout_->count() - 1; i >= 0; --i) {
        if (hBoxLayout_->itemAt(i)->spacerItem()) {
            delete hBoxLayout_->takeAt(i);
            break;
        }
    }
    hBoxLayout_->addSpacing(24);
    hBoxLayout_->addWidget(lineEdit, 1);
    hBoxLayout_->addSpacing(8);
    hBoxLayout_->addWidget(browseBtn);
    hBoxLayout_->addSpacing(16);
}

DictSettingCard::DictSettingCard(qfw::FluentIconEnum icon, const QString& title,
                                 const QString& content, const QStringList& options,
                                 QWidget* parent)
    : qfw::SettingCard(icon, title, content, parent) {
    comboBox = new qfw::ComboBox(this);
    comboBox->addItems(options);
    if (!options.isEmpty()) comboBox->setCurrentIndex(0);
    hBoxLayout_->addWidget(comboBox, 0, Qt::AlignRight);
    hBoxLayout_->addSpacing(16);
}

LineEditSettingCard::LineEditSettingCard(qfw::FluentIconEnum icon, const QString& title,
                                         const QString& content, const QString& value,
                                         bool password, QWidget* parent)
    : LineEditSettingCard(QVariant::fromValue(qfw::FluentIcon(icon).qicon()),
                          title, content, value, password, parent) {}

LineEditSettingCard::LineEditSettingCard(const QVariant& icon, const QString& title,
                                         const QString& content, const QString& value,
                                         bool password, QWidget* parent)
    : qfw::SettingCard(icon, title, content, parent) {
    lineEdit = password ? static_cast<qfw::LineEdit*>(new qfw::PasswordLineEdit(this))
                        : new qfw::LineEdit(this);
    lineEdit->setFixedWidth(250);
    lineEdit->setText(value);
    hBoxLayout_->addWidget(lineEdit);
    hBoxLayout_->addSpacing(16);
}

PlainTextSettingCard::PlainTextSettingCard(qfw::FluentIconEnum icon, const QString& title,
                                           const QString& content, QWidget* parent)
    : qfw::SettingCard(icon, title, content, parent) {
    editor = new qfw::PlainTextEdit(this);
    editor->setFixedSize(350, 82);
    hBoxLayout_->addWidget(editor);
    hBoxLayout_->addSpacing(16);
    setFixedHeight(120);
}

FloatRangeSettingCard::FloatRangeSettingCard(qfw::FluentIconEnum icon, const QString& title,
                                             const QString& content, double value,
                                             double minimum, double maximum, QWidget* parent)
    : qfw::SettingCard(icon, title, content, parent),
      slider(new qfw::Slider(Qt::Horizontal, this)),
      valueLabel(new QLabel(this)) {
    setProperty("qssClass", "RangeSettingCard");
    slider->setMinimumWidth(268);
    slider->setRange(qRound(minimum * 100), qRound(maximum * 100));
    slider->setValue(qRound(value * 100));
    valueLabel->setText(QString::number(value, 'f', 2));
    hBoxLayout_->addStretch(1);
    hBoxLayout_->addWidget(valueLabel, 0, Qt::AlignRight);
    hBoxLayout_->addSpacing(6);
    hBoxLayout_->addWidget(slider, 0, Qt::AlignRight);
    hBoxLayout_->addSpacing(16);
}

AdvancedSettingInterface::AdvancedSettingInterface(const QString& title, QWidget* parent)
    : qfw::ScrollArea(parent) {
    auto* view = new QWidget(this);
    layout_ = new QVBoxLayout(view);
    layout_->setSpacing(26);
    layout_->setContentsMargins(36, 10, 36, 20);
    setWidget(view);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setViewportMargins(0, 90, 0, 20);
    enableTransparentBackground();
    auto* label = new qfw::TitleLabel(title, this);
    QFont font = label->font();
    font.setPixelSize(23);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->move(36, 40);
    label->setFixedHeight(40);
    label->raise();
}

qfw::SettingCardGroup* AdvancedSettingInterface::addGroup(const QString& title) {
    auto* group = new qfw::SettingCardGroup(title, layout_->parentWidget());
    if (auto* label = group->findChild<QLabel*>()) {
        QFont font = label->font();
        font.setPixelSize(14);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
    }
    layout_->addWidget(group);
    return group;
}

void AdvancedSettingInterface::finish() { layout_->addStretch(); }

// All advanced pages share AppConfig with the Python application.
namespace {
QVariant iconOf(qfw::FluentIconEnum icon) {
    return QVariant::fromValue(qfw::FluentIcon(icon).qicon());
}
QVariant iconOf(const QString& path) { return QVariant::fromValue(QIcon(path)); }
qfw::PushSettingCard* boundPath(QWidget* owner, qfw::SettingCardGroup* group,
                                ConfigKeys::Key key, const QVariant& icon,
                                const QString& title, const QString& button,
                                const QString& filter = {}, bool remember = false) {
    auto* card = new qfw::PushSettingCard(
        button, icon, title, AppConfig::instance().value(key).toString(), group);
    group->addSettingCard(card);
    QObject::connect(&AppConfig::instance(), &AppConfig::valueChanged, card,
                     [card, key](const QString& groupName, const QString& name,
                                 const QJsonValue& value) {
        if (groupName == QLatin1String(key.group) && name == QLatin1String(key.name))
            card->setContent(value.toString());
    });
    QObject::connect(card, &qfw::PushSettingCard::clicked, owner,
                     [owner, card, key, title, filter, remember] {
        auto& config = AppConfig::instance();
        const QString start = remember
            ? config.value(ConfigKeys::lastOpenPath).toString() : QString();
        const QString path = QFileDialog::getOpenFileName(owner, title, start, filter);
        if (path.isEmpty() || path == config.value(key).toString()) return;
        if (config.set(key, path)) {
            card->setContent(path);
            if (remember)
                config.set(ConfigKeys::lastOpenPath, QFileInfo(path).absolutePath());
        }
    });
    return card;
}
LineEditSettingCard* boundField(QWidget* owner, qfw::SettingCardGroup* group,
                                ConfigKeys::Key key, const QVariant& icon,
                                const QString& title, const QString& description,
                                const QString& placeholder = {}, bool password = false,
                                int width = 250) {
    auto* card = new LineEditSettingCard(icon, title, description,
        AppConfig::instance().value(key).toVariant().toString(), password, group);
    card->lineEdit->setFixedWidth(width);
    card->lineEdit->setPlaceholderText(placeholder.isEmpty() ? description : placeholder);
    group->addSettingCard(card);
    QObject::connect(card->lineEdit, &QLineEdit::editingFinished, owner, [card, key] {
        AppConfig::instance().set(key, card->lineEdit->text());
    });
    return card;
}
FloatRangeSettingCard* boundFloatRange(QWidget* owner, qfw::SettingCardGroup* group,
                                              ConfigKeys::Key key, qfw::FluentIconEnum icon,
                                              const QString& title, const QString& description,
                                              double minimum, double maximum) {
    auto* card = new FloatRangeSettingCard(icon, title, description,
        AppConfig::instance().value(key).toDouble(), minimum, maximum, group);
    group->addSettingCard(card);
    QObject::connect(card->slider, &QSlider::valueChanged, owner, [card, key](int value) {
        const double scaled = value / 100.0;
        card->valueLabel->setText(QString::number(scaled, 'f', 2));
        AppConfig::instance().set(key, scaled);
    });
    return card;
}
qfw::RangeSettingCard* boundRange(QWidget* owner, qfw::SettingCardGroup* group,
                                  ConfigKeys::Key key, qfw::FluentIconEnum icon,
                                  const QString& title, const QString& description,
                                  int minimum, int maximum) {
    auto* item = new qfw::RangeConfigItem(
        QLatin1String(key.group), QLatin1String(key.name),
        AppConfig::instance().value(key).toInt(),
        std::make_shared<qfw::RangeValidator>(minimum, maximum));
    item->setParent(owner);
    auto* card = new qfw::RangeSettingCard(item, icon, title, description, group);
    group->addSettingCard(card);
    QObject::connect(card, &qfw::RangeSettingCard::valueChanged, owner,
                     [key](int value) { AppConfig::instance().set(key, value); });
    return card;
}
}

qfw::SwitchSettingCard* boundSwitch(QWidget* owner, qfw::SettingCardGroup* group,
                                    ConfigKeys::Key key, qfw::FluentIconEnum icon,
                                    const QString& title, const QString& description) {
    auto* card = new qfw::SwitchSettingCard(icon, title, description, nullptr, group);
    card->setChecked(AppConfig::instance().value(key).toBool());
    group->addSettingCard(card);
    QObject::connect(card, &qfw::SwitchSettingCard::checkedChanged, owner,
                     [key](bool checked) { AppConfig::instance().set(key, checked); });
    return card;
}

DictSettingCard* boundChoice(QWidget* owner, qfw::SettingCardGroup* group,
                             ConfigKeys::Key key, qfw::FluentIconEnum icon,
                             const QString& title, const QString& description,
                             const QStringList& labels, const QStringList& values,
                             bool numeric) {
    auto* card = new DictSettingCard(icon, title, description, labels, group);
    const auto saved = AppConfig::instance().value(key);
    const int selected = values.indexOf(numeric ? QString::number(saved.toInt())
                                                : saved.toString());
    card->comboBox->setCurrentIndex(qMax(0, selected));
    group->addSettingCard(card);
    QObject::connect(card->comboBox, &qfw::ComboBox::currentIndexChanged, owner,
                     [key, values, numeric](int index) {
        if (index < 0 || index >= values.size()) return;
        AppConfig::instance().set(key, numeric ? QJsonValue(values.at(index).toInt())
                                               : QJsonValue(values.at(index)));
    });
    return card;
}

YTDLPSettingInterface::YTDLPSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().YDDS, parent) {
    setObjectName(QStringLiteral("ytdlpSettingInterface"));
    const auto& t = Text::instance();
    using I = qfw::FluentIconEnum;
    auto* path = addGroup(t.YTDLPPath);
    boundPath(this, path, ConfigKeys::ytdlpPath,
              iconOf(QStringLiteral(":/app/images/logo/ytdlp.svg")),
              QStringLiteral("yt-dlp"), t.SelectFile3);
    auto* quality = addGroup(t.DFQ);
    boundChoice(this, quality, ConfigKeys::downloadQuality, I::Camera, t.VideoQuality, t.SVR,
                {t.Text4K2160p, t.Text2K1440p, t.FullHD1080p, t.HD720p,
                 t.SD480p, t.Smooth360p, t.BestQuality, t.WorstQuality},
                {QStringLiteral("2160"), QStringLiteral("1440"), QStringLiteral("1080"),
                 QStringLiteral("720"), QStringLiteral("480"), QStringLiteral("360"),
                 QStringLiteral("best"), QStringLiteral("worst")});
    auto* proxy = addGroup(t.ProxySettings);
    boundSwitch(this, proxy, ConfigKeys::systemProxy, I::Wifi, t.SystemProxy, t.WTUSDP);
    boundField(this, proxy, ConfigKeys::proxyUrl, iconOf(I::Globe),
               t.CustomProxy, t.SCPA,
               AppConfig::instance().value(ConfigKeys::proxyUrl).toString());
    auto* subtitles = addGroup(t.SubtitleSettings);
    boundSwitch(this, subtitles, ConfigKeys::downloadSubtitles, I::Language,
                t.DownloadSubtitles, t.ADVS);
    boundField(this, subtitles, ConfigKeys::subtitleLanguages, iconOf(I::Font),
               t.SubtitleLanguage, t.SSLSMLWC, QStringLiteral("en,zh,ja"));
    boundSwitch(this, subtitles, ConfigKeys::embedSubtitles, I::Chat,
                t.EmbedSubtitles, t.ESIVF);
    auto* metadata = addGroup(t.MetadataThumbnails);
    boundSwitch(this, metadata, ConfigKeys::downloadThumbnail, I::Photo,
                t.DownloadThumbnails, t.DVT);
    boundSwitch(this, metadata, ConfigKeys::embedThumbnail, I::Photo,
                t.EmbedThumbnails, t.ETIVF);
    boundSwitch(this, metadata, ConfigKeys::downloadMetadata, I::Info,
                t.DownloadMetadata, t.DVMI);
    boundSwitch(this, metadata, ConfigKeys::writeDescription, I::Document,
                t.WriteDescription, t.WVDTSF);
    boundSwitch(this, metadata, ConfigKeys::writeInfoJson, I::Code,
                t.WriteInfoJSON, t.WVITJF);
    boundSwitch(this, metadata, ConfigKeys::writeAnnotations, I::Edit,
                t.WriteComments, t.WVCI);
    auto* control = addGroup(t.DownloadControl);
    boundRange(this, control, ConfigKeys::concurrentDownloads, I::SpeedHigh,
               t.ConcurrentDownloads, t.MNOSD, 1, 10);
    boundRange(this, control, ConfigKeys::retryAttempts, I::Sync,
               t.RetryCount, t.NORODF, 0, 10);
    boundRange(this, control, ConfigKeys::downloadTimeout, I::Rotate,
               t.DTS, t.SDTD, 60, 3600);
    boundSwitch(this, control, ConfigKeys::limitDownloadRate, I::PageLeft,
                t.RateLimiting, t.EDRL);
    boundField(this, control, ConfigKeys::maxDownloadRate, iconOf(I::PageRight),
               t.MaxDownloadRate, t.SMDREG11, QStringLiteral("10M"));
    boundSwitch(this, control, ConfigKeys::skipExistingFiles, I::Accept,
                t.SkipExistingFiles, t.ARDEF);
    auto* advanced = addGroup(t.AdvancedSettings);
    boundField(this, advanced, ConfigKeys::outputTemplate, iconOf(I::Save),
               t.OutputTemplate, t.SOFT, QStringLiteral("%(title)s.%(ext)s"));
    boundSwitch(this, advanced, ConfigKeys::useCookies, I::Certificate,
                t.UseCookies, t.UCFFD);
    boundPath(this, advanced, ConfigKeys::cookiesFile, iconOf(I::Document),
              trText("Cookies文件"), t.SelectFile3, t.TextFilesTxtAllFiles);
    finish();
}

OCRSettingInterface::OCRSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().OCRSettings, parent) {
    setObjectName(QStringLiteral("OcrSettingInterface"));
    const auto& t = Text::instance();
    auto& cfg = AppConfig::instance();
    auto* programs = addGroup(t.PaddleOCRPath);
    auto pathCard = [this, &cfg, programs](ConfigKeys::Key key, const QString& title,
                                          const QString& button, const QString& warning,
                                          bool directory, const QString& filter = {}) {
        auto* card = new qfw::PushSettingCard(
            button, QVariant::fromValue(QIcon(QStringLiteral(":/app/images/logo/Paddle.svg"))),
            title, cfg.value(key).toString(), programs);
        programs->addSettingCard(card);
        connect(card, &qfw::PushSettingCard::clicked, this,
                [this, card, key, title, warning, directory, filter, &cfg]() {
            const QString path = directory
                ? QFileDialog::getExistingDirectory(this, title)
                : QFileDialog::getOpenFileName(this, title, {}, filter);
            if (path.isEmpty() || path == cfg.value(key).toString()) return;
            static const QRegularExpression cjk(
                QStringLiteral("[\\x{3400}-\\x{4dbf}\\x{4e00}-\\x{9fff}]"));
            if (path.contains(cjk)) {
                qfw::MessageDialog dialog(Text::instance().Warning, warning, window());
                dialog.yesButton->setText(Text::instance().OK2);
                dialog.cancelButton->hide();
                dialog.exec();
                return;
            }
            if (cfg.set(key, path)) card->setContent(path);
        });
    };
    pathCard(ConfigKeys::paddleocrPath, t.PSPE, t.SelectFile3, t.PEPMNCCC,
             false, QStringLiteral("paddleocr.exe (*.exe)"));
    pathCard(ConfigKeys::supportFilesPath, t.PleaseSelectOCRModel, t.SelectFolder,
             t.OMPMNCCC, true);
    pathCard(ConfigKeys::videocrCliPath, t.PSVCE, t.SelectFile3, t.VCEPMNCCC,
             false, QStringLiteral("videocr-cli.exe (*.exe)"));
    pathCard(ConfigKeys::tempDir, t.PSSETF, t.SelectFolder, t.TFPMNCCC, true);

    auto field = [this, &cfg](qfw::SettingCardGroup* group, ConfigKeys::Key key,
                              qfw::FluentIconEnum icon, const QString& title,
                              const QString& description, const QString& placeholder = {}) {
        auto* card = new LineEditSettingCard(
            icon, title, description, cfg.value(key).toVariant().toString(), false, group);
        card->lineEdit->setPlaceholderText(placeholder.isEmpty() ? description : placeholder);
        group->addSettingCard(card);
        connect(card->lineEdit, &QLineEdit::editingFinished, this, [card, key, &cfg]() {
            cfg.set(key, card->lineEdit->text());
        });
        return card;
    };
    auto number = [this, &cfg](qfw::SettingCardGroup* group, ConfigKeys::Key key,
                               qfw::FluentIconEnum icon, const QString& title,
                               const QString& description, double low, double high,
                               int decimals) {
        const QString original = cfg.value(key).toVariant().toString();
        auto* card = new LineEditSettingCard(icon, title, description, original, false, group);
        if (decimals == 0)
            card->lineEdit->setValidator(new QIntValidator(int(low), int(high), card));
        else {
            auto* validator = new QDoubleValidator(low, high, decimals, card);
            validator->setLocale(QLocale::c());
            card->lineEdit->setValidator(validator);
        }
        card->lineEdit->setPlaceholderText(original);
        group->addSettingCard(card);
        connect(card->lineEdit, &QLineEdit::editingFinished, this, [card, key, &cfg]() {
            const QString value = card->lineEdit->text();
            if (!card->lineEdit->hasAcceptableInput()) {
                card->lineEdit->setText(cfg.value(key).toVariant().toString());
                return;
            }
            cfg.set(key, value.toDouble());
        });
    };
    auto* timing = addGroup(t.TimeSettings);
    field(timing, ConfigKeys::timeStart, qfw::FluentIconEnum::StopWatch,
          t.StartTime, t.STSTFVPEG00O124, QStringLiteral("0:00"));
    field(timing, ConfigKeys::timeEnd, qfw::FluentIconEnum::StopWatch,
          t.EndTime, t.STETFVPEG01O235);
    auto* thresholds = addGroup(t.ThresholdSettings);
    number(thresholds, ConfigKeys::simThreshold, qfw::FluentIconEnum::Rotate,
           t.SimilarityThreshold, t.STBSF01, 0, 100, 0);
    number(thresholds, ConfigKeys::ssimThreshold, qfw::FluentIconEnum::Palette,
           t.SSIMThreshold, t.SSITFDFC01, 0, 100, 0);
    auto* processing = addGroup(t.ProcessingParameters);
    number(processing, ConfigKeys::maxMergeGap, qfw::FluentIconEnum::BackToWindow,
           t.MaxMergeInterval, t.MMIBSS0110, 0.1, 10.0, 2);
    number(processing, ConfigKeys::ocrImageMaxWidth, qfw::FluentIconEnum::Zoom,
           t.MaxOCRImageWidth, t.MIWFOP14P, 100, 4096, 0);
    number(processing, ConfigKeys::framesToSkip, qfw::FluentIconEnum::Market,
           t.SkipFrames, t.FTSDPFS01, 0, 100, 0);
    number(processing, ConfigKeys::minSubtitleDuration, qfw::FluentIconEnum::StopWatch,
           t.MinSubtitleDuration, t.MSDSOF0110, 0.1, 10.0, 2);
    number(processing, ConfigKeys::confidenceThreshold, qfw::FluentIconEnum::Filter,
           t.TextAuto007, t.TextAuto008, 1, 100, 0);
    auto* features = addGroup(t.FeatureToggles);
    auto feature = [this, &cfg, features](ConfigKeys::Key key, qfw::FluentIconEnum icon,
                                          const QString& title, const QString& description) {
        auto* card = new qfw::SwitchSettingCard(icon, title, description, nullptr, features);
        card->setChecked(cfg.value(key).toBool());
        features->addSettingCard(card);
        connect(card, &qfw::SwitchSettingCard::checkedChanged, this,
                [key, &cfg](bool checked) { cfg.set(key, checked); });
        return card;
    };
    auto* gpu = feature(ConfigKeys::useGpu, qfw::FluentIconEnum::DeveloperTools, t.EGA, t.UGFFOP);
    const QString ocrVersion = paddleOcrVersion();
    if (ocrVersion.contains(QStringLiteral("CPU"), Qt::CaseInsensitive)) {
        cfg.set(ConfigKeys::useGpu, false);
        gpu->setChecked(false);
        gpu->setEnabled(false);
    } else if (ocrVersion.contains(QStringLiteral("GPU"), Qt::CaseInsensitive)) {
        // PaddleOCR 3.7.0 GPU 版在 CPU 模式下无法推理 PP-OCRv6，强制启用 GPU
        cfg.set(ConfigKeys::useGpu, true);
        gpu->setChecked(true);
        gpu->setEnabled(false);
    }
    feature(ConfigKeys::useDualZone, qfw::FluentIconEnum::View,
            t.EnableDualAreaOCR, t.SSTASP);
    feature(ConfigKeys::postProcessing, qfw::FluentIconEnum::Edit,
            t.UsePostProcessing, t.APPOTOR);
    feature(ConfigKeys::useServerModel, qfw::FluentIconEnum::Cloud,
            t.UHPM, t.UABMFOP);
    finish();
}

TranslateSettingInterface::TranslateSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().AISettings, parent) {
    setObjectName(QStringLiteral("TranslateSettingInterface"));
    const auto& t = Text::instance();
    using I = qfw::FluentIconEnum;
    auto* ai = addGroup(t.AIParameters);
    auto* temperature = boundField(this, ai, ConfigKeys::aiTemperature,
        iconOf(I::SpeedHigh), t.AITemperature, t.AATGT02);
    auto* validator = new QDoubleValidator(0.1, 2.0, 1, temperature);
    validator->setLocale(QLocale::c());
    temperature->lineEdit->setValidator(validator);
    auto* prompt = new PlainTextSettingCard(I::Code, t.PromptTemplate, t.SYPT, ai);
    prompt->editor->setPlainText(AppConfig::instance().value(ConfigKeys::promptTemplate).toString());
    ai->addSettingCard(prompt);
    connect(prompt->editor, &QPlainTextEdit::textChanged, this, [prompt] {
        AppConfig::instance().set(ConfigKeys::promptTemplate, prompt->editor->toPlainText());
    });

    auto provider = [this](ConfigKeys::Key key, const QString& groupTitle,
                           const QString& icon, const QString& title, const QString& description) {
        auto* group = addGroup(groupTitle);
        boundField(this, group, key, iconOf(icon), title, description, {}, true, 350);
    };
    provider(ConfigKeys::hunyuanApiKey, t.TencentHunyuan,
             QStringLiteral(":/app/images/icons/hunyuan-turbos-latest.svg"),
             t.TencentHunyuanAPIKey, t.SYTHAK);
    provider(ConfigKeys::deepseekApiKey, t.Deepseek,
             QStringLiteral(":/app/images/icons/deepseek.svg"),
             t.DeepseekAPIKey, t.SYDAK);
    provider(ConfigKeys::geminiApiKey, t.Gemini,
             QStringLiteral(":/app/images/icons/gemini-3.5-flash.svg"),
             t.Gemini3FlashAPIKey, t.SYG3FAK);
    provider(ConfigKeys::glmApiKey, t.ZhipuGLM45FLASH,
             QStringLiteral(":/app/images/icons/glm-4.5-flash.svg"),
             t.ZG45FAK, t.SYG45FAK);
    provider(ConfigKeys::internApiKey, t.InternLM,
             QStringLiteral(":/app/images/icons/intern-latest.svg"),
             t.InternLMAPIKey, t.SYIAK);
    provider(ConfigKeys::sparkApiKey, t.XunfeiSparkLite,
             QStringLiteral(":/app/images/icons/spark-lite.svg"),
             t.XSLAP, t.SYXSLAP);
    provider(ConfigKeys::ernieSpeedApiKey, t.BES1NR,
             QStringLiteral(":/app/images/icons/ernie-speed-128k.svg"),
             t.BES1AK, t.SYBES1AK);

    auto* custom = addGroup(t.CMOCA);
    boundSwitch(this, custom, ConfigKeys::customModelEnabled,
                I::Setting, t.EnableCustomModel, t.ECMS);
    boundField(this, custom, ConfigKeys::customModelName, iconOf(I::Tag),
               t.ModelName, t.SetCustomModelName, trText("例如: gpt-4o-mini"), false, 350);
    boundField(this, custom, ConfigKeys::customModelApiKey, iconOf(I::Info),
               t.APIKey, t.SetCustomModelAPIKey, {}, true, 350);
    auto* baseUrl = boundField(this, custom, ConfigKeys::customModelBaseUrl,
               iconOf(I::Link), t.APIBaseURL, t.SCMABU,
               trText("例如: https://api.openai.com/v1"), false, 350);
    boundField(this, custom, ConfigKeys::customModelEndpoint, iconOf(I::Code),
               t.ModelEndpoint, t.SMEODTMN, {}, false, 350);
    auto* detection = new qfw::PushSettingCard(t.Detect, I::Search,
        t.DetectionParams, t.DetectionParamsHint, custom);
    custom->addSettingCard(detection);
    connect(detection, &qfw::PushSettingCard::clicked, this, [this, baseUrl, detection] {
        checkCustomModel(baseUrl, detection);
    });
    finish();
}

void TranslateSettingInterface::checkCustomModel(LineEditSettingCard* baseUrlCard,
                                                  qfw::PushSettingCard* target) {
    const auto& t = Text::instance();
    const auto notify = [this, target](const QString& title, const QString& content) {
        qfw::Flyout::create(title, content, {}, {}, true,
            QVariant::fromValue(static_cast<QWidget*>(target)), this,
            qfw::FlyoutAnimationType::PullUp);
    };
    if (checkingModel_) {
        notify(t.Info, t.DetectionInProgress);
        return;
    }
    const auto& cfg = AppConfig::instance();
    const QString apiKey = cfg.value(ConfigKeys::customModelApiKey).toString().trimmed();
    QString base = cfg.value(ConfigKeys::customModelBaseUrl).toString().trimmed();
    const QString model = cfg.value(ConfigKeys::customModelName).toString().trimmed();
    const QString endpoint = cfg.value(ConfigKeys::customModelEndpoint).toString().trimmed();
    const auto invalid = [notify](const QString& message) {
        notify(trText("检测失败"), message);
    };
    if (apiKey.isEmpty()) { invalid(t.APIKeyCannotBeEmpty); return; }
    if (base.isEmpty()) { invalid(t.ABUCBE); return; }
    if (model.isEmpty()) { invalid(t.MNCBE); return; }
    QUrl url(base);
    if ((url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        || url.host().isEmpty()) {
        invalid(t.ABUMSWHOH);
        return;
    }
    if (url.path().isEmpty() || url.path() == QStringLiteral("/"))
        url.setPath(QStringLiteral("/v1"));
    base = url.toString().remove(QRegularExpression(QStringLiteral("/+$")));
    QUrl requestUrl(base + QStringLiteral("/chat/completions"));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(15000);
    QJsonObject payload{
        {QStringLiteral("model"), endpoint.isEmpty() ? model : endpoint},
        {QStringLiteral("messages"), QJsonArray{
            QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                        {QStringLiteral("content"), QStringLiteral("Hello")}}}},
        {QStringLiteral("max_tokens"), 10}
    };
    checkingModel_ = true;
    notify(t.DetectingTitle, t.VerifyingCustomModel);
    auto* manager = new QNetworkAccessManager(this);
    auto* reply = manager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager, baseUrlCard, base, notify] {
        checkingModel_ = false;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject result = QJsonDocument::fromJson(reply->readAll()).object();
        const QString error = reply->errorString();
        const auto networkError = reply->error();
        const bool networkOk = networkError == QNetworkReply::NoError && status < 400;
        reply->deleteLater();
        manager->deleteLater();
        const auto& t = Text::instance();
        if (!networkOk) {
            const QString message = status == 401 ? t.AKIIOHE
                : status == 404 ? t.MDNEOABUII
                : networkError == QNetworkReply::TimeoutError ? t.RTPCNC
                : t.DetectionFailed + QStringLiteral(": ") + error.left(100);
            notify(trText("检测失败"), message);
            return;
        }
        baseUrlCard->lineEdit->setText(base);
        AppConfig::instance().set(ConfigKeys::customModelBaseUrl, base);
        if (!result.value(QStringLiteral("choices")).toArray().isEmpty())
            notify(trText("检测成功"), t.PCCACS);
        else notify(trText("检测失败"), t.APIResponseAbnormal);
    });
}

WhisperSettingInterface::WhisperSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().WSRS, parent) {
    setObjectName(QStringLiteral("whisperSettingInterface"));
    const auto& t = Text::instance();
    auto* cli = addGroup(t.ProgramPath);
    boundPath(this, cli, ConfigKeys::whisperCliPath, iconOf(qfw::FluentIconEnum::Application),
              QStringLiteral("main.exe"), t.SelectProgram,
              QStringLiteral("Executable files (*.exe);;All files (*.*)"), true);
    auto* model = addGroup(t.ModelPath);
    boundPath(this, model, ConfigKeys::whisperModelPath, iconOf(qfw::FluentIconEnum::Document),
              t.ModelFile, t.SelectModelFile, t.ModelFileFilter, true);
    auto* gpu = addGroup(t.GPUAcceleration);
    boundSwitch(this, gpu, ConfigKeys::whisperUseGpu, qfw::FluentIconEnum::Game,
                t.EGA2, t.UGFSRA);
    finish();
}

FFmpegSettingInterface::FFmpegSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().FVES, parent) {
    setObjectName(QStringLiteral("ffmpegSettingInterface"));
    const auto& t = Text::instance();
    using I = qfw::FluentIconEnum;
    auto* path = addGroup(t.FFmpegPath);
    boundPath(this, path, ConfigKeys::ffmpegPath,
              iconOf(QStringLiteral(":/app/images/logo/FFmpeg.svg")),
              QStringLiteral("FFmpeg"), t.SelectFile3);
    auto* video = addGroup(t.BasicVideoParameters);
    boundChoice(this, video, ConfigKeys::ffmpegVideoCodec, I::Video,
                t.VideoEncoder, t.SVEF,
                {QStringLiteral("H.264"), QStringLiteral("H.265"), QStringLiteral("VP9"), trText("复制原始")},
                {QStringLiteral("libx264"), QStringLiteral("libx265"),
                 QStringLiteral("libvpx-vp9"), QStringLiteral("copy")});
    boundRange(this, video, ConfigKeys::ffmpegCrf, I::SpeedHigh,
               t.CRFQualityParameter, t.Text0IL12ICRLVMBQ, 0, 51);
    boundChoice(this, video, ConfigKeys::ffmpegPreset, I::Setting,
                t.EncodingSpeedPreset, t.BBESACR,
                {t.VeryFast, t.SuperFast, t.VeryFast2, t.Faster, t.Fast,
                 t.Medium, t.Slow, t.Slower, t.VerySlow},
                {QStringLiteral("ultrafast"), QStringLiteral("superfast"),
                 QStringLiteral("veryfast"), QStringLiteral("faster"),
                 QStringLiteral("fast"), QStringLiteral("medium"),
                 QStringLiteral("slow"), QStringLiteral("slower"),
                 QStringLiteral("veryslow")});
    auto* audio = addGroup(t.AudioProcessing);
    boundChoice(this, audio, ConfigKeys::ffmpegAudioMode, I::Music,
                t.AudioProcessingMode, t.SAPM,
                {t.AutoDetect, t.EncodeAudio, t.NoAudio, t.CopyOriginal},
                {QStringLiteral("auto"), QStringLiteral("encode"),
                 QStringLiteral("none"), QStringLiteral("copy")});
    boundChoice(this, audio, ConfigKeys::ffmpegAudioCodec, I::Music,
                t.AudioEncoder, t.SAEF,
                {QStringLiteral("AAC"), QStringLiteral("MP3"), QStringLiteral("Opus"), trText("复制原始")},
                {QStringLiteral("aac"), QStringLiteral("libmp3lame"),
                 QStringLiteral("opus"), QStringLiteral("copy")});
    boundChoice(this, audio, ConfigKeys::ffmpegAudioBitrate, I::Volume,
                t.AudioBitrate, t.SAEQ,
                {QStringLiteral("64k"), QStringLiteral("96k"), QStringLiteral("128k"),
                 QStringLiteral("192k"), QStringLiteral("256k"), QStringLiteral("320k")},
                {QStringLiteral("64k"), QStringLiteral("96k"), QStringLiteral("128k"),
                 QStringLiteral("192k"), QStringLiteral("256k"), QStringLiteral("320k")});
    auto* advanced = addGroup(t.XAP);
    boundSwitch(this, advanced, ConfigKeys::ffmpegUseAdvanced, I::DeveloperTools, t.EAP, t.CXEP);
    boundRange(this, advanced, ConfigKeys::ffmpegRefFrames, I::Layout,
               t.ReferenceFrameCount, t.RFC11, 1, 16);
    boundRange(this, advanced, ConfigKeys::ffmpegBFrames, I::Scroll,
               t.BFrameCount, t.BFrameCount016, 0, 16);
    boundRange(this, advanced, ConfigKeys::ffmpegKeyint, I::Calendar,
               t.KeyframeInterval, t.MKI11, 1, 1000);
    boundRange(this, advanced, ConfigKeys::ffmpegMinkeyint, I::Calendar,
               t.MKI, t.MKI112, 1, 100);
    boundRange(this, advanced, ConfigKeys::ffmpegScenecut, I::Cut,
               t.SceneChangeThreshold, t.SCDT01, 0, 100);
    boundFloatRange(this, advanced, ConfigKeys::ffmpegQcomp, I::Edit,
                    t.QCF, t.QCCF0010, 0.0, 1.0);
    boundChoice(this, advanced, ConfigKeys::ffmpegAqMode, I::Alignment,
                t.AQM, t.SAQM,
                {trText("模式0"), trText("模式1"), trText("模式2"), trText("模式3")},
                {QStringLiteral("0"), QStringLiteral("1"),
                 QStringLiteral("2"), QStringLiteral("3")}, true);
    boundFloatRange(this, advanced, ConfigKeys::ffmpegAqStrength, I::Zoom,
                    t.AQS, t.AQS0020, 0.0, 2.0);
    auto* output = addGroup(t.OutputSettings);
    boundChoice(this, output, ConfigKeys::ffmpegOutputFormat, I::Save,
                t.OutputFileFormat, t.SOVF,
                {QStringLiteral("MP4"), QStringLiteral("MKV"), QStringLiteral("AVI"),
                 QStringLiteral("MOV"), QStringLiteral("WebM")},
                {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
                 QStringLiteral("mov"), QStringLiteral("webm")});
    boundSwitch(this, output, ConfigKeys::ffmpegOverwriteOutput, I::Accept,
                t.OverwriteOutputFile, t.OIOFAE);
    auto* processing = addGroup(t.VideoProcessing);
    boundRange(this, processing, ConfigKeys::concurrentEncodes, I::SpeedHigh,
               t.CEC, t.MPCWEMVS, 1, 5);
    boundChoice(this, processing, ConfigKeys::ffmpegScale, I::Zoom,
                t.VideoScaling, t.AVR,
                {t.KeepOriginalSize, t.Text720p, t.Text1080p,
                 t.Text1440p, t.Text2160p, t.Custom},
                {QStringLiteral("none"), QStringLiteral("720p"), QStringLiteral("1080p"),
                 QStringLiteral("1440p"), QStringLiteral("2160p"), QStringLiteral("custom")});
    boundField(this, processing, ConfigKeys::ffmpegCustomScale, iconOf(I::Edit),
               t.CustomSize, t.SCREG11, QStringLiteral("1920:1080"));
    boundChoice(this, processing, ConfigKeys::ffmpegFps, I::SpeedHigh,
                t.FrameRateSettings, t.SOVFR,
                {t.KOFR, t.Text24Fps, t.Text25Fps, t.Text30Fps,
                 t.Text50Fps, t.Text60Fps},
                {QStringLiteral("source"), QStringLiteral("24"), QStringLiteral("25"),
                 QStringLiteral("30"), QStringLiteral("50"), QStringLiteral("60")});
    boundField(this, processing, ConfigKeys::ffmpegVideoBitrate, iconOf(I::SpeedHigh),
               t.VideoBitrateLimit, t.SVBLEG2EMNL);
    auto* performance = addGroup(t.PerformanceOptions);
    boundSwitch(this, performance, ConfigKeys::ffmpegUseHardwareAcceleration,
                I::DeveloperTools, t.EHA, t.UGHAFE);
    boundChoice(this, performance, ConfigKeys::ffmpegHardwareAccelerator, I::VPN,
                t.HAT, t.SHAM,
                {t.AutoDetect, t.NVIDIACUDA, t.IntelQSV, t.DXVA2, t.VideoToolbox},
                {QStringLiteral("auto"), QStringLiteral("cuda"), QStringLiteral("qsv"),
                 QStringLiteral("dxva2"), QStringLiteral("videotoolbox")});
    finish();
}

ReleaseSettingInterface::ReleaseSettingInterface(QWidget* parent)
    : AdvancedSettingInterface(Text::instance().UploadSettings, parent) {
    setObjectName(QStringLiteral("releaseSettingInterface"));
    const auto& t = Text::instance();
    auto* program = addGroup(t.UploadVideoPath);
    boundPath(this, program, ConfigKeys::apiPath,
              iconOf(QStringLiteral(":/app/images/logo/bilibili.svg")),
              QStringLiteral("upload-video"), t.SelectFile3);
    auto* cookies = addGroup(t.CRFU);
    boundField(this, cookies, ConfigKeys::bilibiliSessdata,
               iconOf(qfw::FluentIconEnum::SearchMirror), t.SESSDATA,
               t.SetYourSESSDATA, {}, true, 350);
    boundField(this, cookies, ConfigKeys::bilibiliBiliJct,
               iconOf(qfw::FluentIconEnum::SearchMirror), t.BILIJCT,
               t.SetYourBILIJCT, {}, true, 350);
    boundField(this, cookies, ConfigKeys::bilibiliBuvid3,
               iconOf(qfw::FluentIconEnum::SearchMirror), t.BUVID3,
               t.SetYourBUVID3, {}, true, 350);
    finish();
}
}
