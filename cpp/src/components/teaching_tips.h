#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <QString>
#include <functional>

class QWidget;
class QFrame;
class QEvent;
namespace qfw { class TeachingTip; class TeachingTipManager; }

namespace fkw {
// 对应 Python TeachingTipManager；页面导航由主窗口提供。
class TeachingTipManager : public QObject {
public:
    TeachingTipManager(QWidget* window, std::function<void(const QString&)> openRoute,
                       std::function<QWidget*(const QString&)> routePage);
    void restartTour(int delayMs = 0);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    struct Step { QString title; QString content; QString route; QString anchor; int page = 0; };
    void showCurrentTip();
    void showTip();
    void closeTip();
    void updatePosition();
    QWidget* prepareStep();
    void nextStep();
    void previousStep();
    void finishTour();

    QPointer<QWidget> window_;
    std::function<void(const QString&)> openRoute_;
    std::function<QWidget*(const QString&)> routePage_;
    QVector<Step> steps_;
    QPointer<QWidget> target_;
    QPointer<QFrame> highlight_;
    QVector<QPointer<QObject>> tracked_;
    qfw::TeachingTipManager* positionManager_ = nullptr;
    QPointer<qfw::TeachingTip> tip_;
    QTimer timer_;
    QTimer positionTimer_;
    QMetaObject::Connection targetDestroyed_;
    int currentStep_ = 0;
};
} // namespace fkw
