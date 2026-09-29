#pragma once

#include <qtfluentwidgets.h>

namespace fkw {
class HomeInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit HomeInterface(QWidget* parent = nullptr);
signals:
    void routeRequested(const QString& route);
    void logRequested();
    void restartRequested();
};
}
