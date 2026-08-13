from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter
from PySide6.QtWidgets import QVBoxLayout

from libs.qfluentwidgets_pro import CaptionLabel, CardWidget, IconWidget, isDarkTheme


class EmptyStatusWidget(CardWidget):
    def __init__(self, icon, text, parent=None):
        super().__init__(parent=parent)
        self.iconWidget = IconWidget(icon)
        self.label = CaptionLabel(text)

        self.vBoxLayout = QVBoxLayout(self)

        self._initWidget()

    def _initWidget(self):
        self.setBorderRadius(10)

        self.iconWidget.setFixedSize(80, 80)

        self.label.setTextColor(QColor(96, 96, 96), QColor(216, 216, 216))
        self.label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self.vBoxLayout.setSpacing(10)
        self.vBoxLayout.setContentsMargins(16, 20, 16, 20)
        self.vBoxLayout.addWidget(self.iconWidget, 0, Qt.AlignmentFlag.AlignHCenter)
        self.vBoxLayout.addWidget(self.label, 0, Qt.AlignmentFlag.AlignHCenter)

    def setIcon(self, icon):
        self.iconWidget.setIcon(icon)

    def setText(self, text):
        self.label.setText(text)

    def _normalBackgroundColor(self):
        return QColor(255, 255, 255, 13 if isDarkTheme() else 200)

    def paintEvent(self, e):
        painter = QPainter(self)
        painter.setRenderHints(QPainter.Antialiasing)
        painter.setBrush(self.backgroundColor)
        painter.setPen(Qt.PenStyle.NoPen)

        r = self.borderRadius
        painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), r, r)
