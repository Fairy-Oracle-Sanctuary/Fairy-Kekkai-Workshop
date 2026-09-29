#include "common/style_sheet.h"
#include <QFile>
#include <QWidget>
namespace fkw {
QString styleSheetPath(StyleSheet sheet, qfw::Theme theme) {
    if (theme == qfw::Theme::Auto) theme = qfw::QConfig::instance().theme();
    const QString name = sheet == StyleSheet::SampleCard
        ? QStringLiteral("sample_card") : QString();
    return QStringLiteral(":/app/qss/") +
        (qfw::isDarkThemeMode(theme) ? QStringLiteral("dark/")
                                     : QStringLiteral("light/")) +
        name + QStringLiteral(".qss");
}
void applyStyleSheet(QWidget* widget, StyleSheet sheet, qfw::Theme theme) {
    if (!widget) return;
    QFile file(styleSheetPath(sheet, theme));
    if (file.open(QIODevice::ReadOnly))
        widget->setStyleSheet(QString::fromUtf8(file.readAll()));
}
}
