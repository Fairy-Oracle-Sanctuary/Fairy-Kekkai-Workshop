#include "components/sample_card.h"
#include "common/style_sheet.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fkw
{
    BaseSampleCard::BaseSampleCard(const QIcon &icon, const QString &title,
                                   const QString &content, QWidget *parent)
        : qfw::CardWidget(parent)
    {
        setFixedSize(360, 90);
        setClickEnabled(true);
        auto *iconWidget = new qfw::IconWidget(icon, this);
        iconWidget->setFixedSize(48, 48);
        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("titleLabel"));
        auto *contentLabel = new QLabel(qfw::TextWrap::wrap(content, 40, false).first, this);
        contentLabel->setWordWrap(true);
        contentLabel->setObjectName(QStringLiteral("contentLabel"));
        auto *h = new QHBoxLayout(this);
        auto *v = new QVBoxLayout();
        h->setSpacing(28);
        h->setContentsMargins(20, 0, 0, 0);
        h->setAlignment(Qt::AlignVCenter);
        h->addWidget(iconWidget);
        v->setSpacing(2);
        v->setContentsMargins(0, 0, 0, 0);
        v->setAlignment(Qt::AlignVCenter);
        v->addStretch();
        v->addWidget(titleLabel);
        v->addWidget(contentLabel);
        v->addStretch();
        h->addLayout(v);
        const auto updateColors = [this]() {
            applyStyleSheet(this, StyleSheet::SampleCard);
        };
        connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged,
                this, updateColors);
        updateColors();
    }

    SampleCardView::SampleCardView(const QString &title, QWidget *parent) : QWidget(parent)
    {
        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("viewTitleLabel"));
        auto *vertical = new QVBoxLayout(this);
        flowLayout_ = new qfw::FlowLayout();
        vertical->setContentsMargins(36, 0, 36, 0);
        vertical->setSpacing(10);
        flowLayout_->setContentsMargins(0, 0, 0, 0);
        flowLayout_->setHorizontalSpacing(12);
        flowLayout_->setVerticalSpacing(12);
        vertical->addWidget(titleLabel);
        vertical->addLayout(flowLayout_, 1);
        const auto updateColors = [this]() {
            applyStyleSheet(this, StyleSheet::SampleCard);
        };
        connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged,
                this, updateColors);
        updateColors();
    }

    void SampleCardView::addSampleCard(const QIcon &icon, const QString &title,
                                       const QString &content, const QString &routeKey)
    {
        auto *card = new BaseSampleCard(icon, title, content, this);
        connect(card, &qfw::CardWidget::clicked, this,
                [this, routeKey]()
                { emit routeRequested(routeKey); });
        flowLayout_->addWidget(card);
    }

    void SampleCardView::addOpenUrlCard(const QIcon &icon, const QString &title,
                                        const QString &content, const QUrl &url)
    {
        auto *card = new BaseSampleCard(icon, title, content, this);
        card->setToolTip(url.toString());
        connect(card, &qfw::CardWidget::clicked, this, [url]()
                { QDesktopServices::openUrl(url); });
        flowLayout_->addWidget(card);
    }
}
