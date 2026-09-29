#include "components/floating_window.h"

#include <QAbstractButton>
#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRubberBand>
#include <QScreen>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

#include <string>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef _MSC_VER
// 鼠标穿透（WS_EX_TRANSPARENT）与窗口绑定（WindowFromPoint / GetWindowRect 等）都走
// user32，显式声明依赖，避免不同生成器默认库集合差异导致链接失败。
#pragma comment(lib, "user32.lib")
#endif
#endif

#include "common/config.h"
#include "common/config_keys.h"
#include "common/event_bus.h"
#include "common/text.h"
#include "common/text_format.h"
#include "service/ocr_service.h"
#include "service/translate_service.h"

namespace fkw {
namespace {

constexpr int kMaxHistory = 50;   // 对应 Python FloatingWindow._ocr_history 上限
constexpr int kPreviewChars = 50; // 状态栏预览截断长度

#ifdef Q_OS_WIN
constexpr int kVkLButton = 0x01;
constexpr UINT kGaRoot = 2;

HWND hwndOf(quintptr value) { return reinterpret_cast<HWND>(value); }
quintptr valueOf(HWND hwnd) { return reinterpret_cast<quintptr>(hwnd); }
#endif

QString number(int value) { return QString::number(value); }

// 状态栏预览：超长只取前 50 字符（对齐 Python text[:50] if len(text) > 50 else text）
QString previewOf(const QString& text) {
    return text.size() > kPreviewChars ? text.left(kPreviewChars) : text;
}

}  // namespace

// ───────────────────────── RangeSelector ─────────────────────────

RangeSelector::RangeSelector(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    // 覆盖整个虚拟桌面，跨屏也能框选（对齐 Python primaryScreen().virtualGeometry()）
    if (QScreen* screen = QGuiApplication::primaryScreen())
        setGeometry(screen->virtualGeometry());
    rubber_ = new QRubberBand(QRubberBand::Rectangle, this);
}

void RangeSelector::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    origin_ = event->position().toPoint();
    rubber_->setGeometry(QRect(origin_, origin_));
    rubber_->show();
}

void RangeSelector::mouseMoveEvent(QMouseEvent* event) {
    if (!rubber_->isVisible()) return;
    rubber_->setGeometry(QRect(origin_, event->position().toPoint()).normalized());
}

void RangeSelector::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    const QRect rect = QRect(origin_, event->position().toPoint()).normalized();
    rubber_->hide();
    close();
    // 先收起遮罩再发信号：接收方可能立刻重新拉起一次框选（选区过小）
    emit rangeSelected(rect);
}

void RangeSelector::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        emit rangeSelected(QRect());
        return;
    }
    QWidget::keyPressEvent(event);
}

void RangeSelector::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 80));
}

// ───────────────────────── RangeOverlay ─────────────────────────

RangeOverlay::RangeOverlay(const QRect& rect, QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(rect);
}

void RangeOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0, 120, 215, 200));
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawRect(0, 0, width() - 1, height() - 1);
    painter.fillRect(rect(), QColor(0, 120, 215, 15));
}

// ───────────────────────── FloatingWindow ─────────────────────────

FloatingWindow::FloatingWindow(QWidget* parent) : QWidget(parent) {
    setWindowIcon(QIcon(QStringLiteral(":/app/images/logo.png")));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    // 关掉即销毁：入口按钮由 ocr_window_closed 恢复，下次点击重新建窗（对齐 Python）
    setAttribute(Qt::WA_DeleteOnClose);
    resize(480, 200);
    setMinimumHeight(120);
    setWindowTitle(Text::instance().FWTitle);

    transparencyTimer_ = new QTimer(this);
    transparencyTimer_->setInterval(100);
    connect(transparencyTimer_, &QTimer::timeout, this, &FloatingWindow::checkTransparentHover);

    traceTimer_ = new QTimer(this);
    traceTimer_->setInterval(200);
    connect(traceTimer_, &QTimer::timeout, this, &FloatingWindow::traceBoundWindow);

    pickTimer_ = new QTimer(this);
    pickTimer_->setInterval(20);
    connect(pickTimer_, &QTimer::timeout, this, &FloatingWindow::pollPickWindow);

    initUi();

    connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged, this,
            [this](qfw::Theme) { onThemeChanged(); });

    auto& bus = GlobalEventBus::instance();
    connect(&bus, &GlobalEventBus::screen_ocr_started, this, &FloatingWindow::onScreenOcrStarted);
    connect(&bus, &GlobalEventBus::screen_ocr_log, this, &FloatingWindow::onScreenOcrLog);
    connect(&bus, &GlobalEventBus::screen_ocr_finished, this,
            &FloatingWindow::onScreenOcrFinished);
    connect(&bus, &GlobalEventBus::screen_translate_finished, this,
            &FloatingWindow::onScreenTranslateFinished);
}

FloatingWindow::~FloatingWindow() = default;

qfw::TransparentToolButton* FloatingWindow::makeToolButton(qfw::FluentIconEnum icon,
                                                           const QString& tooltip) {
    auto* button = new qfw::TransparentToolButton(icon, this);
    button->setFixedSize(28, 28);
    button->setIconSize(QSize(16, 16));
    button->setToolTip(tooltip);
    toolButtons_.append(button);
    return button;
}

void FloatingWindow::initUi() {
    const auto& t = Text::instance();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 14, 10);
    layout->setSpacing(6);

    // 标题行
    auto* header = new QHBoxLayout();
    header->setSpacing(6);
    header->addWidget(new qfw::StrongBodyLabel(t.FWTitle, this));
    header->addStretch();

    btnMinimize_ = makeToolButton(qfw::FluentIconEnum::Minimize, t.Minimize);
    connect(btnMinimize_, &QAbstractButton::clicked, this, &FloatingWindow::showMinimized);
    header->addWidget(btnMinimize_);

    btnClose_ = makeToolButton(qfw::FluentIconEnum::Close, t.Close);
    connect(btnClose_, &QAbstractButton::clicked, this, &FloatingWindow::close);
    header->addWidget(btnClose_);
    layout->addLayout(header);

    // 工具栏按钮行
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);
    toolbar->setContentsMargins(0, 0, 0, 0);

    btnOcrRange_ = makeToolButton(qfw::FluentIconEnum::Code, t.FWSelectRange);
    connect(btnOcrRange_, &QAbstractButton::clicked, this, &FloatingWindow::onOcrRange);
    toolbar->addWidget(btnOcrRange_);

    btnOcrOnce_ = makeToolButton(qfw::FluentIconEnum::Play, t.FWOCROnce);
    connect(btnOcrOnce_, &QAbstractButton::clicked, this, &FloatingWindow::onOcrOnce);
    toolbar->addWidget(btnOcrOnce_);

    btnTranslate_ = makeToolButton(qfw::FluentIconEnum::Message, t.FWTranslate);
    connect(btnTranslate_, &QAbstractButton::clicked, this, &FloatingWindow::onTranslateOnce);
    toolbar->addWidget(btnTranslate_);

    btnOcrTranslate_ = makeToolButton(qfw::FluentIconEnum::Send, t.FWOCRAndTranslate);
    connect(btnOcrTranslate_, &QAbstractButton::clicked, this,
            &FloatingWindow::onOcrAndTranslate);
    toolbar->addWidget(btnOcrTranslate_);

    btnBindWindow_ = makeToolButton(qfw::FluentIconEnum::Link, t.FWBindWindow);
    connect(btnBindWindow_, &QAbstractButton::clicked, this, &FloatingWindow::onBindWindow);
    toolbar->addWidget(btnBindWindow_);

    toolbar->addSpacing(8);

    btnShowRange_ = makeToolButton(qfw::FluentIconEnum::View, t.FWRangeVisible);
    connect(btnShowRange_, &QAbstractButton::clicked, this,
            &FloatingWindow::onToggleRangeVisible);
    toolbar->addWidget(btnShowRange_);

    btnCopy_ = makeToolButton(qfw::FluentIconEnum::Copy, t.FWCopyResult);
    connect(btnCopy_, &QAbstractButton::clicked, this, &FloatingWindow::onCopyResult);
    toolbar->addWidget(btnCopy_);

    btnHistory_ = makeToolButton(qfw::FluentIconEnum::History, t.FWHistory);
    connect(btnHistory_, &QAbstractButton::clicked, this, &FloatingWindow::onHistory);
    toolbar->addWidget(btnHistory_);

    toolbar->addSpacing(8);

    btnToggleTop_ = makeToolButton(qfw::FluentIconEnum::Unpin, t.FWUnpin);
    connect(btnToggleTop_, &QAbstractButton::clicked, this, &FloatingWindow::toggleTopmost);
    toolbar->addWidget(btnToggleTop_);

    btnToggleMouse_ = makeToolButton(qfw::FluentIconEnum::Embed, t.FWMousePass);
    connect(btnToggleMouse_, &QAbstractButton::clicked, this,
            &FloatingWindow::toggleMouseTransparent);
    toolbar->addWidget(btnToggleMouse_);

    btnBgTransparent_ = makeToolButton(qfw::FluentIconEnum::Palette, t.FWBgTransparent);
    connect(btnBgTransparent_, &QAbstractButton::clicked, this,
            &FloatingWindow::toggleBgTransparent);
    toolbar->addWidget(btnBgTransparent_);

    // Python 用 FIF.COMPLETED 表示“未锁定”，C++ 图标集里没有该图，改用锁的开关态
    btnLock_ = makeToolButton(qfw::FluentIconEnum::LockOpen, t.FWLockToolbar);
    connect(btnLock_, &QAbstractButton::clicked, this, &FloatingWindow::toggleLock);
    toolbar->addWidget(btnLock_);

    toolbar->addStretch();
    layout->addLayout(toolbar);

    // 状态栏
    statusLabel_ = new qfw::CaptionLabel(t.FWReady, this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);
}

void FloatingWindow::setStatus(const QString& text) { statusLabel_->setText(text); }

void FloatingWindow::setActionButtonsEnabled(bool enabled) {
    btnOcrOnce_->setEnabled(enabled);
    btnTranslate_->setEnabled(enabled);
    btnOcrTranslate_->setEnabled(enabled);
}

void FloatingWindow::onThemeChanged() {
    statusLabel_->setStyleSheet(qfw::isDarkTheme()
                                    ? QStringLiteral("color: rgba(255,255,255,140);")
                                    : QString());
    update();
}

// ───────── OCR / 翻译 ─────────

void FloatingWindow::onOcrRange() {
    setStatus(Text::instance().FWSelectRangeStatus);
    if (rangeSelector_) {
        rangeSelector_->close();
        rangeSelector_->deleteLater();
        rangeSelector_ = nullptr;
    }
    rangeSelector_ = new RangeSelector(this);
    connect(rangeSelector_, &RangeSelector::rangeSelected, this,
            &FloatingWindow::onRangeSelected);
    rangeSelector_->show();
}

void FloatingWindow::onRangeSelected(const QRect& rect) {
    const auto& t = Text::instance();
    if (rect.isNull()) {
        setStatus(t.FWCancelSelect);
        return;
    }
    if (rect.width() < 10 || rect.height() < 10) {
        setStatus(t.FWRangeTooSmall);
        onOcrRange();
        return;
    }
    ocrRect_ = rect;
    hasOcrRect_ = true;
    setStatus(formatText(t.FWRangeInfo, {number(rect.x()), number(rect.y()),
                                         number(rect.width()), number(rect.height())}));
    if (rangeVisible_) showRangeOverlay(rect);
}

void FloatingWindow::showRangeOverlay(const QRect& rect) {
    if (rangeOverlay_) {
        rangeOverlay_->close();
        rangeOverlay_->deleteLater();
        rangeOverlay_ = nullptr;
    }
    rangeOverlay_ = new RangeOverlay(rect, this);
    rangeOverlay_->show();
}

void FloatingWindow::onOcrOnce() {
    if (!hasOcrRect_) {
        setStatus(Text::instance().FWSelectRangeFirst);
        return;
    }
    setActionButtonsEnabled(false);
    setStatus(Text::instance().FWOCRRunning);
    // ScreenOcrRunner 自回收（setAutoDelete(false) + run() 末尾 deleteLater）
    QThreadPool::globalInstance()->start(new ScreenOcrRunner(ocrRect_));
}

void FloatingWindow::onTranslateOnce() {
    if (lastOcrText_.isEmpty()) {
        setStatus(Text::instance().FWNoTextToTranslate);
        return;
    }
    setActionButtonsEnabled(false);
    setStatus(Text::instance().FWTranslating);
    QThreadPool::globalInstance()->start(new ScreenTranslateRunner(lastOcrText_));
}

void FloatingWindow::onOcrAndTranslate() {
    if (!hasOcrRect_) {
        setStatus(Text::instance().FWSelectRangeFirst);
        return;
    }
    autoTranslate_ = true;
    setActionButtonsEnabled(false);
    setStatus(Text::instance().FWOCRRunning);
    QThreadPool::globalInstance()->start(new ScreenOcrRunner(ocrRect_));
}

void FloatingWindow::onScreenOcrStarted() {
    setStatus(Text::instance().FWOCRInProgress);
}

void FloatingWindow::onScreenOcrLog(const QString& line) { setStatus(line); }

void FloatingWindow::onScreenOcrFinished(bool success, const QString& text) {
    const auto& t = Text::instance();
    if (success) {
        lastOcrText_ = text;
        ocrHistory_.append(text);
        if (ocrHistory_.size() > kMaxHistory) ocrHistory_.removeFirst();
        setStatus(formatText(t.FWOCRComplete, {previewOf(text)}));
        // OCR 并翻译模式：OCR 成功后自动触发翻译（按钮继续禁用，翻译完成时恢复）
        if (autoTranslate_ && !text.isEmpty()) {
            autoTranslate_ = false;
            onTranslateOnce();
            return;
        }
        // 普通 OCR 模式，或 OCR 成功但无文本：恢复按钮
        autoTranslate_ = false;
        setActionButtonsEnabled(true);
    } else {
        autoTranslate_ = false;
        setActionButtonsEnabled(true);
        setStatus(formatText(t.FWOCRFailed, {text}));
    }
}

void FloatingWindow::onScreenTranslateFinished(bool success, const QString& text) {
    const auto& t = Text::instance();
    if (success) {
        lastTranslateText_ = text;
        setStatus(formatText(t.FWTranslateComplete, {previewOf(text)}));
    } else {
        setStatus(formatText(t.TextAuto060, {text}));
    }
    // 线程结束即恢复动作按钮（对齐 Python _on_translate_thread_finished）
    setActionButtonsEnabled(true);
}

// ───────── 窗口绑定 / 跟随 ─────────

void FloatingWindow::onBindWindow() {
    const auto& t = Text::instance();
#ifndef Q_OS_WIN
    setStatus(t.FWWindowsOnly);
#else
    if (boundHwnd_ != 0) {
        unbindWindow();
        return;
    }
    setStatus(t.FWClickToBind);
    picking_ = true;
    // 点“绑定窗口”按钮时左键可能还没抬起：先等它抬起（pickArmed_），再等下一次按下
    pickArmed_ = false;
    btnBindWindow_->setEnabled(false);
    pickTimer_->start();
#endif
}

void FloatingWindow::pollPickWindow() {
#ifdef Q_OS_WIN
    if (!picking_) {
        pickTimer_->stop();
        return;
    }
    const bool down = (GetAsyncKeyState(kVkLButton) & 0x8000) != 0;
    if (!pickArmed_) {
        if (!down) pickArmed_ = true;
        return;
    }
    if (!down) return;

    POINT point{};
    if (!GetCursorPos(&point)) {
        finishBind(0, QString());
        return;
    }
    HWND hwnd = WindowFromPoint(point);
    quintptr value = 0;
    QString title;
    if (hwnd) {
        const HWND root = GetAncestor(hwnd, kGaRoot);
        if (root) hwnd = root;
        value = valueOf(hwnd);
        const int length = GetWindowTextLengthW(hwnd);
        if (length > 0) {
            std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
            const int copied = GetWindowTextW(hwnd, buffer.data(), length + 1);
            if (copied > 0) title = QString::fromWCharArray(buffer.data(), copied);
        }
    }
    finishBind(value, title);
#else
    pickTimer_->stop();
#endif
}

void FloatingWindow::finishBind(quintptr hwnd, const QString& title) {
    const auto& t = Text::instance();
    pickTimer_->stop();
    picking_ = false;
    pickArmed_ = false;
    btnBindWindow_->setEnabled(true);
    if (hwnd == 0) {
        setStatus(t.FWCancelBind);
        return;
    }
    boundHwnd_ = hwnd;
    boundTitle_ = title;
    hasTraceLast_ = false;
    hasTraceStartRect_ = false;
    btnBindWindow_->setToolTip(formatText(t.FWBindWindowWith, {title}));
    setStatus(formatText(t.FWBoundTo, {title}));
    traceTimer_->start();
}

void FloatingWindow::unbindWindow() {
    const auto& t = Text::instance();
    boundHwnd_ = 0;
    boundTitle_.clear();
    hasTraceLast_ = false;
    hasTraceStartRect_ = false;
    traceTimer_->stop();
    btnBindWindow_->setToolTip(t.FWBindWindow);
    setStatus(t.FWUnbound);
}

void FloatingWindow::traceBoundWindow() {
#ifdef Q_OS_WIN
    if (boundHwnd_ == 0) return;
    RECT rect{};
    if (!GetWindowRect(hwndOf(boundHwnd_), &rect)) return;
    if (rect.left == 0 && rect.top == 0) return;
    const qreal dpr = devicePixelRatioF();
    const QPoint currentTopLeft(static_cast<int>(rect.left / dpr),
                                static_cast<int>(rect.top / dpr));
    const QSize currentSize(static_cast<int>((rect.right - rect.left) / dpr),
                            static_cast<int>((rect.bottom - rect.top) / dpr));

    if (!hasTraceLast_) {
        traceLastTopLeft_ = currentTopLeft;
        traceLastSize_ = currentSize;
        hasTraceLast_ = true;
        traceStartPos_ = currentTopLeft;
        if (hasOcrRect_) {
            traceStartRect_ = ocrRect_;
            hasTraceStartRect_ = true;
        }
        return;
    }
    // 尺寸变化说明窗口被缩放/切换了布局：重置基线，不跟随平移
    if (currentSize != traceLastSize_) {
        traceLastTopLeft_ = currentTopLeft;
        traceLastSize_ = currentSize;
        traceStartPos_ = currentTopLeft;
        if (hasOcrRect_) {
            traceStartRect_ = ocrRect_;
            hasTraceStartRect_ = true;
        }
        return;
    }
    if (currentTopLeft == traceLastTopLeft_) return;
    traceLastTopLeft_ = currentTopLeft;

    if (!hasOcrRect_ || !hasTraceStartRect_) return;
    const QPoint delta = currentTopLeft - traceStartPos_;
    const QRect newRect = traceStartRect_.translated(delta);
    ocrRect_ = newRect;
    if (rangeOverlay_) rangeOverlay_->setGeometry(newRect);
    setStatus(formatText(Text::instance().FWRangeFollow,
                         {number(newRect.x()), number(newRect.y()), number(newRect.width()),
                          number(newRect.height())}));
#endif
}

// ───────── 显示控制 / 复制 / 历史 ─────────

void FloatingWindow::onToggleRangeVisible() {
    const auto& t = Text::instance();
    rangeVisible_ = !rangeVisible_;
    btnShowRange_->setIcon(
        qfw::FluentIcon(rangeVisible_ ? qfw::FluentIconEnum::View : qfw::FluentIconEnum::Hide)
            .qicon());
    if (rangeVisible_ && hasOcrRect_) {
        showRangeOverlay(ocrRect_);
    } else if (!rangeVisible_ && rangeOverlay_) {
        rangeOverlay_->close();
        rangeOverlay_->deleteLater();
        rangeOverlay_ = nullptr;
    }
    setStatus(rangeVisible_ ? t.FWRangeShowOn : t.FWRangeShowOff);
}

void FloatingWindow::onCopyResult() {
    const auto& t = Text::instance();
    const QString copyText = !lastTranslateText_.isEmpty() ? lastTranslateText_ : lastOcrText_;
    if (copyText.isEmpty()) {
        setStatus(t.FWNoResultToCopy);
        return;
    }
    QGuiApplication::clipboard()->setText(copyText);
    setStatus(t.FWCopiedToClipboard);
}

void FloatingWindow::onHistory() {
    const auto& t = Text::instance();
    if (ocrHistory_.isEmpty()) {
        setStatus(t.FWNoHistory);
        return;
    }
    auto* menu = new qfw::RoundMenu(QString(), this);
    const int total = ocrHistory_.size();
    for (int i = 0; i < total; ++i) {
        // 菜单里最新一条在最上面，编号与 Python 一致（从 total 递减到 1）
        const QString text = ocrHistory_.at(total - 1 - i);
        QString flat = text;
        flat.replace(QLatin1Char('\n'), QLatin1Char(' '));
        QString preview = flat.left(10);
        if (flat.size() > 10) preview += QStringLiteral("…    ");
        auto* action = new QAction(QStringLiteral("%1. %2").arg(total - i).arg(preview), menu);
        // 与 project_card 一致：RoundMenu 在自身事件分发栈里关闭并 deleteLater，
        // 回调统一延后一个事件循环，等菜单完全退出后再改状态/写剪贴板。
        connect(action, &QAction::triggered, this, [this, text]() {
            QTimer::singleShot(0, this, [this, text]() { onHistoryItemClicked(text); });
        });
        menu->addAction(action);
    }
    connect(menu, &qfw::RoundMenu::closedSignal, menu, &QObject::deleteLater);
    menu->execAt(QCursor::pos());
}

void FloatingWindow::onHistoryItemClicked(const QString& text) {
    lastOcrText_ = text;
    QGuiApplication::clipboard()->setText(text);
    setStatus(formatText(Text::instance().FWCopiedFromHistory, {text.left(30)}));
}

void FloatingWindow::toggleBgTransparent() {
    const auto& t = Text::instance();
    bgTransparent_ = !bgTransparent_;
    btnBgTransparent_->setIcon(
        qfw::FluentIcon(bgTransparent_ ? qfw::FluentIconEnum::Brush : qfw::FluentIconEnum::Palette)
            .qicon());
    update();
    setStatus(bgTransparent_ ? t.FWBgTransparentOn : t.FWBgTransparentOff);
}

void FloatingWindow::toggleLock() {
    const auto& t = Text::instance();
    locked_ = !locked_;
    btnLock_->setIcon(
        qfw::FluentIcon(locked_ ? qfw::FluentIconEnum::LockClosed : qfw::FluentIconEnum::LockOpen)
            .qicon());
    for (auto* button : toolButtons_) {
        if (button == btnLock_ || button == btnClose_) continue;
        button->setEnabled(!locked_);
    }
    setStatus(locked_ ? t.FWToolbarLocked : t.FWToolbarUnlocked);
}

// ───────── 背景绘制 ─────────

void FloatingWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int radius = 8;
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), radius, radius);
    painter.setClipPath(path);

    const int bgAlpha = bgTransparent_ ? 80 : 220;
    QPen pen;
    if (qfw::isDarkTheme()) {
        painter.fillRect(rect(), QColor(32, 32, 40, bgAlpha));
        pen = QPen(QColor(70, 130, 220, 120));
    } else {
        painter.fillRect(rect(), QColor(255, 255, 255, qMax(bgAlpha - 70, 0)));
        pen = QPen(QColor(70, 130, 220, 100));
    }
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawPath(path);
}

// ───────── 拖拽 ─────────

void FloatingWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    dragPos_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    hasDragPos_ = true;
}

void FloatingWindow::mouseMoveEvent(QMouseEvent* event) {
    if (!hasDragPos_ || !(event->buttons() & Qt::LeftButton)) return;
    move(event->globalPosition().toPoint() - dragPos_);
}

void FloatingWindow::mouseReleaseEvent(QMouseEvent*) { hasDragPos_ = false; }

// ───────── 鼠标穿透 ─────────

void FloatingWindow::setWin32Transparent(bool on) {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (on)
        style |= WS_EX_TRANSPARENT;
    else
        style &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
#else
    Q_UNUSED(on);
#endif
}

void FloatingWindow::toggleMouseTransparent() {
    const auto& t = Text::instance();
    mouseTransparent_ = !mouseTransparent_;
    if (mouseTransparent_) {
        setWin32Transparent(true);
        transparencyTimer_->start();
        btnToggleMouse_->setIcon(
            qfw::FluentIcon(qfw::FluentIconEnum::Cancel).qicon());
        setStatus(t.FWMousePassOn);
    } else {
        transparencyTimer_->stop();
        setWin32Transparent(false);
        btnToggleMouse_->setIcon(
            qfw::FluentIcon(qfw::FluentIconEnum::Embed).qicon());
        setStatus(t.FWMousePassOff);
    }
}

void FloatingWindow::checkTransparentHover() {
    // 穿透模式下鼠标悬停在可用按钮上时临时取消穿透，保证仍然能点
    const QPoint cursorPos = mapFromGlobal(QCursor::pos());
    bool onButton = false;
    for (auto* button : toolButtons_) {
        if (button->isVisible() && button->isEnabled() &&
            button->geometry().contains(cursorPos)) {
            onButton = true;
            break;
        }
    }
    setWin32Transparent(!onButton);
}

// ───────── 置顶切换 ─────────

void FloatingWindow::toggleTopmost() {
    const auto& t = Text::instance();
    const Qt::WindowFlags flags = windowFlags();
    if (isTopmost_) {
        setWindowFlags(flags & ~Qt::WindowStaysOnTopHint);
        btnToggleTop_->setIcon(qfw::FluentIcon(qfw::FluentIconEnum::Pin).qicon());
        btnToggleTop_->setToolTip(t.PinToTop);
        setStatus(t.FWTopOff);
    } else {
        setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        btnToggleTop_->setIcon(qfw::FluentIcon(qfw::FluentIconEnum::Unpin).qicon());
        btnToggleTop_->setToolTip(t.FWUnpin);
        setStatus(t.FWTopOn);
    }
    isTopmost_ = !isTopmost_;
    // setWindowFlags 会重建原生窗口，扩展样式被重置：穿透状态下需重新套用
    show();
    if (mouseTransparent_) setWin32Transparent(true);
}

void FloatingWindow::closeEvent(QCloseEvent* event) {
    traceTimer_->stop();
    pickTimer_->stop();
    transparencyTimer_->stop();
    picking_ = false;
    if (rangeOverlay_) {
        rangeOverlay_->close();
        rangeOverlay_->deleteLater();
        rangeOverlay_ = nullptr;
    }
    if (rangeSelector_) {
        rangeSelector_->close();
        rangeSelector_->deleteLater();
        rangeSelector_ = nullptr;
    }
    // 通知主页恢复悬浮窗口入口按钮（对齐 Python event_bus.ocr_window_closed）
    emit GlobalEventBus::instance().ocr_window_closed();
    QWidget::closeEvent(event);
}

}  // namespace fkw
