"""
以下文件中的代码用到了仓库(https://github.com/zhiyiYo/Fluent-M3U8)中的源码
Fairy-Kekkai-Workshop/app/view/setting_interface.py
Fairy-Kekkai-Workshop/app/service/version_service.py
Fairy-Kekkai-Workshop/app/common/setting.py
Fairy-Kekkai-Workshop/app/common/logger.py
Fairy-Kekkai-Workshop/app/components/sample_card.py
Fairy-Kekkai-Workshop/deploy.py
"""

import os
import sys

from PySide6.QtCore import QFile, QLocale, QSharedMemory, QTranslator
from PySide6.QtWidgets import QApplication
from qfluentwidgets import FluentTranslator

from app.common.config import Language, cfg
from app.common.setting import TEAM, VERSION
from app.view.main_window import MainWindow


def is_app_running():
    """检查应用程序是否已经在运行"""
    # 使用共享内存或系统信号量来确保单例
    app_id = "Fairy-Kekkai-Workshop"
    shared_memory = QSharedMemory(app_id)

    if shared_memory.attach():
        # 已经有一个实例在运行
        return True
    else:
        # 这是第一个实例
        shared_memory.create(1)
        return False


def main():
    # 检查是否已经有实例在运行
    if is_app_running():
        # 可以尝试激活已运行的实例
        print("应用程序已经在运行中")
        return 1

    # 界面缩放
    if cfg.get(cfg.dpiScale) != "Auto":
        os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "0"
        os.environ["QT_SCALE_FACTOR"] = str(cfg.get(cfg.dpiScale))

    # 创建应用程序实例
    app = QApplication(sys.argv)
    app.setApplicationName("Fairy-Kekkai-Workshop")
    app.setApplicationVersion(VERSION)
    app.setOrganizationName(TEAM)

    # 安装翻译器
    language = cfg.get(cfg.language)
    locale = QLocale.system() if language == Language.AUTO else language.value
    translator = FluentTranslator(locale)
    galleryTranslator = QTranslator()
    if language != Language.AUTO or QFile.exists(f":/app/i18n/app.{locale.name()}.qm"):
        galleryTranslator.load(locale, "app", ".", ":/app/i18n")

    app.installTranslator(translator)
    app.installTranslator(galleryTranslator)

    # 创建并显示主窗口
    window = MainWindow()
    window.show()

    # 运行应用程序
    return app.exec()


if __name__ == "__main__":
    print(sys.platform)
    sys.exit(main())

# Fairy-Kekkai-Workshop

"""
## 更新日志
- 修复：导入外部路径项目时，无效链接路径被加入项目列表导致启动崩溃的问题
- 修复：导入项目时重复检测失效，同一路径可被重复导入的问题（Python 3.12+ 兼容性）
- 修复：编辑项目名称后自定义拖拽排序位置重置到末尾的问题
- 修复：编辑项目名称后修改图标可能操作到错误项目的问题

## 下载提示
- Windows10/11：
  - CPU：
    - [Fairy-Kekkai-Workshop-v2.5.1-CPU-v1.5.1-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.1/Fairy-Kekkai-Workshop-v2.5.1-CPU-v1.5.1-Windows-x86_64-Setup.exe)
  - GPU：
    - [Fairy-Kekkai-Workshop-v2.5.1-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.1/Fairy-Kekkai-Workshop-v2.5.1-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe) (Nvidia 10 Series graphics cards)
    - [Fairy-Kekkai-Workshop-v2.5.1-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.1/Fairy-Kekkai-Workshop-v2.5.1-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe) (Nvidia 16 - 50 Series graphics cards)
  - 如果你已安装过上个版本：
    - [Fairy-Kekkai-Workshop-v2.5.1-Clear-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.1/Fairy-Kekkai-Workshop-v2.5.1-Clear-Windows-x86_64-Setup.exe)
- mac版本无变动，直接下载上一个版本即可
- 迅雷链接：https://pan.xunlei.com/s/VOl2n0KP6LH3zXUqcYX1iYUAA1?pwd=yzim#

# Fairy Kekkai Workshop
!OCRUPDATE!

"""
