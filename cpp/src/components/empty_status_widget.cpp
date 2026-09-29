#include "components/empty_status_widget.h"

#include <QRandomGenerator>
#include <QVBoxLayout>

namespace fkw
{
    EmptyStatusWidget::EmptyStatusWidget(const QString &text, QWidget *parent)
        : qfw::CardWidget(parent)
    {
        setBorderRadius(10);
        setFixedSize(346, 254);
        setClickEnabled(true);
        setCursor(Qt::PointingHandCursor);
        connect(this, &qfw::CardWidget::clicked, this, &EmptyStatusWidget::advanceIcon);
        iconIndex_ = QRandomGenerator::global()->bounded(10);
        icon_ = new qfw::IconWidget(this);
        icon_->setFixedSize(120, 120);
        icon_->setIcon(QStringLiteral(":/app/images/logo/Face%1.svg")
                           .arg(iconIndex_ + 1, 2, 10, QLatin1Char('0')));
        auto *label = new qfw::SubtitleLabel(text, this);
        label->setTextColor(QColor(96, 96, 96), QColor(216, 216, 216));
        label->setAlignment(Qt::AlignCenter);
        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(10);
        layout->setContentsMargins(16, 20, 16, 20);
        layout->addStretch();
        layout->addWidget(icon_, 0, Qt::AlignHCenter);
        layout->addWidget(label, 0, Qt::AlignHCenter);
        layout->addStretch();
    }
    void EmptyStatusWidget::advanceIcon()
    {
        iconIndex_ = (iconIndex_ + 1) % 10;
        icon_->setIcon(QStringLiteral(":/app/images/logo/Face%1.svg")
                           .arg(iconIndex_ + 1, 2, 10, QLatin1Char('0')));
    }
} // namespace fkw
