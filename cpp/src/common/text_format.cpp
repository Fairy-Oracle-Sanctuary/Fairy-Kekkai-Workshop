#include "common/text_format.h"
#include <QStringList>
#include <QRegularExpression>

namespace fkw {
QString wrapLongLatinRuns(const QString& text) {
    static const QRegularExpression longRun(QStringLiteral("[A-Za-z0-9_]{10,}"));
    QString result;
    int position = 0;
    auto matches = longRun.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += text.mid(position, match.capturedStart() - position);
        const QString run = match.captured();
        for (int i = 0; i < run.size(); ++i) {
            if (i) result += QChar(0x200B);
            result += run.at(i);
        }
        position = match.capturedEnd();
    }
    return result + text.mid(position);
}
QString formatText(const QString& pattern,
                   std::initializer_list<QString> positional,
                   const QHash<QString, QString>& named) {
    QStringList values;
    for (const auto& value : positional) values.append(value);
    QString result;
    result.reserve(pattern.size());
    int automaticIndex = 0;
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar ch = pattern.at(i);
        if (ch == QLatin1Char('{') && i + 1 < pattern.size()
            && pattern.at(i + 1) == QLatin1Char('{')) {
            result += QLatin1Char('{');
            ++i;
            continue;
        }
        if (ch == QLatin1Char('}') && i + 1 < pattern.size()
            && pattern.at(i + 1) == QLatin1Char('}')) {
            result += QLatin1Char('}');
            ++i;
            continue;
        }
        if (ch != QLatin1Char('{')) {
            result += ch;
            continue;
        }
        const int end = pattern.indexOf(QLatin1Char('}'), i + 1);
        if (end < 0) {
            result += ch;
            continue;
        }
        const QString key = pattern.mid(i + 1, end - i - 1);
        QString replacement;
        bool found = false;
        if (key.isEmpty()) {
            const int index = automaticIndex++;
            if (index < values.size()) {
                replacement = values.at(index);
                found = true;
            }
        } else if (named.contains(key)) {
            replacement = named.value(key);
            found = true;
        } else {
            bool numeric = false;
            const int index = key.toInt(&numeric);
            if (numeric && index >= 0 && index < values.size()) {
                replacement = values.at(index);
                found = true;
            }
        }
        if (found) {
            result += replacement;
            i = end;
        } else {
            result += ch;
        }
    }
    return result;
}
QString joinTranslatedLabel(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty()) return left + right;
    const ushort last = left.back().unicode();
    const ushort first = right.front().unicode();
    if (((last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z'))
        && ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')))
        return left + QLatin1Char(' ') + right;
    return left + right;
}
} // namespace fkw
