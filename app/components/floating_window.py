import ctypes
import sys

IS_WIN = sys.platform == 'win32'

if IS_WIN:
    import ctypes.wintypes  # noqa: F811

from PySide6.QtCore import (
    Q_ARG,
    QMetaObject,
    QPoint,
    QRect,
    QSize,
    Qt,
    QTimer,
    Signal,
    Slot,
)
from PySide6.QtGui import QColor, QCursor, QIcon, QPainter, QPainterPath, QPen
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QRubberBand,
    QVBoxLayout,
    QWidget,
)
from qfluentwidgets import (
    CaptionLabel,
    StrongBodyLabel,
    TransparentToolButton,
    isDarkTheme,
    qconfig,
)
from qfluentwidgets import (
    FluentIcon as FIF,
)

from ..common.event_bus import event_bus
from ..common.text import Text

if IS_WIN:
    GWL_EXSTYLE = -20
    WS_EX_TRANSPARENT = 0x00000020

    user32 = ctypes.windll.user32
    user32.SetWindowLongW.restype = ctypes.c_long
    user32.SetWindowLongW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_long]
    user32.GetWindowLongW.restype = ctypes.c_long
    user32.GetWindowLongW.argtypes = [ctypes.c_void_p, ctypes.c_int]
    user32.GetWindowTextW.restype = ctypes.c_int
    user32.GetWindowTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int]
    user32.GetWindowTextLengthW.restype = ctypes.c_int
    user32.GetWindowTextLengthW.argtypes = [ctypes.c_void_p]
    user32.WindowFromPoint.restype = ctypes.c_void_p
    user32.WindowFromPoint.argtypes = [ctypes.wintypes.POINT]
    user32.GetWindowRect.restype = ctypes.c_int
    user32.GetWindowRect.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.wintypes.RECT)]
    user32.GetForegroundWindow.restype = ctypes.c_void_p
    user32.GetCursorPos.argtypes = [ctypes.POINTER(ctypes.wintypes.POINT)]
    user32.GetCursorPos.restype = ctypes.c_int
    user32.GetKeyState.argtypes = [ctypes.c_int]
    user32.GetKeyState.restype = ctypes.c_short
    user32.GetAncestor.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    user32.GetAncestor.restype = ctypes.c_void_p
    user32.GetWindowThreadProcessId.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint),
    ]
    user32.GetWindowThreadProcessId.restype = ctypes.c_uint
else:
    user32 = None

VK_LBUTTON = 0x01
GA_ROOT = 2


class FloatingWindow(QWidget):
    """悬浮窗口组件

    特性：
    - 无边框、置顶、半透明背景
    - 可拖拽移动
    - 可切换鼠标穿透
    - 可切换置顶
    """

    _PADDING = 8

    def __init__(self, parent=None):
        super().__init__(parent)
        self.t = Text()
        self.setWindowIcon(QIcon(":/app/images/logo.png"))
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Window
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.resize(480, 200)
        self.setMinimumHeight(120)
        self._mouse_transparent = False
        self._drag_pos = None
        self._tool_buttons = []
        self._ocr_history = []
        self._last_ocr_text = ""
        self._last_translate_text = ""
        self._auto_translate = False  # OCR并翻译模式标志
        self._bound_hwnd = None
        self._bound_title = ""
        self._trace_last_rect = None
        self._trace_start_pos = None
        self._trace_start_rect = None
        self._transparency_timer = QTimer(self)
        self._transparency_timer.setInterval(100)
        self._transparency_timer.timeout.connect(self._check_transparent_hover)

        self._trace_timer = QTimer(self)
        self._trace_timer.setInterval(200)
        self._trace_timer.timeout.connect(self._trace_bound_window)

        self._init_ui()
        qconfig.themeChanged.connect(self._on_theme_changed)
        event_bus.screen_ocr_started.connect(self._on_screen_ocr_started)
        event_bus.screen_ocr_log.connect(self._on_screen_ocr_log)
        event_bus.screen_ocr_finished.connect(self._on_screen_ocr_finished)
        event_bus.screen_translate_finished.connect(self._on_screen_translate_finished)

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 14, 10)
        layout.setSpacing(6)

        # 标题行
        header = QHBoxLayout()
        header.setSpacing(6)

        title = StrongBodyLabel(self.t.FWTitle)
        header.addWidget(title)

        header.addStretch()

        self.btn_minimize = self._make_tool_btn(
            FIF.MINIMIZE, self.t.Minimize, self.showMinimized
        )
        header.addWidget(self.btn_minimize)

        self.btn_close = self._make_tool_btn(FIF.CLOSE, self.t.Close, self.close)
        header.addWidget(self.btn_close)

        layout.addLayout(header)

        # 工具栏按钮行
        toolbar = QHBoxLayout()
        toolbar.setSpacing(4)
        toolbar.setContentsMargins(0, 0, 0, 0)

        # OCR 相关
        self.btn_ocr_range = self._make_tool_btn(
            FIF.CODE, self.t.FWSelectRange, self._on_ocr_range
        )
        toolbar.addWidget(self.btn_ocr_range)

        self.btn_ocr_once = self._make_tool_btn(
            FIF.PLAY, self.t.FWOCROnce, self._on_ocr_once
        )
        toolbar.addWidget(self.btn_ocr_once)

        self.btn_translate = self._make_tool_btn(
            FIF.MESSAGE, self.t.FWTranslate, self._on_translate_once
        )
        toolbar.addWidget(self.btn_translate)

        self.btn_ocr_translate = self._make_tool_btn(
            FIF.SEND, self.t.FWOCRAndTranslate, self._on_ocr_and_translate
        )
        toolbar.addWidget(self.btn_ocr_translate)

        self.btn_bind_window = self._make_tool_btn(
            FIF.LINK, self.t.FWBindWindow, self._on_bind_window
        )
        toolbar.addWidget(self.btn_bind_window)

        toolbar.addSpacing(8)

        # 显示控制
        self._range_visible = True
        self.btn_show_range = self._make_tool_btn(
            FIF.VIEW, self.t.FWRangeVisible, self._on_toggle_range_visible
        )
        toolbar.addWidget(self.btn_show_range)

        self.btn_copy = self._make_tool_btn(
            FIF.COPY, self.t.FWCopyResult, self._on_copy_result
        )
        toolbar.addWidget(self.btn_copy)

        self.btn_history = self._make_tool_btn(
            FIF.HISTORY, self.t.FWHistory, self._on_history
        )
        toolbar.addWidget(self.btn_history)

        toolbar.addSpacing(8)

        # 窗口控制
        self._is_topmost = True
        self.btn_toggle_top = self._make_tool_btn(
            FIF.UNPIN, self.t.FWUnpin, self._toggle_topmost
        )
        toolbar.addWidget(self.btn_toggle_top)

        self.btn_toggle_mouse = self._make_tool_btn(
            FIF.EMBED, self.t.FWMousePass, self._toggle_mouse_transparent
        )
        toolbar.addWidget(self.btn_toggle_mouse)

        self.btn_bg_transparent = self._make_tool_btn(
            FIF.PALETTE, self.t.FWBgTransparent, self._toggle_bg_transparent
        )
        toolbar.addWidget(self.btn_bg_transparent)

        self.btn_lock = self._make_tool_btn(
            FIF.COMPLETED, self.t.FWLockToolbar, self._toggle_lock
        )
        toolbar.addWidget(self.btn_lock)

        toolbar.addStretch()

        layout.addLayout(toolbar)

        # 状态栏
        self.status_label = CaptionLabel(self.t.FWReady)
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

    def _on_theme_changed(self, theme):
        """主题切换时更新状态栏颜色并重绘"""
        if isDarkTheme():
            self.status_label.setStyleSheet("color: rgba(255,255,255,140);")
        else:
            self.status_label.setStyleSheet("")
        self.update()

    def _make_tool_btn(self, icon, tooltip, callback):
        btn = TransparentToolButton(icon)
        btn.setFixedSize(28, 28)
        btn.setIconSize(QSize(16, 16))
        btn.setToolTip(tooltip)
        btn.clicked.connect(callback)
        self._tool_buttons.append(btn)
        return btn

    def _set_status(self, text: str):
        self.status_label.setText(text)

    def _set_action_buttons_enabled(self, enabled: bool):
        """统一管理三个动作按钮的启用/禁用状态（互斥禁用）"""
        self.btn_ocr_once.setEnabled(enabled)
        self.btn_translate.setEnabled(enabled)
        self.btn_ocr_translate.setEnabled(enabled)

    # --- OCR 功能（占位） ---
    def _on_ocr_range(self):
        self._set_status(self.t.FWSelectRangeStatus)
        self._range_selector = RangeSelector(self)
        self._range_selector.range_selected.connect(self._on_range_selected)
        self._range_selector.show()

    def _on_range_selected(self, rect: QRect):
        if rect.isNull():
            self._set_status(self.t.FWCancelSelect)
            return
        if rect.width() < 10 or rect.height() < 10:
            self._set_status(self.t.FWRangeTooSmall)
            self._on_ocr_range()
            return
        self._ocr_rect = rect
        self._set_status(
            self.t.FWRangeInfo.format(rect.x(), rect.y(), rect.width(), rect.height())
        )
        if self._range_visible:
            self._show_range_overlay(rect)

    def _show_range_overlay(self, rect: QRect):
        if hasattr(self, "_range_overlay") and self._range_overlay:
            self._range_overlay.close()
        self._range_overlay = RangeOverlay(rect)
        self._range_overlay.show()

    def _on_ocr_once(self):
        if not hasattr(self, "_ocr_rect") or not self._ocr_rect:
            self._set_status(self.t.FWSelectRangeFirst)
            return
        self._set_action_buttons_enabled(False)
        self._set_status(self.t.FWOCRRunning)

        from ..service.ocr_service import ScreenOCRThread

        self._ocr_thread = ScreenOCRThread(self._ocr_rect)
        self._ocr_thread.finished.connect(self._on_ocr_thread_finished)
        self._ocr_thread.start()

    def _on_ocr_thread_finished(self):
        # 仅清理线程引用，按钮恢复统一由 _on_screen_ocr_finished /
        # _on_translate_thread_finished 处理
        # （跨线程信号入队顺序：screen_ocr_finished 先于 QThread.finished，
        #  若在此处根据 _auto_translate 判断会因已被重置而错误恢复按钮）
        self._ocr_thread = None

    def _on_translate_once(self):
        if not self._last_ocr_text:
            self._set_status(self.t.FWNoTextToTranslate)
            return
        self._set_action_buttons_enabled(False)
        self._set_status(self.t.FWTranslating)

        from ..service.translate_service import ScreenTranslateThread

        self._translate_thread = ScreenTranslateThread(self._last_ocr_text)
        self._translate_thread.finished.connect(self._on_translate_thread_finished)
        self._translate_thread.start()

    def _on_ocr_and_translate(self):
        """OCR完成后自动翻译"""
        if not hasattr(self, "_ocr_rect") or not self._ocr_rect:
            self._set_status(self.t.FWSelectRangeFirst)
            return
        self._auto_translate = True
        self._set_action_buttons_enabled(False)
        self._set_status(self.t.FWOCRRunning)

        from ..service.ocr_service import ScreenOCRThread

        self._ocr_thread = ScreenOCRThread(self._ocr_rect)
        self._ocr_thread.finished.connect(self._on_ocr_thread_finished)
        self._ocr_thread.start()

    def _on_translate_thread_finished(self):
        self._set_action_buttons_enabled(True)
        self._translate_thread = None

    def _on_screen_translate_finished(self, success: bool, text: str):
        if success:
            self._last_translate_text = text
            self._set_status(
                self.t.FWTranslateComplete.format(text[:50])
                if len(text) > 50
                else self.t.FWTranslateComplete.format(text)
            )
        else:
            self._set_status(self.t.TextAuto060.format(text))

    def _on_screen_ocr_started(self):
        self._set_status(self.t.FWOCRInProgress)

    def _on_screen_ocr_log(self, text: str):
        self._set_status(text)

    def _on_screen_ocr_finished(self, success: bool, text: str):
        if success:
            self._last_ocr_text = text
            self._ocr_history.append(text)
            if len(self._ocr_history) > 50:
                self._ocr_history.pop(0)
            self._set_status(
                self.t.FWOCRComplete.format(text[:50])
                if len(text) > 50
                else self.t.FWOCRComplete.format(text)
            )
            # OCR并翻译模式：OCR成功后自动触发翻译（按钮继续禁用，由翻译完成时恢复）
            if self._auto_translate and text:
                self._auto_translate = False
                self._on_translate_once()
                return
            # 普通 OCR 模式或 OCR 成功但无文本：恢复按钮
            self._auto_translate = False
            self._set_action_buttons_enabled(True)
        else:
            self._auto_translate = False
            self._set_action_buttons_enabled(True)
            self._set_status(self.t.FWOCRFailed.format(text))

    def _on_grab_window(self):
        pass

    def _on_bind_window(self):
        if not IS_WIN:
            self._set_status(self.t.FWWindowsOnly)
            return
        if self._bound_hwnd:
            self._unbind_window()
            return
        self._set_status(self.t.FWClickToBind)
        self._picking = True
        self.btn_bind_window.setEnabled(False)

        import threading

        def _pick_loop():
            import time

            while self._picking:
                if user32.GetKeyState(VK_LBUTTON) < 0:
                    break
                time.sleep(0.01)
            if not self._picking:
                return
            try:
                pt = ctypes.wintypes.POINT()
                user32.GetCursorPos(ctypes.byref(pt))
                hwnd = user32.WindowFromPoint(pt)
                if hwnd:
                    hwnd = user32.GetAncestor(hwnd, GA_ROOT) or hwnd
                    length = user32.GetWindowTextLengthW(hwnd)
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(hwnd, buf, length + 1)
                    title = buf.value
                else:
                    title = ""
                QMetaObject.invokeMethod(
                    self,
                    "_on_window_picked_qt",
                    Qt.ConnectionType.QueuedConnection,
                    Q_ARG(int, int(hwnd) if hwnd else 0),
                    Q_ARG(str, title),
                )
            except Exception:
                pass

        threading.Thread(target=_pick_loop, daemon=True).start()

    @Slot(int, str)
    def _on_window_picked_qt(self, hwnd: int, title: str):
        self._picking = False
        self.btn_bind_window.setEnabled(True)
        if hwnd == 0:
            self._set_status(self.t.FWCancelBind)
            return
        self._bound_hwnd = hwnd
        self._bound_title = title
        self._trace_last_rect = None
        self._trace_start_pos = None
        self._trace_start_rect = None
        self.btn_bind_window.setToolTip(self.t.FWBindWindowWith.format(title))
        self._set_status(self.t.FWBoundTo.format(title))
        self._trace_timer.start()

    def _unbind_window(self):
        self._bound_hwnd = None
        self._bound_title = ""
        self._trace_last_rect = None
        self._trace_start_pos = None
        self._trace_start_rect = None
        self._trace_timer.stop()
        self.btn_bind_window.setToolTip(self.t.FWBindWindow)
        self._set_status(self.t.FWUnbound)

    def _trace_bound_window(self):
        if not IS_WIN or not self._bound_hwnd:
            return
        try:
            rect = ctypes.wintypes.RECT()
            user32.GetWindowRect(self._bound_hwnd, ctypes.byref(rect))
            if rect.left == 0 and rect.top == 0:
                return
            dpr = self.devicePixelRatioF()
            curr_topleft = QPoint(
                int(rect.left / dpr),
                int(rect.top / dpr),
            )
            curr_size = (
                int((rect.right - rect.left) / dpr),
                int((rect.bottom - rect.top) / dpr),
            )
        except Exception:
            return
        if self._trace_last_rect is None:
            self._trace_last_rect = (curr_topleft, curr_size)
            self._trace_start_pos = curr_topleft
            if hasattr(self, "_ocr_rect") and self._ocr_rect:
                self._trace_start_rect = QRect(self._ocr_rect)
            return
        last_topleft, last_size = self._trace_last_rect
        if curr_size != last_size:
            self._trace_last_rect = (curr_topleft, curr_size)
            self._trace_start_pos = curr_topleft
            if hasattr(self, "_ocr_rect") and self._ocr_rect:
                self._trace_start_rect = QRect(self._ocr_rect)
            return
        if curr_topleft == last_topleft:
            return
        self._trace_last_rect = (curr_topleft, curr_size)
        if not hasattr(self, "_ocr_rect") or not self._ocr_rect:
            return
        if self._trace_start_rect is None:
            return
        delta = curr_topleft - self._trace_start_pos
        new_rect = self._trace_start_rect.translated(delta)
        self._ocr_rect = new_rect
        if hasattr(self, "_range_overlay") and self._range_overlay:
            self._range_overlay.setGeometry(new_rect)
        self._set_status(
            self.t.FWRangeFollow.format(
                new_rect.x(), new_rect.y(), new_rect.width(), new_rect.height()
            )
        )

    def _on_toggle_range_visible(self):
        self._range_visible = not self._range_visible
        self.btn_show_range.setIcon(FIF.VIEW if self._range_visible else FIF.HIDE)
        if self._range_visible and hasattr(self, "_ocr_rect") and self._ocr_rect:
            self._show_range_overlay(self._ocr_rect)
        elif (
            not self._range_visible
            and hasattr(self, "_range_overlay")
            and self._range_overlay
        ):
            self._range_overlay.close()
            self._range_overlay = None
        self._set_status(
            self.t.FWRangeShowOn if self._range_visible else self.t.FWRangeShowOff
        )

    def _on_copy_result(self):
        copy_text = self._last_translate_text or self._last_ocr_text
        if not copy_text:
            self._set_status(self.t.FWNoResultToCopy)
            return
        clipboard = QApplication.clipboard()
        clipboard.setText(copy_text)
        self._set_status(self.t.FWCopiedToClipboard)

    def _on_history(self):
        if not self._ocr_history:
            self._set_status(self.t.FWNoHistory)
            return
        from qfluentwidgets import Action, RoundMenu

        menu = RoundMenu(parent=self)
        for i, text in enumerate(reversed(self._ocr_history)):
            preview = text.replace("\n", " ")[:10]
            if len(text.replace("\n", " ")) > 10:
                preview += "…    "
            act = Action(f"{len(self._ocr_history) - i}. {preview}")
            act.triggered.connect(
                lambda checked, t=text: self._on_history_item_clicked(t)
            )
            menu.addAction(act)
        menu.exec(QCursor.pos())

    def _on_history_item_clicked(self, text: str):
        self._last_ocr_text = text
        clipboard = QApplication.clipboard()
        clipboard.setText(text)
        self._set_status(self.t.FWCopiedFromHistory.format(text[:30]))

    def _toggle_bg_transparent(self):
        self._bg_transparent = not getattr(self, "_bg_transparent", False)
        self.btn_bg_transparent.setIcon(
            FIF.PALETTE if not self._bg_transparent else FIF.BRUSH
        )
        self.update()
        self._set_status(
            self.t.FWBgTransparentOn
            if self._bg_transparent
            else self.t.FWBgTransparentOff
        )

    def _toggle_lock(self):
        self._locked = not getattr(self, "_locked", False)
        self.btn_lock.setIcon(FIF.COMPLETED if not self._locked else FIF.CALENDAR)
        for btn in self._tool_buttons:
            if btn not in (self.btn_lock, self.btn_close):
                btn.setEnabled(not self._locked)
        self._set_status(
            self.t.FWToolbarLocked if self._locked else self.t.FWToolbarUnlocked
        )

    # --- 背景绘制 ---
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        w, h = self.width(), self.height()
        radius = 8

        path = QPainterPath()
        path.addRoundedRect(0, 0, w, h, radius, radius)
        painter.setClipPath(path)

        bg_alpha = 80 if getattr(self, "_bg_transparent", False) else 220
        if isDarkTheme():
            painter.fillRect(self.rect(), QColor(32, 32, 40, bg_alpha))
            pen = QPen(QColor(70, 130, 220, 120))
        else:
            painter.fillRect(self.rect(), QColor(255, 255, 255, max(bg_alpha - 70, 0)))
            pen = QPen(QColor(70, 130, 220, 100))
        pen.setWidth(1)
        painter.setPen(pen)
        painter.drawPath(path)

    # --- 拖拽 ---
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_pos = (
                event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            )

    def mouseMoveEvent(self, event):
        if self._drag_pos and event.buttons() & Qt.MouseButton.LeftButton:
            self.move(event.globalPosition().toPoint() - self._drag_pos)

    def mouseReleaseEvent(self, event):
        self._drag_pos = None

    # --- 鼠标穿透 ---
    def _set_win32_transparent(self, on: bool):
        if not IS_WIN:
            return
        hwnd = int(self.winId())
        if on:
            user32.SetWindowLongW(
                hwnd,
                GWL_EXSTYLE,
                user32.GetWindowLongW(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT,
            )
        else:
            user32.SetWindowLongW(
                hwnd,
                GWL_EXSTYLE,
                user32.GetWindowLongW(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT,
            )

    def _toggle_mouse_transparent(self):
        self._mouse_transparent = not self._mouse_transparent
        if self._mouse_transparent:
            self._set_win32_transparent(True)
            self._transparency_timer.start()
            self.btn_toggle_mouse.setIcon(FIF.CANCEL)
            self._set_status(self.t.FWMousePassOn)
        else:
            self._transparency_timer.stop()
            self._set_win32_transparent(False)
            self.btn_toggle_mouse.setIcon(FIF.EMBED)
            self._set_status(self.t.FWMousePassOff)

    def _check_transparent_hover(self):
        """穿透模式下，鼠标悬停在按钮区域时临时取消穿透，允许点击"""
        cursor_pos = self.mapFromGlobal(QCursor.pos())
        on_button = any(
            btn.geometry().contains(cursor_pos)
            for btn in self._tool_buttons
            if btn.isVisible() and btn.isEnabled()
        )
        self._set_win32_transparent(not on_button)

    # --- 置顶切换 ---
    def _toggle_topmost(self):
        flags = self.windowFlags()
        is_top = bool(flags & Qt.WindowType.WindowStaysOnTopHint)
        if is_top:
            self.setWindowFlags(flags & ~Qt.WindowType.WindowStaysOnTopHint)
            self.btn_toggle_top.setIcon(FIF.PIN)
            self.btn_toggle_top.setToolTip(self.t.PinToTop)
            self._set_status(self.t.FWTopOff)
        else:
            self.setWindowFlags(flags | Qt.WindowType.WindowStaysOnTopHint)
            self.btn_toggle_top.setIcon(FIF.UNPIN)
            self.btn_toggle_top.setToolTip(self.t.FWUnpin)
            self._set_status(self.t.FWTopOn)
        self.show()

    # --- 右键菜单 ---
    def contextMenuEvent(self, event):
        pass

    def closeEvent(self, event) -> None:
        self._trace_timer.stop()
        if hasattr(self, "_range_overlay") and self._range_overlay:
            self._range_overlay.close()
        event_bus.ocr_window_closed.emit()
        return super().closeEvent(event)


class RangeSelector(QWidget):
    """全屏遮罩，用 QRubberBand 框选屏幕区域"""

    range_selected = Signal(QRect)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setCursor(Qt.CursorShape.CrossCursor)

        screen = QApplication.primaryScreen().virtualGeometry()
        self.setGeometry(screen)

        self._origin = QPoint()
        self._rubber = QRubberBand(QRubberBand.Shape.Rectangle, self)

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._origin = event.position().toPoint()
            self._rubber.setGeometry(QRect(self._origin, self._origin))
            self._rubber.show()

    def mouseMoveEvent(self, event):
        if self._rubber.isVisible():
            self._rubber.setGeometry(
                QRect(self._origin, event.position().toPoint()).normalized()
            )

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            rect = QRect(self._origin, event.position().toPoint()).normalized()
            self._rubber.hide()
            self.range_selected.emit(rect)
            self.close()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.range_selected.emit(QRect())
            self.close()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(0, 0, 0, 80))


class RangeOverlay(QWidget):
    """持续显示选中的 OCR 区域，半透明边框"""

    def __init__(self, rect: QRect, parent=None):
        super().__init__(parent)
        self._rect = rect
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setGeometry(rect)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        pen = QPen(QColor(0, 120, 215, 200))
        pen.setWidth(2)
        painter.setPen(pen)
        painter.drawRect(0, 0, self.width() - 1, self.height() - 1)
        painter.fillRect(self.rect(), QColor(0, 120, 215, 15))


if __name__ == "__main__":
    from PySide6.QtWidgets import QApplication

    app = QApplication(sys.argv)
    w = FloatingWindow()
    w.show()
    sys.exit(app.exec())
