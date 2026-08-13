import os
import sys
from pathlib import Path

from PySide6.QtCore import QDir, QFileInfo, QProcess, QUrl
from PySide6.QtGui import QDesktopServices


def openUrl(url):
    """打开本地文件或网址（对齐 Easy-FFmpeg）"""
    if not url:
        return False
    if not url.startswith("http"):
        if not os.path.exists(url):
            return False
        QDesktopServices.openUrl(QUrl.fromLocalFile(url))
    else:
        QDesktopServices.openUrl(QUrl(url))
    return True


def showInFolder(path):
    """在文件管理器中显示文件/文件夹（对齐 Easy-FFmpeg）

    文件 → explorer /select, 选中；文件夹 → 直接打开。
    使用 QProcess.startDetached 传参列表，避免字符串拼接导致的路径转义问题。
    """
    if not path:
        return False

    if isinstance(path, Path):
        path = str(path.absolute())

    if not path or path.lower().startswith("http"):
        return False

    if not os.path.exists(path):
        return False

    info = QFileInfo(path)
    if sys.platform == "win32":
        args = [QDir.toNativeSeparators(path)]
        if not info.isDir():
            args.insert(0, "/select,")
        QProcess.startDetached("explorer", args)
    elif sys.platform == "darwin":
        args = [
            "-e",
            'tell application "Finder"',
            "-e",
            "activate",
            "-e",
            f'select POSIX file "{path}"',
            "-e",
            "end tell",
            "-e",
            "return",
        ]
        QProcess.execute("/usr/bin/osascript", args)
    else:
        url = QUrl.fromLocalFile(path if info.isDir() else info.path())
        QDesktopServices.openUrl(url)

    return True
