#pragma once

#include <qtfluentwidgets.h>

namespace fkw {
class EmptyStatusWidget : public qfw::CardWidget {
    Q_OBJECT
public:
    explicit EmptyStatusWidget(const QString& text, QWidget* parent = nullptr);
    void advanceIcon();
private:
    qfw::IconWidget* icon_ = nullptr;
    int iconIndex_ = 0;
};
} // namespace fkw
