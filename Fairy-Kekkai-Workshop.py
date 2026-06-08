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

from PySide6.QtCore import QSharedMemory
from PySide6.QtWidgets import QApplication

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

"""
