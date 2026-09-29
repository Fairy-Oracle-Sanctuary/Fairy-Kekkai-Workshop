#pragma once

#include <qtfluentwidgets.h>

#include <QList>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QRubberBand;
class QTimer;

namespace fkw {

// 全屏遮罩 + QRubberBand 框选（对应 app/components/floating_window.py 的 RangeSelector）。
// 左键拖拽选区，Esc 取消；确认后 emit rangeSelected（取消时发空 QRect）。
class RangeSelector : public QWidget {
    Q_OBJECT
public:
    explicit RangeSelector(QWidget* parent = nullptr);

signals:
    void rangeSelected(const QRect& rect);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QPoint origin_;
    QRubberBand* rubber_ = nullptr;
};

// 持续显示选中的 OCR 区域，半透明蓝色边框（对应 Python RangeOverlay）。
class RangeOverlay : public QWidget {
    Q_OBJECT
public:
    explicit RangeOverlay(const QRect& rect, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

// 悬浮 OCR 窗口（对应 Python FloatingWindow）：无边框 + 置顶 + 半透明背景，可拖拽、
// 可框选屏幕区域做单次 OCR / 一次翻译 / OCR 并翻译，可绑定并跟随目标窗口，支持
// 鼠标穿透、背景透明、工具栏锁定与历史记录。仅 Windows 启用窗口绑定与鼠标穿透。
class FloatingWindow : public QWidget {
    Q_OBJECT
public:
    explicit FloatingWindow(QWidget* parent = nullptr);
    ~FloatingWindow() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initUi();
    qfw::TransparentToolButton* makeToolButton(qfw::FluentIconEnum icon,
                                               const QString& tooltip);
    void setStatus(const QString& text);
    // 统一管理三个动作按钮的启用/禁用状态（互斥禁用）
    void setActionButtonsEnabled(bool enabled);

    void onThemeChanged();
    void onOcrRange();
    void onRangeSelected(const QRect& rect);
    void showRangeOverlay(const QRect& rect);
    void onOcrOnce();
    void onTranslateOnce();
    void onOcrAndTranslate();
    void onScreenOcrStarted();
    void onScreenOcrLog(const QString& line);
    void onScreenOcrFinished(bool success, const QString& text);
    void onScreenTranslateFinished(bool success, const QString& text);

    void onBindWindow();
    void pollPickWindow();
    void finishBind(quintptr hwnd, const QString& title);
    void unbindWindow();
    void traceBoundWindow();

    void onToggleRangeVisible();
    void onCopyResult();
    void onHistory();
    void onHistoryItemClicked(const QString& text);
    void toggleBgTransparent();
    void toggleLock();
    void toggleMouseTransparent();
    void checkTransparentHover();
    void setWin32Transparent(bool on);
    void toggleTopmost();

    qfw::TransparentToolButton* btnMinimize_ = nullptr;
    qfw::TransparentToolButton* btnClose_ = nullptr;
    qfw::TransparentToolButton* btnOcrRange_ = nullptr;
    qfw::TransparentToolButton* btnOcrOnce_ = nullptr;
    qfw::TransparentToolButton* btnTranslate_ = nullptr;
    qfw::TransparentToolButton* btnOcrTranslate_ = nullptr;
    qfw::TransparentToolButton* btnBindWindow_ = nullptr;
    qfw::TransparentToolButton* btnShowRange_ = nullptr;
    qfw::TransparentToolButton* btnCopy_ = nullptr;
    qfw::TransparentToolButton* btnHistory_ = nullptr;
    qfw::TransparentToolButton* btnToggleTop_ = nullptr;
    qfw::TransparentToolButton* btnToggleMouse_ = nullptr;
    qfw::TransparentToolButton* btnBgTransparent_ = nullptr;
    qfw::TransparentToolButton* btnLock_ = nullptr;
    qfw::CaptionLabel* statusLabel_ = nullptr;
    QList<qfw::TransparentToolButton*> toolButtons_;

    QTimer* transparencyTimer_ = nullptr;  // 穿透模式下轮询悬停，临时恢复可点击
    QTimer* traceTimer_ = nullptr;         // 绑定窗口位置轮询
    QTimer* pickTimer_ = nullptr;          // 绑定窗口时轮询鼠标左键

    bool mouseTransparent_ = false;
    bool hasDragPos_ = false;
    QPoint dragPos_;

    QStringList ocrHistory_;  // 上限 50 条
    QString lastOcrText_;
    QString lastTranslateText_;
    bool autoTranslate_ = false;  // OCR 并翻译模式标志

    bool hasOcrRect_ = false;
    QRect ocrRect_;
    bool rangeVisible_ = true;
    QPointer<RangeOverlay> rangeOverlay_;
    QPointer<RangeSelector> rangeSelector_;

    bool isTopmost_ = true;
    bool bgTransparent_ = false;
    bool locked_ = false;

    bool picking_ = false;
    bool pickArmed_ = false;  // 先等本次点击抬起，再等下一次按下，避免误抓自身
    quintptr boundHwnd_ = 0;
    QString boundTitle_;
    bool hasTraceLast_ = false;
    QPoint traceLastTopLeft_;
    QSize traceLastSize_;
    QPoint traceStartPos_;
    QRect traceStartRect_;
    bool hasTraceStartRect_ = false;
};

}  // namespace fkw
