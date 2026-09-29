#pragma once
#include <QString>
#include <QHash>
#include <initializer_list>

namespace fkw {
// Python str.format syntax used by app/common/text.py. Text remains translated
// before formatting, as in the Python singleton.
QString formatText(const QString& pattern,
                   std::initializer_list<QString> positional,
                   const QHash<QString, QString>& named = {});
QString joinTranslatedLabel(const QString& left, const QString& right);
// Insert invisible break opportunities in long Latin tokens for wrapping UI labels.
QString wrapLongLatinRuns(const QString& text);
} // namespace fkw
