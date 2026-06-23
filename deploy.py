import argparse
import os
import sys

from app.common.setting import VERSION

parser = argparse.ArgumentParser(description="打包 Fairy Kekkai Workshop")
parser.add_argument(
    "--onefile",
    action="store_true",
    help="打包为单文件可执行程序",
)
parsed_args = parser.parse_args()

if sys.platform == "win32":
    args = [
        sys.executable,  # 使用当前Python解释器
        "-m",
        "nuitka",
        "--standalone",
        *(["--onefile"] if parsed_args.onefile else []),
        "--windows-uac-admin",
        "--windows-disable-console",
        "--plugin-enable=pyside6",
        "--include-qt-plugins=sensible,sqldrivers",
        "--assume-yes-for-downloads",
        "--mingw64",
        "--show-memory",
        "--show-progress",
        "--windows-icon-from-ico=app/resource/images/logo.ico",
        f"--windows-file-version={VERSION}",
        f"--windows-product-version={VERSION}",
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
        *(["--onefile"] if parsed_args.onefile else []),
        "--plugin-enable=pyside6",
        "--show-memory",
        "--show-progress",
        "--macos-create-app-bundle",
        "--assume-yes-for-download",
        "--macos-disable-console",
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
        "nuitka",
        "--standalone",
        *(["--onefile"] if parsed_args.onefile else []),
        "--plugin-enable=pyside6",
        "--include-qt-plugins=sensible,sqldrivers",
        "--assume-yes-for-downloads",
        "--show-memory",
        "--show-progress",
        "--linux-icon=app/resource/images/logo.png",
        "--output-dir=dist",
        "Fairy-Kekkai-Workshop.py",
    ]


os.system(" ".join(args))
print("打包完成！")
