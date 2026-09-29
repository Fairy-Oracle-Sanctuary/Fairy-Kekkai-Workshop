#include "components/info_card.h"

#include <QHBoxLayout>
#include <QDesktopServices>
#include <QFile>
#include <QIcon>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/event_bus.h"
#include "common/logger.h"
#include "common/setting.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"
#include "components/statistic_widget.h"

namespace fkw {
FairyKekkaiWorkshopInfoCard::FairyKekkaiWorkshopInfoCard(QWidget* parent)
    : qfw::SimpleCardWidget(parent) {
    setBorderRadius(8);
    auto* icon = new qfw::ImageLabel(this);
    icon->setPixmap(QIcon(QStringLiteral(":/app/images/logo.png")).pixmap(120, 120));
    icon->setBorderRadius(8, 8, 8, 8);
    icon->scaledToWidth(120);
    auto* name = new qfw::TitleLabel(trText("Fairy Kekkai Workshop"), this);
    auto* update = new qfw::PrimaryPushButton(trText("更新"), this);
    update->setFixedWidth(160);
    auto* company = new qfw::HyperlinkLabel(QUrl(QStringLiteral("https://space.bilibili.com/499929312")),
                                             QStringLiteral("Baby2016"), this);
    // 版本号与更新时间统一取自内嵌 setting_data.json，避免与更新检查（version_service）比较用的版本号不一致。
    auto* version = new StatisticsWidget(
        trText("版本"), QStringLiteral("v") + settingData(QStringLiteral("VERSION")).toString(), this);
    auto* updateTime = new StatisticsWidget(
        trText("更新时间"), settingData(QStringLiteral("UPDATE_TIME")).toString(), this);
    // OCR 项显示当前 PaddleOCR 变体（含 CPU/GPU 标识），与 Python 端 info_card 的 PADDLEOCR_VERSION 对齐。
    auto* ocrVersion = new StatisticsWidget(QStringLiteral("OCR"), paddleOcrVersion(), this);
    auto* description = new qfw::BodyLabel(trText("仙 · 结界工坊"), this);
    description->setWordWrap(true);
    auto* log = new qfw::PrimaryPushButton(qfw::FluentIcon(qfw::FluentIconEnum::BookShelf).qicon(),
                                            trText("Log"), this);
    log->setFixedSize(80, 32);
    auto* floating = new qfw::PrimaryPushButton(qfw::FluentIcon(qfw::FluentIconEnum::Pin).qicon(),
                                                 QStringLiteral("OCR"), this);
    floating->setFixedSize(80, 32);
    auto* clear = new qfw::TransparentToolButton(qfw::FluentIconEnum::Delete, this);
    auto* reset = new qfw::TransparentToolButton(qfw::FluentIconEnum::Sync, this);
    auto* github = new qfw::TransparentToolButton(qfw::FluentIconEnum::Github, this);
    auto* website = new qfw::TransparentToolButton(qfw::FluentIconEnum::Globe, this);
    for (auto* button : {clear, reset, github, website}) button->setFixedSize(32, 32);
    clear->setToolTip(trText("清空所有日志"));
    reset->setToolTip(trText("重置所有设置并重启"));
    floating->setToolTip(trText("悬浮窗口"));

    auto* outer = new QHBoxLayout(this);
    outer->setSpacing(30);
    outer->setContentsMargins(34, 24, 24, 24);
    outer->addWidget(icon);
    auto* vertical = new QVBoxLayout();
    vertical->setSpacing(0);
    outer->addLayout(vertical, 1);
    auto* top = new QHBoxLayout();
    top->addWidget(name);
    top->addWidget(update, 0, Qt::AlignRight);
    vertical->addLayout(top);
    vertical->addSpacing(3);
    vertical->addWidget(company, 0, Qt::AlignLeft);
    vertical->addSpacing(20);
    auto* statistics = new QHBoxLayout();
    statistics->setSpacing(10);
    statistics->addWidget(version);
    statistics->addWidget(new qfw::VerticalSeparator());
    statistics->addWidget(updateTime);
    statistics->addWidget(new qfw::VerticalSeparator());
    statistics->addWidget(ocrVersion);
    statistics->addStretch();
    vertical->addLayout(statistics);
    vertical->addSpacing(20);
    vertical->addWidget(description);
    vertical->addSpacing(12);
    auto* actions = new QHBoxLayout();
    actions->setSpacing(12);
    actions->addWidget(log);
    actions->addWidget(floating);
    actions->addStretch();
    actions->addWidget(clear);
    actions->addWidget(reset);
    actions->addWidget(github);
    actions->addWidget(website);
    vertical->addLayout(actions);
    connect(log, &QPushButton::clicked, this, &FairyKekkaiWorkshopInfoCard::logRequested);
    // 对齐 Python info_card.__onFloatingButtonClicked：点击后立即禁用入口，等悬浮窗
    // 关闭（ocr_window_closed）再恢复，避免同时开出多个悬浮窗。
    connect(floating, &QPushButton::clicked, this, [this, floating]() {
        floating->setEnabled(false);
        emit floatingWindowRequested();
    });
#ifndef Q_OS_WIN
    // 屏幕框选 / 窗口绑定依赖 Win32，非 Windows 平台直接禁用入口。
    floating->setEnabled(false);
#else
    connect(&GlobalEventBus::instance(), &GlobalEventBus::ocr_window_closed, this,
            [floating]() { floating->setEnabled(true); });
#endif
    connect(update, &QPushButton::clicked, this,
            &FairyKekkaiWorkshopInfoCard::updateRequested);
    connect(clear, &QPushButton::clicked, this, [this]() {
        const auto& t = Text::instance();
        qfw::MessageDialog box(t.ClearAllLogs, t.AYSYWTCALFTACBU, window());
        box.yesButton->setText(t.OK);
        box.cancelButton->setText(t.Cancel);
        if (!box.exec()) return;
        const int removed = Logger::clearAllLogs();
        emit GlobalEventBus::instance().logsCleared();
        NotificationService::success(t.ClearSuccessful,
            formatText(t.ClearedLogFiles, {QString::number(removed)}), this);
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        const auto& t = Text::instance();
        qfw::MessageDialog box(t.ResetAllSettings, t.AYSYWTRASTAWRATACBU, window());
        box.yesButton->setText(t.OK);
        box.cancelButton->setText(t.Cancel);
        if (!box.exec()) return;
        QFile config(configFile());
        if (config.exists() && !config.remove()) {
            NotificationService::error(t.ResetFailed, config.errorString(), this);
            return;
        }
        emit restartRequested();
    });
    connect(github, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop")));
    });
    connect(website, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://fkw.ora-san.org")));
    });
}
}
