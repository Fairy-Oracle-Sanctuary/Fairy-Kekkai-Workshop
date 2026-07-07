import os
import platform
import re
import subprocess
import sys

from app.common.setting import VERSION


def _windows_file_version(ver: str) -> str:
    """将任意版本字符串转为合法的 Windows 文件版本号 X.Y.Z.W。"""
    nums = re.findall(r'\d+', ver)
    while len(nums) < 4:
        nums.append('0')
    return '.'.join(nums[:4])


if sys.platform == "win32":
    wv = _windows_file_version(VERSION)
    args = [
        sys.executable,
        "-m",
        "nuitka",
        "--standalone",
        "--windows-uac-admin",
        "--windows-disable-console",
        "--plugin-enable=pyside6",
        "--include-qt-plugins=sensible,sqldrivers",
        "--assume-yes-for-downloads",
    ]
    # ARM Windows 用 MSVC，x64 用 MinGW
    if platform.machine().lower() in ("amd64", "x86_64"):
        args.append("--mingw64")
    args += [
        "--show-memory",
        "--show-progress",
        "--windows-icon-from-ico=app/resource/images/logo.ico",
        f"--windows-file-version={wv}",
        f"--windows-product-version={wv}",
        '--windows-file-description="Fairy Kekkai Workshop"',
        "--output-dir=dist",
        "Fairy-Kekkai-Workshop.py",
    ]

elif sys.platform == "darwin":
    args = [
        sys.executable,
        "-m",
        "nuitka",
        "--standalone",
        "--plugin-enable=pyside6",
        "--show-memory",
        "--show-progress",
        "--macos-create-app-bundle",
        "--assume-yes-for-download",
        "--macos-disable-console",
        "--include-data-dir=tools=tools",
        f"--macos-app-version={VERSION}",
        '--macos-app-name="Fairy Kekkai Workshop"',
        "--macos-app-icon=app/resource/images/logo.ico",
        "--output-dir=dist",
        "Fairy-Kekkai-Workshop.py",
    ]
else:
    args = [
        sys.executable,
        "-m",
        "PyInstaller",
        "-w",
        "Fairy-Kekkai-Workshop.py",
    ]


subprocess.run(args, check=True)
print("打包完成！")
