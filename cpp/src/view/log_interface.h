#pragma once

#include <QHash>
#include <qtfluentwidgets.h>

namespace fkw {
class LogInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit LogInterface(const QString& key, QWidget* parent = nullptr);
    void setLog(const QString& value);
    void appendLog(const QString& value);
private:
    qfw::PlainTextEdit* text_;
};
class LogWindow : public qfw::FluentWindow {
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);
private:
    QHash<QString, LogInterface*> pages_;
};
}
