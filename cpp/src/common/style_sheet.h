#pragma once
#include <QString>
#include <qtfluentwidgets.h>
class QWidget;
namespace fkw {
enum class StyleSheet { SampleCard };
QString styleSheetPath(StyleSheet sheet, qfw::Theme theme = qfw::Theme::Auto);
void applyStyleSheet(QWidget* widget, StyleSheet sheet,
                     qfw::Theme theme = qfw::Theme::Auto);
}
