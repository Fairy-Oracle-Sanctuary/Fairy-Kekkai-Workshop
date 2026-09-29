#pragma once

#include <QApplication>
#include <QWidget>
#include <qtfluentwidgets.h>

namespace fkw {

// Application notifications follow the Python NotificationService defaults.
class NotificationService {
public:
    static void success(const QString& title, const QString& content,
                        QWidget* parent = nullptr, int duration = 3000) {
        if (auto* target = windowFor(parent))
            qfw::InfoBar::success(title, content, Qt::Horizontal, true, duration,
                                  qfw::InfoBarPosition::BottomRight, target);
    }

    static void error(const QString& title, const QString& content,
                      QWidget* parent = nullptr, int duration = 5000) {
        if (auto* target = windowFor(parent))
            qfw::InfoBar::error(title, content, Qt::Horizontal, true, duration,
                                qfw::InfoBarPosition::BottomRight, target);
    }

    static void warning(const QString& title, const QString& content,
                        QWidget* parent = nullptr, int duration = 3000) {
        if (auto* target = windowFor(parent))
            qfw::InfoBar::warning(title, content, Qt::Horizontal, true, duration,
                                  qfw::InfoBarPosition::BottomRight, target);
    }

    static void info(const QString& title, const QString& content,
                     QWidget* parent = nullptr, int duration = 3000) {
        if (auto* target = windowFor(parent))
            qfw::InfoBar::info(title, content, Qt::Horizontal, true, duration,
                               qfw::InfoBarPosition::BottomRight, target);
    }

private:
    static QWidget* windowFor(QWidget* parent) {
        if (parent) return parent->window();
        for (QWidget* candidate : QApplication::topLevelWidgets())
            if (candidate->isWindow() && candidate->isVisible()) return candidate;
        return nullptr;
    }
};
}  // namespace fkw
