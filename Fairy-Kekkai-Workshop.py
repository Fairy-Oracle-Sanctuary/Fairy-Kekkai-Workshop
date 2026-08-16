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

from PySide6.QtCore import QFile, QLocale, QTranslator, QtMsgType, qInstallMessageHandler

from app.common.application import SingletonApplication
from app.common.config import Language, cfg
from app.common.setting import TEAM, VERSION
from app.view.main_window import MainWindow
from libs.qfluentwidgets_pro import FluentTranslator


def _filter_font_warnings(msg_type, context, message):
    """过滤 qfluentwidgets 内部的 QFont::setPointSize(-1) 无害警告"""
    if msg_type == QtMsgType.QtWarningMsg and "QFont::setPointSize" in message:
        return
    print(message, file=sys.stderr)


def main():
    # 界面缩放
    if cfg.get(cfg.dpiScale) != "Auto":
        os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "0"
        os.environ["QT_SCALE_FACTOR"] = str(cfg.get(cfg.dpiScale))

    # 创建应用程序实例（SingletonApplication 内部处理单例检测）
    app = SingletonApplication(sys.argv, "Fairy-Kekkai-Workshop")
    qInstallMessageHandler(_filter_font_warnings)
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
