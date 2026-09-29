#pragma once

#include <qtfluentwidgets.h>

namespace fkw {
class FairyKekkaiWorkshopInfoCard : public qfw::SimpleCardWidget {
    Q_OBJECT
public:
    explicit FairyKekkaiWorkshopInfoCard(QWidget* parent = nullptr);
signals:
    void logRequested();
    void floatingWindowRequested();
    void updateRequested();
    void restartRequested();
};
}
