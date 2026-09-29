#pragma once
#include <QRect>
class QScreen;
namespace fkw {
QScreen* getCurrentScreen();
QRect getCurrentScreenGeometry(bool available = true);
}
