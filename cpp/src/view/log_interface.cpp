#include "view/log_interface.h"

#include <QDir>
#include <QFile>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/event_bus.h"

namespace fkw
{
    namespace {
    QString categoryFor(const QString& name) {
        const QString lower = name.toLower();
        if (lower.contains(QStringLiteral("download")) || lower.contains(QStringLiteral("ytdlp")))
            return QStringLiteral("downloadLog");
        if (lower.contains(QStringLiteral("project"))) return QStringLiteral("projectLog");
        if (lower.contains(QStringLiteral("videocr")) || lower.contains(QStringLiteral("ocr")))
            return QStringLiteral("videocrLog");
        if (lower.contains(QStringLiteral("whisper"))) return QStringLiteral("whisperLog");
        if (lower.contains(QStringLiteral("translate")) || lower.contains(QStringLiteral("ai")))
            return QStringLiteral("aiLog");
        if (lower.contains(QStringLiteral("ffmpeg"))) return QStringLiteral("ffmpegLog");
        return {};
    }
    }
    LogInterface::LogInterface(const QString &key, QWidget *parent) : qfw::ScrollArea(parent)
    {
        setObjectName(key);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *view = new QWidget(this);
        auto *layout = new QVBoxLayout(view);
        text_ = new qfw::PlainTextEdit(view);
        text_->setReadOnly(true);
        text_->setPlaceholderText(trText("日志将显示在这里"));
        layout->addWidget(text_);
        setWidget(view);
        enableTransparentBackground();
    }
    void LogInterface::setLog(const QString &value) { text_->setPlainText(value); }
    void LogInterface::appendLog(const QString &value) {
        text_->appendPlainText(value.trimmed());
    }

    LogWindow::LogWindow(QWidget *parent) : qfw::FluentWindow(parent)
    {
        setWindowTitle(trText("全部日志"));
        setWindowIcon(QIcon(QStringLiteral(":/app/images/logo.png")));
        setMicaEffectEnabled(false);
        resize(700, 400);
        setMinimumSize(400, 250);
        struct Page
        {
            const char *key;
            const char *label;
            qfw::FluentIconEnum icon;
        };
        const Page pages[] = {
            {"allLog", "全部日志", qfw::FluentIconEnum::Home},
            {"projectLog", "项目日志", qfw::FluentIconEnum::Folder},
            {"downloadLog", "下载日志", qfw::FluentIconEnum::Download},
            {"videocrLog", "字幕提取日志", qfw::FluentIconEnum::Video},
            {"whisperLog", "语音识别日志", qfw::FluentIconEnum::Microphone},
            {"aiLog", "AI翻译日志", qfw::FluentIconEnum::Message},
            {"ffmpegLog", "FFmpeg压制日志", qfw::FluentIconEnum::ZipFolder}};
        QString combined;
        QHash<QString, QString> categorized;
        const QDir logRoot(QDir(userDataFolder()).filePath(QStringLiteral("Log")));
        for (const auto &file : logRoot.entryInfoList({QStringLiteral("*.log")}, QDir::Files, QDir::Name))
        {
            QFile input(file.absoluteFilePath());
            if (!input.open(QIODevice::ReadOnly)) continue;
            const QString content = QString::fromUtf8(input.readAll()) + QLatin1Char('\n');
            combined += content;
            categorized[categoryFor(file.baseName())] += content;
        }
        for (const auto &page : pages)
        {
            const QString key = QString::fromUtf8(page.key);
            auto *view = new LogInterface(key, this);
            view->setLog(key == QStringLiteral("allLog") ? combined : categorized.value(key));
            pages_.insert(key, view);
            addSubInterface(view, page.icon, trText(page.label));
        }
        connect(&GlobalEventBus::instance(), &GlobalEventBus::logsCleared,
                this, [this]() {
            for (auto* page : pages_) page->setLog(QString());
        });
        connect(&GlobalEventBus::instance(), &GlobalEventBus::log_message,
                this, [this](const QString& name, const QString& message) {
            if (auto* all = pages_.value(QStringLiteral("allLog"))) all->appendLog(message);
            if (auto* category = pages_.value(categoryFor(name))) category->appendLog(message);
        });
    }
}
