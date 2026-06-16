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
- PaddleOCR 独立构建：CVUtils 源码（DBNet/CRNN/ONNX Runtime/SSIM）全部从 LunaTranslator 迁移至本项目，CMake FetchContent 自动下载 OpenCV/Clipper2/ONNX Runtime/DirectML，零手动配置
- OCR 访问冲突崩溃保护：新增 SEH 异常捕获（catch-all + /EHa），极端场景下自动跳过问题帧而非崩溃
- AI 翻译 SRT 多行修复：发送前将字幕内换行替换为空格，避免多行字幕导致 AI 编号错乱
- OCR 默认参数优化：SSIM 阈值 92→90，新增置信度阈值配置（0.0-1.0，默认 0.3），支持词语级+帧级双重过滤；UI 新增参数调整指南
- Whisper 设置界面重构：模型选择逻辑简化，自动检测可用模型
- 项目拖拽排序与置顶功能
- 国际化（i18n）系统全面上线：基于 Qt Linguist，支持中文、英文
- Windows 10 暗色模式背景渲染修复
- 语言词典重构为使用语言代码作为键值
- 文档完善：双语 README 增加项目理念与许可说明；DEVELOPMENT.md 新增 paddleocr C++ 编译章节和 OCR 参数参考表
- 更新 Qt 资源编译器至 6.10.2
- 优化翻译上下文管理，分批翻译时仅保留两轮历史，极大降低 API Token 消耗与延迟（支持 Deepseek/多模型）
- Whisper 自动语音识别（ASR）集成，支持命令行工具，核心模块切换为 Const-me/Whisper 实现，性能与兼容性提升
- 新增新手引导功能，提升首次使用体验
- OCR 与 Whisper 处理流程优化，支持进度追踪与批量处理
- 批量任务功能上线，支持多集自动派发
- 批量删除功能上线，支持一键删除项目内各个文件
- 设置界面与翻译界面提示文本优化，提升用户体验
- 下载流程日志细化，修复路径类型转换问题
- UI 交互大幅优化，完善系统托盘通知与应用关闭流程
- 重构跨平台可执行文件路径检测逻辑，提升兼容性
- 部署脚本主函数重构，支持 macOS 工具目录自动包含
- 新增 Whisper 示例项目与依赖，调整默认语言与提示文本
- 构建路径与 Output 目录优化，完善 .gitignore
- README 增加 Star History 图表与 emoji 渲染修复
- Whisper 构建文档完善
- 图标与界面细节更新
- 用户可自定义窗口类型

## 下载提示
- [Fairy-Kekkai-Workshop-v2.0.0-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.0.0/Fairy-Kekkai-Workshop-v2.0.0-Windows-x86_64-Setup.exe)
- [Fairy-Kekkai-Workshop-v2.0.0-macos-arm64](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.0.0/Fairy-Kekkai-Workshop-v2.0.0-macos-arm64.dmg)(macos arm64)
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

Fairy Kekkai Workshop 是一款基于 PySide6（Qt for Python）开发的桌面字幕制作与本地化软件。

软件需要使用 runFullTrust 功能，因为其核心功能涉及项目管理以及用户指定工作目录中的文件操作，包括创建、读取、移动、重命名和删除项目文件及生成的媒体资源。

软件还集成了 FFmpeg、Whisper、PaddleOCR 和 yt-dlp 等外部工具，需要通过创建子进程完成媒体处理、OCR 识别、语音转文字、字幕生成、翻译流程和视频编码等任务。

runFullTrust 仅用于实现上述桌面应用核心功能，不用于监控用户活动、修改系统设置、访问无关文件或收集用户个人信息。

Fairy Kekkai Workshop is a desktop subtitle production and localization application built with PySide6 (Qt for Python).

The application requires the runFullTrust capability because it provides project management features that operate on user-selected working directories, including creating, reading, moving, renaming, and deleting project files and generated media assets.

The application also integrates external processing tools such as FFmpeg, Whisper, PaddleOCR, and yt-dlp. These tools are executed as child processes to perform media acquisition, OCR recognition, speech transcription, subtitle generation, translation workflows, and video encoding.

The runFullTrust capability is required solely to support these core desktop application functions. The application does not use this capability to monitor user activity, modify system settings, access unrelated files, or collect personal information.

"""
