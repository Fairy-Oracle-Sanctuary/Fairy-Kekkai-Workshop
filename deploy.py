import os
import sys

from app.common.setting import VERSION


def main():
    if sys.platform == "win32":
        args = [
            sys.executable,  # 使用当前Python解释器
            "-m",
            "nuitka",
            "--standalone",
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
            "pyinstaller",
            "-w",
            "Fairy-Kekkai-Workshop.py",
        ]

    os.system(" ".join(args))


if __name__ == "__main__":
    try:
        main()
        print("打包完成！")
    except Exception as e:
        print(f"打包失败: {e}")
        sys.exit(1)
