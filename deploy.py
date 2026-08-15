import os
import re
import subprocess
import sys

from app.common.setting import VERSION


def _windows_file_version(ver: str) -> str:
    """将任意版本字符串转为合法的 Windows 文件版本号 X.Y.Z.W。"""
    nums = re.findall(r"\d+", ver)
    while len(nums) < 4:
        nums.append("0")
    return ".".join(nums[:4])


def cleanup_dist(dist_dir: str):
    """Remove unneeded Qt plugins/dlls from dist (Nuitka plugin set is not precise)"""
    removable = [
        "qt6pdf.dll",
        "qt6pdfwidgets.dll",
        "pythoncom39.dll",
        r"PySide6\qt-plugins\platforms\qdirect2d.dll",
        r"PySide6\qt-plugins\imageformats\qwebp.dll",
        r"PySide6\qt-plugins\imageformats\qtiff.dll",
        r"PySide6\qt-plugins\imageformats\qicns.dll",
        r"PySide6\qt-plugins\imageformats\qtga.dll",
        r"PySide6\qt-plugins\imageformats\qwbmp.dll",
        r"PySide6\qt-plugins\imageformats\qpdf.dll",
        r"PySide6\qt-plugins\imageformats\qgif.dll",
        r"PySide6\qt-plugins\sqldrivers\qsqlibase.dll",
        r"PySide6\qt-plugins\sqldrivers\qsqloci.dll",
        r"PySide6\qt-plugins\sqldrivers\qsqlodbc.dll",
        r"PySide6\qt-plugins\sqldrivers\qsqlpsql.dll",
        r"PySide6\qt-plugins\sqldrivers\qsqlmimer.dll",
        r"PySide6\qt-plugins\tls\qcertonlybackend.dll",
    ]
    removed = 0
    for rel in removable:
        fp = os.path.join(dist_dir, rel)
        if os.path.isfile(fp):
            size = os.path.getsize(fp)
            os.remove(fp)
            removed += size
            print(f"  Removed {rel} ({size / 1024:.0f} KB)")
    if removed:
        print(f"  Total cleaned {removed / 1024 / 1024:.1f} MB")


if sys.platform == "win32":
    wv = _windows_file_version(VERSION)
    print(wv)
    args = [
        sys.executable,
        "-m",
        "nuitka",
        "--standalone",
        "--windows-uac-admin",
        "--windows-disable-console",
        "--plugin-enable=pyside6",
        "--include-qt-plugins=sensible,sqldrivers,imageformats,platforms,styles,iconengines",
        "--assume-yes-for-downloads",
        "--mingw64",
        "--lto=yes",
        "--nofollow-import-to=pyqtgraph",
        "--nofollow-import-to=colorthief",
        "--nofollow-import-to=pythoncom",
        "--nofollow-import-to=PySide6.QtWebChannel",
        "--nofollow-import-to=PySide6.QtWebEngineCore",
        "--nofollow-import-to=PySide6.QtWebEngineWidgets",
        "--nofollow-import-to=PySide6.QtPositioning",
        "--nofollow-import-to=PySide6.QtQml",
        "--nofollow-import-to=PySide6.QtQmlModels",
        "--nofollow-import-to=PySide6.QtQuick",
        "--nofollow-import-to=PySide6.QtQuickWidgets",
        "--nofollow-import-to=PySide6.QtPrintSupport",
        "--nofollow-import-to=PySide6.QtOpenGL",
        "--nofollow-import-to=PySide6.QtPdf",
        "--show-memory",
        "--show-progress",
        "--windows-icon-from-ico=app/resource/images/logo.ico",
        f"--windows-file-version={wv}",
        f"--windows-product-version={wv}",
        '--windows-file-description="Fairy Kekkai Workshop"',
        "--output-dir=dist",
        "Fairy-Kekkai-Workshop.py",
    ]


subprocess.run(args, check=True)

