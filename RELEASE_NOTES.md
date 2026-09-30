## 更新日志

### 重大变化 / Breaking Changes
- 核心全量重写：从 Python + PySide6 迁移到 C++17 + Qt 6 + Qt-Fluent-Widgets，编译为原生桌面程序，体积更小、启动更快、内存占用更低
  Full core rewrite: migrated from Python + PySide6 to C++17 + Qt 6 + Qt-Fluent-Widgets, shipping as a native desktop application with smaller size, faster startup and lower memory usage
- 从此版本起，安装包内不再附带 Python 运行时与 Python 依赖，老版本升级可使用增量包（Clear），无需因主程序大版本变化而卸载；Clear 不含外部工具与模型，需另行保留或补齐所需资源
  From this version onward the installer no longer bundles the Python runtime or Python dependencies; older builds can be upgraded with the incremental (Clear) package without uninstalling solely because of a major application version change; Clear excludes external tools and models, which must be retained or supplied separately
- 源码目录结构重组：新增 `cpp/` 原生源码树（view / components / service / common），原 Python 实现保留在 `app/` 作为参考
  Source tree reorganized: new `cpp/` native source tree (view / components / service / common), with the original Python implementation kept under `app/` as reference

### 新增 / Added
- 悬浮截图翻译窗口：屏幕任意区域框选后即可 OCR 并调用 AI 翻译，无需切换窗口
  Floating screenshot translation window: select any region on screen to OCR and translate with AI without leaving the current window
  - 支持窗口绑定：可把框选区域锁定到指定窗口，随窗口移动、缩放自动跟随
    Window binding: lock the selected region to a target window and follow its move/resize automatically
  - 支持置顶、锁定、鼠标穿透、背景透明、圆角等显示模式
    Supports always-on-top, lock, click-through, transparent background and rounded corners
  - 内置历史记录，可回看并复制此前的识别 / 翻译结果
    Built-in history to review and copy previous OCR / translation results
- 启动维护机制：首次启动时自动把散落在软件目录中的项目迁移到独立数据目录，界面模态展示迁移进度
  Startup maintenance: on first launch, projects scattered inside the install directory are migrated to a dedicated data directory with modal progress reporting
- 旧版资源清理：启动时扫描并清理上一代 OCR 模型/资源与 Python(Nuitka + PySide6) 残留依赖，释放磁盘占用
  Legacy cleanup: scans and removes the previous generation's OCR assets and leftover Python (Nuitka + PySide6) dependencies at startup to reclaim disk space
- 全新打包链路：`package-cpp-release.ps1` 一键完成 CMake 构建 → windeployqt 依赖收集 → Inno Setup 打包
  New packaging pipeline: `package-cpp-release.ps1` performs CMake build → windeployqt dependency collection → Inno Setup packaging in one command
- 发布管线全面转向 C++：`.github/workflows/build-cpp.yml` 打 tag 即自动预检、并行构建并发布 CPU / GPU (CUDA 11.8 / 12.9) / Clear 四个安装包，Python 版旧管线 `release.yml`、`deploy-windows.yml` 已下线
  Release pipeline fully moved to the C++ build: `.github/workflows/build-cpp.yml` pre-checks, builds and publishes the CPU / GPU (CUDA 11.8 / 12.9) / Clear installers on tag push, retiring the old Python workflows `release.yml` and `deploy-windows.yml`
- 发布前自动校验发布说明契约（四个安装包链接齐全、链接版本号与 tag 一致、更换 OCR 引擎时自动要求补上引擎换代标记），避免更新包匹配错版本
  The release body contract is now validated automatically (all four installer links present, link versions matching the tag, and the engine-swap marker enforced whenever the OCR engine changes), preventing updates from resolving to the wrong package

### 修复 / Fixed
- 修复框选区域在高 DPI 屏幕下坐标偏移的问题（按设备像素比换算）
  Fixed region selection offset on high-DPI screens (coordinates now scaled by device pixel ratio)
- 修复悬浮窗关闭后入口按钮未恢复、无法再次打开的问题
  Fixed the entry button not being re-enabled after the floating window closed
- 修复悬浮窗历史菜单回调在菜单销毁时可能访问已释放内存导致的崩溃
  Fixed a crash where the floating window's history menu callback could touch freed memory during menu teardown
- 修复 C++ 版默认图标/主题资源在深色模式下显示异常的问题
  Fixed incorrect default icon/theme rendering in dark mode on the C++ build

### 改进 / Improved
- OCR 引擎升级到 PaddleOCR-Standalone v3.7.0，安装包与 CI 变体同步更新
  OCR engine upgraded to PaddleOCR-Standalone v3.7.0, with installer and CI variants updated accordingly
- 版本号统一由 `cpp/resources/setting_data.json` 派生，打包脚本与界面展示保持一致
  Version number now derives from `cpp/resources/setting_data.json`, keeping the packaging script and in-app display consistent
- 任务执行统一到 QRunnable + 事件总线架构，屏幕 OCR / 屏幕翻译复用同一套线程池调度
  Task execution unified under the QRunnable + event bus architecture; screen OCR and screen translation share the same thread-pool scheduling
- 移植进度对照表 `cpp/PORTING_MATRIX.md` 持续维护，逐项记录与 Python 版的差异
  Porting progress matrix `cpp/PORTING_MATRIX.md` is continuously maintained, recording differences against the Python version item by item

## 下载提示

| 平台 / Platform | 类型 / Type | 安装包 / Installer |
| --- | --- | --- |
| Windows 10/11 | CPU | [Fairy-Kekkai-Workshop-v3.0.0-CPU-v3.7.0-Windows-x86_64-Setup.exe（含 PP-OCRv6 引擎与模型，替代旧版 PaddleOCR-CPU-v1.5.1）](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.0.0/Fairy-Kekkai-Workshop-v3.0.0-CPU-v3.7.0-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 11.8, Nvidia 10 系列) | [Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe（含 PP-OCRv6 引擎与模型，替代旧版 PaddleOCR-GPU-v1.5.1-CUDA-11.8）](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.0.0/Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 12.9, Nvidia 16 - 50 系列) | [Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-12.9-Windows-x86_64-Setup.exe（含 PP-OCRv6 引擎与模型，替代旧版 PaddleOCR-GPU-v1.5.1-CUDA-12.9）](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.0.0/Fairy-Kekkai-Workshop-v3.0.0-GPU-v3.7.0-CUDA-12.9-Windows-x86_64-Setup.exe) |

- 从 2.x（Python 版）升级到 3.0.0 可以使用 Clear 增量包，也可以下载上表中与你显卡对应的整包。Clear 不含 OCR 引擎与模型；本次更换了引擎代次，如需 OCR 功能，请另行准备新版引擎与模型，或直接使用对应整包。启动维护会清理旧代次资源
  You can upgrade from 2.x (Python build) to 3.0.0 using either Clear or the matching full package above. Clear excludes OCR engines and models. This release changes the engine generation; to use OCR, supply the new engine and models separately or use the matching full package. Startup maintenance removes obsolete resources
- Clear 增量包（可用于旧版本升级，包括主程序大版本更新；不含外部工具与模型）：
  Clear incremental package (supports upgrades from older versions, including major application updates; excludes external tools and models):
  [Fairy-Kekkai-Workshop-v3.0.0-Clear-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.0.0/Fairy-Kekkai-Workshop-v3.0.0-Clear-Windows-x86_64-Setup.exe)
- mac 版本无变动，直接下载上一个版本即可
  macOS version unchanged, download the previous version directly
- 迅雷链接 / Thunder Drive: https://pan.xunlei.com/s/VOl2n0KP6LH3zXUqcYX1iYUAA1?pwd=yzim#
- 从 2.x 升级无需手动卸载旧版：启动维护会自动迁移项目数据并清理旧版 PaddleOCR 引擎、旧代次识别模型与 Python 运行库残留
  Upgrading from 2.x does not require uninstalling the old build: startup maintenance migrates project data and removes the previous PaddleOCR engine, the old model generation and leftover Python runtime files automatically

## 使用说明 / Usage

- **Windows**：根据显卡选择对应版本运行安装包，按向导完成安装（需管理员权限）。
  Choose the version matching your GPU and run the installer, then follow the setup wizard (administrator privileges required).
- 升级安装时若提示清理旧版资源，请保持网络与磁盘空间充足，清理过程不可中断。
  If prompted to clean up legacy resources during an upgrade, make sure network and disk space are sufficient; the cleanup must not be interrupted.

!OCRUPDATE!
