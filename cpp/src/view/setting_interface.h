#pragma once

#include <qtfluentwidgets.h>

namespace fkw {
class SettingInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit SettingInterface(QWidget* parent = nullptr);
signals:
    void tutorialRequested();
};
}
