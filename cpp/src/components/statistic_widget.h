#pragma once

#include <QWidget>

namespace fkw {
class StatisticsWidget : public QWidget {
    Q_OBJECT
public:
    StatisticsWidget(const QString& title, const QString& value, QWidget* parent = nullptr);
};
}
