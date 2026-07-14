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

from PySide6.QtCore import QSharedMemory, QTranslator
from PySide6.QtWidgets import QApplication
from qfluentwidgets import FluentTranslator

from app.common.config import cfg
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
    locale = cfg.get(cfg.language).value
    translator = FluentTranslator(locale)
    galleryTranslator = QTranslator()
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
- 新增悬浮窗 OCR

## 下载提示
- Windows10/11：
  - CPU：
    - [Fairy-Kekkai-Workshop-v2.4.0-CPU-v1.5.1-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.4.0/Fairy-Kekkai-Workshop-v2.4.0-CPU-v1.5.1-Windows-x86_64-Setup.exe)
  - GPU：
    - [Fairy-Kekkai-Workshop-v2.4.0-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.4.0/Fairy-Kekkai-Workshop-v2.4.0-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe)
    - [Fairy-Kekkai-Workshop-v2.4.0-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.4.0/Fairy-Kekkai-Workshop-v2.4.0-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe)
- mac版本无变动，直接下载上一个版本即可
- 迅雷链接：https://pan.xunlei.com/s/VOl2n0KP6LH3zXUqcYX1iYUAA1?pwd=yzim#

# Fairy Kekkai Workshop

---

## 中文简介

Fairy Kekkai Workshop（仙·结界工坊）是一款自由软件字幕制作平台，旨在为字幕组、内容创作者、本地化团队以及个人用户提供完整的一站式字幕制作解决方案。

软件集项目管理、OCR 字幕识别、语音转文字、AI 辅助翻译、视频处理与压制等功能于一体，并整合 PaddleOCR、Whisper、FFmpeg 等优秀自由软件与开源项目，帮助用户高效完成从素材处理到成品发布的整个工作流程。

Fairy Kekkai Workshop 尊重用户自由。用户可以自由运行、研究、修改和再分发本软件。本项目致力于构建开放、透明、可持续发展的字幕制作生态，而非将用户锁定在封闭的平台和服务之中。

### 主要功能

* 项目管理与任务组织
* OCR 字幕提取
* Whisper 语音识别
* AI 辅助翻译
* 字幕编辑与校对
* FFmpeg 视频编码与压制
* GPU 加速支持
* 多格式字幕导出

项目地址：https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop

---

## English Description

Fairy Kekkai Workshop is a free software platform designed for subtitle production, localization, and media processing workflows.
The application integrates project management, OCR subtitle extraction, speech recognition, AI-assisted translation, subtitle editing, and video encoding into a unified environment. By leveraging powerful free and open-source technologies such as PaddleOCR, Whisper, and FFmpeg, Fairy Kekkai Workshop helps users efficiently complete the entire workflow from source processing to final release.
Fairy Kekkai Workshop respects user freedom. Users are free to run, study, modify, and redistribute the software. The project is committed to building an open, transparent, and sustainable ecosystem for subtitle creation rather than locking users into proprietary platforms or services.

### Key Features

* Project management and workflow organization
* OCR-based subtitle extraction
* Whisper speech recognition
* AI-assisted subtitle translation
* Subtitle editing and review
* FFmpeg-powered video encoding
* GPU acceleration support
* Multiple subtitle export formats

### Philosophy

Fairy Kekkai Workshop is developed under the spirit of free software. We believe users should control their tools, not the other way around. Transparency, freedom, collaboration, and community-driven development are fundamental values of this project.

Project page: https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop


---

✨一站式字幕制作平台 • 📁项目管理 • 🔤OCR提取 • 🎙️语音识别 • 🌐AI翻译 • 🎬视频压制 • ⚡GPU加速 • 🔓自由软件

✨All-in-One Fansub Platform • 📁Project Management • 🔤OCR Extraction • 🎙️Speech Recognition • 🌐AI Translation • 🎬Video Encoding • ⚡GPU Acceleration • 🔓Free Software

---

Copyleft 🄯 2025 天机阁(Fairy-Oracle-Sanctuary)
Copyleft 🄯 2025 Fairy Oracle Sanctuary

---

本软件为自由软件。

软件源代码采用 GNU General Public License v3.0（GPL-3.0）许可协议发布；项目图标及相关美术资源采用 Creative Commons Attribution-ShareAlike 4.0 International（CC BY-SA 4.0）许可协议发布。

用户有权根据相应许可证条款自由运行、研究、修改和再分发本软件。

This software is free software.

The source code is licensed under the GNU General Public License v3.0 (GPL-3.0). Project icons and artwork are licensed under the Creative Commons Attribution-ShareAlike 4.0 International License (CC BY-SA 4.0).

Users are free to run, study, modify, and redistribute the software in accordance with the applicable license terms.

---

The application requires runFullTrust for project file operations and executing external tools (FFmpeg, Whisper, PaddleOCR, yt-dlp) for media processing. This capability is used solely for core desktop application functions.

Our application is a desktop software installer that requires elevation during installation to write to Program Files directory and register file associations in the system registry. The installed application itself runs without elevation. This is standard practice for desktop software distribution on Windows.
"""
