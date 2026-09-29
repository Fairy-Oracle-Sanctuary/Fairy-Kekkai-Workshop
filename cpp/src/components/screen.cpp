#include "components/screen.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

namespace fkw {
QScreen* getCurrentScreen() {
    for (QScreen* screen : QGuiApplication::screens())
        if (screen->geometry().contains(QCursor::pos())) return screen;
    return nullptr;
}
QRect getCurrentScreenGeometry(bool available) {
    QScreen* screen = getCurrentScreen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return QRect(0, 0, 1920, 1080);
    return available ? screen->availableGeometry() : screen->geometry();
}
}  // namespace fkw
