#include "components/statistic_widget.h"

#include <QFont>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

namespace fkw {
StatisticsWidget::StatisticsWidget(const QString& title, const QString& value, QWidget* parent)
    : QWidget(parent) {
    auto* titleLabel = new qfw::CaptionLabel(title, this);
    auto* valueLabel = new qfw::BodyLabel(value, this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->addWidget(valueLabel, 0, Qt::AlignTop);
    layout->addWidget(titleLabel, 0, Qt::AlignBottom);
    QFont font = valueLabel->font();
    font.setPointSize(18);
    font.setWeight(QFont::DemiBold);
    valueLabel->setFont(font);
}
}
