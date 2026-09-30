# Fairy-Kekkai-Workshop 开发文档

## 项目概述

**Fairy-Kekkai-Workshop** 是一个功能完整的视频字幕处理和管理工具，支持视频下载、字幕提取、多语言翻译、悬浮取词翻译、视频压制等功能。

自 **v3.0.0** 起，主程序已从 PySide6 全量重写为 **C++17 + Qt 6 + Qt-Fluent-Widgets** 原生桌面应用，源码位于 `cpp/`，并通过 CMake + MSVC 构建、Inno Setup 打包分发。仓库中的 `app/` 目录保留旧版 Python + PySide6 实现，作为移植对照参考，不再随安装包分发。

本文件同时覆盖两代实现：C++ 版架构见「[C++ 原生版架构](#c-原生版架构)」一节，其余章节如未特别说明，描述的都是 `app/` 下的 Python 参考实现。

### 核心特性
- 📥 **视频下载**：基于 yt-dlp，支持 1800+ 视频网站
- 🔤 **字幕提取**：基于 [VideOCR](https://github.com/timminator/VideOCR)，支持 PaddleOCR 与 Google Lens 双引擎，支持 200+ 种语言
- 🎤 **语音识别**：C++ 应用使用官方 whisper.cpp v1.9.4 与 Silero VAD，支持多语言语音转字幕和实时进度；接入及编译说明见 `cpp/WHISPER.md`。
- 🌐 **智能翻译**：支持多个 AI 模型（OpenAI、Deepseek、腾讯混元、ERNIE、Gemini、书生等）
- 🔍 **悬浮取词翻译**：屏幕任意区域框选后 OCR + AI 翻译，支持窗口绑定、跟随、置顶、鼠标穿透（仅 Windows）
- 🎬 **视频压制**：基于 FFmpeg，支持自定义编码参数
- 💾 **项目管理**：完整的项目文件系统管理，支持导入/链接外部项目，首次启动自动迁移到独立数据目录
- 🎨 **主题切换**：标题栏快捷主题切换按钮，支持深色/浅色模式
- 🚀 **启动页**：带进度条和状态文字的启动页面，含启动维护（项目迁移 / 旧版资源清理）
- 🌍 **多语言支持**：支持 9 种语言界面（简体中文、英语、日语、韩语、德语、西班牙语、法语、葡萄牙语、繁体中文）

---

## 项目结构

```
Fairy-Kekkai-Workshop/
├── Fairy-Kekkai-Workshop.py       # 主入口文件
├── DEVELOPMENT.md                 # 本文件
├── requirements.txt               # 依赖列表
├── deploy.py                      # 打包脚本
├── app/
│   ├── common/                    # 公共模块
│   │   ├── config.py              # 配置管理（QConfig）
│   │   ├── event_bus.py           # 全局事件总线
│   │   ├── events.py              # 事件数据类
│   │   ├── logger.py              # 日志模块
│   │   ├── setting.py             # 应用常量和默认配置
│   │   └── style_sheet.py         # QSS 样式表管理
│   │
│   ├── components/                # UI 组件
│   │   ├── dialog.py              # 自定义对话框
│   │   ├── config_card.py         # 配置卡片组件
│   │   ├── project_card.py        # 项目卡片组件
│   │   ├── task_card.py           # 任务卡片组件
│   │   ├── infobar.py             # 通知栏组件
│   │   ├── system_tray.py         # 系统托盘
│   │   └── *.py                   # 其他 UI 组件
│   │
│   ├── service/                   # 业务逻辑服务
│   │   ├── project_service.py     # 项目管理服务
│   │   ├── translate_service.py   # 翻译服务（支持多个 AI 模型）
│   │   ├── download_service.py    # 视频下载服务
│   │   ├── ffmpeg_service.py      # 视频压制服务
│   │   ├── ocr_service.py         # OCR 字幕提取服务
│   │   ├── srt_service.py         # 字幕文件处理
│   │   ├── version_service.py     # 版本更新检查
│   │   └── CLI/                   # CLI 工具模块
│   │       ├── videocr/           # videocr 核心模块（来自 [VideOCR](https://github.com/timminator/VideOCR)）
│   │       │   ├── __init__.py    # 模块导出
│   │       │   ├── api.py         # OCR API（save_subtitles_to_file）
│   │       │   ├── video.py       # 视频处理（帧提取、OCR、字幕生成）
│   │       │   ├── models.py      # 数据模型
│   │       │   ├── utils.py       # 工具函数（SSIM、时间解析、语言字典等）
│   │       │   ├── lang_dictionaries.py # PaddleOCR/Google Lens 语言代码映射
│   │       │   └── pyav_adapter.py # PyAV 视频解码适配器
│   │       ├── whispernet/         # WhisperNet CLI (C#)
│   │       │   ├── Program.cs     # CLI 入口，支持进度输出
│   │       │   └── WhisperNetCLI.csproj
│   │       ├── whisper/            # Whisper C++ 源码
│   │       │   ├── API/            # Whisper API 定义
│   │       │   ├── CPU/            # CPU 实现
│   │       │   ├── D3D/            # Direct3D 实现
│   │       │   ├── ML/             # 机器学习模型
│   │       │   ├── Whisper/        # Whisper 核心实现
│   │       │   ├── Utils/          # 工具函数
│   │       │   ├── Whisper.vcxproj # Visual Studio 项目文件
│   │       │   └── ...             # 其他源文件
│   │       └── paddleocr/         # paddleocr C++ 源码（已移除，改用 VideOCR）
│   │
│   ├── view/                      # UI 视图层
│   │   ├── main_window.py         # 主窗口（含启动页、主题切换按钮）
│   │   ├── home_interface.py      # 主页（含关于卡片、清空日志/重置设置按钮）
│   │   ├── project_interface.py   # 项目管理页
│   │   ├── download_interface.py  # 下载页
│   │   ├── translate_interface.py # 翻译页
│   │   ├── ffmpeg_interface.py    # 压制页
│   │   ├── videocr_interface.py   # OCR 页
│   │   ├── whisper_interface.py   # 语音识别页
│   │   ├── setting_interface.py   # 设置页（含 Whisper CLI/模型路径配置）
│   │   └── *_task_interface.py    # 任务进度页
│   │
│   └── resource/                  # 资源文件
│       ├── resource_rc.py         # Qt 资源编译文件
│       ├── resource.qrc           # Qt 资源描述
│       ├── images/                # 图片资源
│       └── qss/                   # 样式表
│
├── tools/                         # 外部工具目录
│   ├── OCR.model/                 # OCR 模型文件（PaddleOCR 模型）
│   ├── videocr-cli.exe            # VideOCR CLI 可执行文件（Nuitka 打包）
│   ├── Whisper.model/             # Whisper 模型文件（ggml 格式）
│   ├── Whisper/                   # WhisperNet CLI 及依赖
│   │   ├── WhisperNetCLI.exe
│   │   ├── Whisper.dll
│   │   ├── WhisperNet.dll
│   │   ├── ComLight.dll
│   │   └── ...
│   ├── yt-dlp.exe                 # 视频下载工具
│   └── ffmpeg.exe                 # 视频处理工具
│
└── AppData/                       # 应用数据目录
    ├── config.json                # 用户配置
    ├── project.json               # 项目记录
    └── database.db                # 应用数据库
```

---

## C++ 原生版架构

> v3.0.0 起主程序使用本节的实现。Python 版（`app/`）保留为移植对照，两者共用同一套 `tools/` 外部工具与 `AppData/` 数据结构。

```
cpp/
├── CMakeLists.txt                 # 构建入口（C++17 / AUTOMOC / AUTORCC）
├── PORTING_MATRIX.md              # 与 Python 版的逐项移植对照表
├── resources/
│   ├── app_assets.qrc             # 图标等资源
│   ├── app.rc.in                  # exe 图标资源模板（configure_file 生成）
│   └── setting_data.json          # 版本号/更新地址/语言字典/AI 错误码映射
└── src/
    ├── main.cpp                   # 入口：QApplication、单例、主窗口
    │
    ├── common/                    # 公共基础设施
    │   ├── application.*          # 应用级单例与全局初始化
    │   ├── app_data.*             # AppData 目录布局与读写
    │   ├── config.*               # 应用配置（写作配合 qfw::QConfig）
    │   ├── config_keys.h          # 配置键常量
    │   ├── event_bus.*            # 全局事件总线（跨界面/线程通信）
    │   ├── events.h               # 事件数据结构
    │   ├── logger.*               # 日志
    │   ├── setting.*              # 应用常量与默认值
    │   ├── style_sheet.*          # QSS 管理
    │   ├── text.* / text_format.* # 国际化文案与占位符格式化
    │   ├── task_status.h          # 任务状态枚举
    │   └── utils.*                # showInFolder / openUrl 等跨平台工具
    │
    ├── components/                # 可复用 UI 组件
    │   ├── base_stacked_interface.*  # 多页堆叠界面基类
    │   ├── base_function_interface.* # 单个功能页基类
    │   ├── base_task_interface.*     # 任务页基类（卡片调度/批量/清理）
    │   ├── config_card.* / project_card.* / task_card.*
    │   ├── dialog.*               # 自定义对话框（BaseInputDialog 等）
    │   ├── info_card.*            # 主页「关于」卡片（含悬浮取词入口）
    │   ├── floating_window.*      # 悬浮截图取词窗口（FloatingWindow/RangeSelector/RangeOverlay）
    │   ├── startup_maintenance.*  # 启动维护对话框与后台 Worker
    │   ├── project_migration.*    # 项目搬迁交互
    │   ├── screen.*               # 屏幕截图/框选辅助
    │   ├── update_dialog.*        # 更新提示对话框
    │   ├── statistic_widget.* / empty_status_widget.* / file_item_widget.* / sample_card.* / pager.*
    │   └── system_tray.* / notification_service.h
    │
    ├── service/                   # 业务逻辑
    │   ├── task_base.*            # QRunnable + QObject 任务基类
    │   ├── download_service.*     # yt-dlp 下载
    │   ├── ffmpeg_service.*       # FFmpeg 压制
    │   ├── ocr_service.* / paddleocr.* / ocr_migration.*   # OCR 与旧版资源迁移
    │   ├── whisper_service.*      # Whisper 语音识别
    │   ├── translate_service.*    # AI 翻译（含屏幕取词一次性翻译 Runner）
    │   ├── project_service.* / project_health.* / project_relocate.*
    │   ├── legacy_cleanup.*       # 上一代 Python 残留清理
    │   ├── video_preview.* / video_frame_service.*
    │   └── version_service.*      # 版本更新检查（解析 RELEASE_NOTES）
    │
    └── view/                      # 界面层
        ├── main_window.*          # 主窗口（启动页、主题按钮、路由）
        ├── home_interface.*       # 主页
        ├── project_interface.* / project_detail_interface.* / project_stacked_interface.*
        ├── download_interface.* / videocr_interface.* / whisper_interface.*
        ├── translate_interface.* / ffmpeg_interface.*
        ├── setting_interface.* / log_interface.*
        └── *_task_interface.*     # 各功能的任务进度页
```

### 关键约定

- **构建系统**：`cpp/CMakeLists.txt` 通过 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 收集 `src/**/*.cpp|h`，新增源文件无需手动登记。
- **自动 MOC**：启用 `CMAKE_AUTOMOC` / `CMAKE_AUTORCC`，带 `Q_OBJECT` 的头文件会被自动处理。
- **产物命名**：target 名为 `Fairy-Kekkai-Workshop-Cpp`，但 `OUTPUT_NAME` 统一为 `Fairy-Kekkai-Workshop`，与 Python 版共用同一 AppId 与安装目录，安装包与快捷方式可直接沿用。
- **管理员权限**：MSVC 下通过 `/MANIFESTUAC:level='requireAdministrator'` 声明提权，屏幕框选等 Win32 能力依赖该权限。
- **图标**：`configure_file` 由 `resources/app.rc.in` 生成 `.rc`，复用 Python 端同一份 `app/resource/images/logo.ico`。
- **主题 API**：主题相关信号与查询在 `qfw::QConfig`（`themeChanged` / `isDarkTheme()`），不是 `fkw::AppConfig`。
- **图标 API**：`qfw::FluentIcon` **没有** 隐式 `QIcon` 转换，`setIcon` 时必须显式 `.qicon()`。
- **菜单回调**：`qfw::RoundMenu` 的菜单项槽需用 `QTimer::singleShot(0, ...)` 延后一个事件循环，避免菜单自身事件栈销毁时访问已释放内存。
- **异步任务**：统一使用 `QObject + QRunnable`（`setAutoDelete(false)`，`run()` 末尾 `deleteLater()`），由 `QThreadPool::globalInstance()->start(...)` 调度，结果通过 `GlobalEventBus` 回传界面线程。
- **版本号单一来源**：`cpp/resources/setting_data.json` 的 `VERSION` 字段，打包脚本与 README 徽章均从此派生。

### 构建与打包

**手动构建**：

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --config Release
```

依赖：Qt 6（Widgets / Svg / Network）、OpenCV 4.12（core / videoio）、Qt-Fluent-Widgets（`third_party/` 子目录）。若 CMake 找不到 OpenCV，可显式指定：

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR=D:/CODE/opencv-4.12.0/build
```

**一键发布**（构建 + `windeployqt` 依赖收集 + Inno Setup 打包）：

```powershell
.\package-cpp-release.ps1                          # 默认 CPU 变体
.\package-cpp-release.ps1 -Variant "GPU-v3.7.0-CUDA-12.9"
.\package-cpp-release.ps1 -NoTools -IncludeWhisper -Variant Clear  # 主程序 + Whisper/VAD 升级包
```

CI 缓存：Qt 保留 install-qt-action 自带缓存；OpenCV、各 OCR 引擎、共享 OCR 模型、Whisper/VAD、videocr-cli、FFmpeg/yt-dlp 和 Inno 翻译使用 GitHub Actions 缓存，下载完成后在编译前保存。FFmpeg/yt-dlp 与翻译按 UTC 周刷新。替换同 URL 的资源需递增工作流 `DEPENDENCY_CACHE_REVISION`；更换 Whisper ZIP 还需更新 `WHISPER_ASSET_SHA256`，VAD 对应更新模型路径、URL 和 SHA256。修改有版本号的资源 URL 时同步修改其缓存键。

CI 使用 MSVC + Ninja + sccache（GitHub Actions 后端）缓存 C/C++ 编译结果，编译器、输入和编译参数变化会影响命中；每次运行末尾输出统计。`package-cpp-release.ps1` 新增可选 `-CompilerLauncher` 参数，本机默认 Visual Studio 方式不变。首次运行仍需完整准备；建议先在 main 上手工运行以建立默认分支缓存，再打发布标签。缓存受 GitHub 的分支作用域、容量和淘汰规则影响，不是永久存储。

当前四个安装包都包含 `tools/Whisper.model/ggml-silero-v6.2.0.bin` 与官方 Vulkan Whisper 运行文件。Clear 使用 `-NoTools -IncludeWhisper`，只更新主程序、Whisper 和 VAD，不包含 OCR 引擎、OCR 模型、FFmpeg、yt-dlp 或 `PADDLEOCR` 标识；用户现有 OCR 资源保留。三种整包仍包含完整工具链，但 Whisper 转录模型（small/medium/large）仍由用户自行提供。`scripts/stage-whisper.ps1` 使用精确清单，排除旧 main.exe、本机备份和下载缓存；工作流对四种包都检查 Whisper/VAD 文件，并校验 VAD 下载的 SHA256。

产物命名规则：`Fairy-Kekkai-Workshop-v{version}-{Variant}-Windows-x86_64-Setup.exe`，其中 `Variant` 取值 `CPU-v3.7.0` / `GPU-v3.7.0-CUDA-11.8` / `GPU-v3.7.0-CUDA-12.9` / `Clear`。

**CI**：`.github/workflows/build-cpp.yml` 是唯一的发布管线。`push: tags: ['v*']` 触发时先做预检（tag 与 `VERSION` 一致、`RELEASE_NOTES.md` 契约），再在 `windows-2022` 上并行构建四个变体，校验产物名与 `PADDLEOCR` 标识后发布 GitHub Release；`workflow_dispatch` 手工触发只构建并上传 artifact，不发布。OpenCV 与 PaddleOCR-Standalone 从 Release 资产获取。

### 发布与更新约定

程序内的「检查更新」完全由 GitHub Release 正文驱动（C++ 见 `cpp/src/service/version_service.cpp`，Python 参考实现见 `app/service/version_service.py`），正文格式因此是有约束的接口，发布时必须满足：

- 正文必须含 `## 更新日志` 与 `## 下载提示` 两个二级标题，下载项必须是 markdown 链接 `[安装包名](url)`，否则客户端解析不到。
- `## 下载提示` 中至少要有一个链接指向该 tag 下的 `Fairy-Kekkai-Workshop-v<版本>-Clear-Windows-x86_64-Setup.exe`（兜底路径），且三个整包与 Clear 包都要作为 Release 资产上传，缺一个就会出现 404。
- **只要本次更新更换了 OCR 引擎，正文末尾必须单独加一行 `!OCRUPDATE!`**（客户端据此改抓与本地机型匹配的整包，标记本身会在 UI 中被剥离）。缺这一行时所有用户都会拿到不含引擎的 Clear 包。
- 安装包名必须保留 `-CPU-` / `-GPU-...-CUDA-11.8-` / `-GPU-...-CUDA-12.9-` 机型分段：跨代次匹配只认这一段，因为 OCR 版本号在换引擎时必然跳变（`v1.5.1` → `v3.7.0` → 下一代）。
- 换引擎的固定动作清单（以下硬编码需同步）：`setting.cpp::paddleOcrSupportFilesName()` 的模型代次、`ocr_migration.cpp` 的 `legacyEngineDirs()`/`legacyModelDirs()`/`legacyArchives()` 清单（引擎目录、模型目录、压缩包另有正则兜底，清单只影响清理报告的文案）、`build-cpp.yml` 的 `PADDLEOCR_BASE_URL` 与 `SUPPORT_ASSET`/`SUPPORT_DIR`，以及它的 `matrix.variant` 变体名（`CPU-v3.7.0` 等，换引擎后要跟着改，否则下载 URL 会 404）。
- 命名三者必须一致：`PADDLEOCR` 第一行 `PaddleOCR-<Variant>`（安装包运行时据此定位 `tools\<Variant>` 与机型）→ 安装包文件名 `Fairy-Kekkai-Workshop-v<version>-<Variant>-Windows-x86_64-Setup.exe`；`package-cpp-release.ps1` 与 `build-cpp.yml` 的半成品目录名也由变体派生，改名时要一并改。
- 发布只有一个入口：`.github/workflows/build-cpp.yml`（`push: tags: ['v*']` + `workflow_dispatch`）。Python 版的两条旧管线 `release.yml` / `deploy-windows.yml` 已删除，不会再出现「给 C++ 版本打 tag 却发布出 Python 产物」的情况。打 tag 前建议本地先跑一遍 `python scripts/check-release-notes.py --version <版本> --check-ocr-update`，它与 CI 预检跑的是同一个脚本：校验 tag 与 `cpp/resources/setting_data.json` 的 `VERSION` 一致、发布说明结构与四个安装包链接齐全、换引擎必须带 `!OCRUPDATE!`；release job 还会比对 `RELEASE_NOTES.md` 里列出的安装包与实际上传的资产是否完全一致，任何一个不满足都会直接失败。

### 项目多标签（当前开发版本，下个版本发布）

- `ProjectStackedInterface` 使用美化库 `qfw::TabBar` + `QStackedWidget`。项目列表是不可关闭标签，每个打开的项目拥有独立 `ProjectDetailInterface`，切换标签保留该页的分页、滚动位置与控件状态。
- 重复打开相同项目切回已有标签；路径使用规范化绝对路径，存在时优先使用 canonical 路径，Windows 下忽略大小写。标签使用独立 routeKey，拖动标签排序不会让页面与标签错配。
- 标签支持关闭、拖动和横向滚动；「＋」返回项目列表以选择更多项目，详情页返回列表不会关闭当前标签。关闭标签不删除项目，不取消已派发到功能页队列的任务；目前不保存标签到下次启动。
- 项目列表的编辑/改名同步标签名称、路径与详情；删除或解除链接关闭对应标签。设置页迁移项目库后，`applyRelocation()` 通过 `project_updated{old_path,path}` 同步已打开标签。
- `TabBar::removeTab()` 的中间索引信号在应用侧屏蔽，删除后按 routeKey 重新选页。关闭按钮操作延迟一个事件循环，详情页的异步加载回调仍绑定自身 QObject 生命周期。
- 本次未编译或运行，需用户验收多项目打开、重复打开、拖动、关闭、改名、删除以及项目库迁移。

### 紧凑项目详情（当前开发版本，下个版本发布）

- 顶部显示项目名、五阶段文件就绪进度、批量任务和添加新集；路径与原标题通过信息按钮展开。刷新、返回列表、批量删除保留在「更多」菜单。
- 分集默认收起，显示选择框、标题及封面/原视频/原字幕/译文/成片五个状态图标，悬停查看名称；展开时才创建原有七种 `FileItemWidget`，原有文件导入、下载、OCR、Whisper、翻译、压制、原文激活和删除功能保持可用。
- 每集的编辑标题、插入、视频链接与删除进入 Fluent 菜单。菜单动作延后一轮执行，并绑定按钮生命周期，防止刷新销毁页面后继续调用旧控件。
- 本页全选支持跨分页累计选择；选择工具栏只在有选中分集时显示。批量任务和文件删除限定在选中分集内，无选择时从顶部或菜单进入则面向整个项目。批量任务仍按原规则筛选可执行项，删除仍需选择文件和确认。
- 分页、展开与选择状态由各详情页独立保存；同页刷新恢复滚动位置，插入/删除导致集号变化时清空编号选择与展开状态，避免误作用于重编号后的其它分集。总进度是五类文件存在率，不代表后台任务执行进度。
- 本次只做源码静态检查，未编译或运行；需用户验收窄窗口、多语言长标题、分页、折叠、批量范围及原文件操作。

### 新手引导（当前开发版本，下个版本发布）

- `cpp/src/components/teaching_tips.*` 在原 Python 引导流程上扩展为 Windows 24 个具体操作点（非 Windows 跳过 OCR/Whisper 六步），覆盖项目目录、新建/导入/播放列表、多标签、下载与筛选、OCR/Whisper 输入和模型、翻译密钥与语言/服务、输出、任务页及重播入口。
- 每步由 route、子页面索引和稳定的控件 objectName 定位。文件浏览按钮、项目操作按钮、下拉框及密钥输入框直接作为 TeachingTip 的 target；设置卡片进一步定位实际按钮。项目引导切回列表，功能引导主动切到相应主页面、任务页或高级设置。
- 显示前将目标滚动到可见区域，箭头优先放在控件下方，空间不足时放在上方；目标加高亮边框。窗口/祖先移动、布局和滚动引起位置变化时重新定位，关闭引导时清理过滤器，控件销毁时解除提示并重新定位。
- 保留上一步、下一步、跳过、完成和应用模态浏览；引导不自动点击业务按钮、创建项目或提交任务。结束写入 `MainWindow/IsFirstRun=false` 并返回主页。首次引导在启动维护后显示，截图自动化跳过；设置页可重播。
- 20 条详细说明统一加入 `app/common/text.py` 并由 `cpp/tools/generate_text.py` 生成 C++ Text；八份 `.ts` 翻译源同步。用户构建前应运行 `lrelease Fairy-Kekkai-Workshop.pro` 更新 `.qm`（C++ qrc 直接引用这些文件）。本次未运行翻译编译、C++ 编译或界面验收。

### 移植对照

`cpp/PORTING_MATRIX.md` 逐文件记录 Python 实现与 C++ 实现的对应关系与差异，移植新功能或修复时应同步维护。

---

## 环境搭建

> 本节描述 **Python 参考实现（`app/`）** 的环境搭建。若需构建 C++ 版主程序，请直接参阅上文「[C++ 原生版架构](#c-原生版架构)」中的构建与打包说明。

### 系统要求
- Python 3.9+
- Windows/macOS/Linux
- 硬件加速器（可选）：用于视频压制加速

### 安装步骤

1. **克隆仓库**
   ```bash
   git clone https://github.com/Fairy-Oracle-Sanctuary/Touhou-translate.git
   cd Touhou-translate/Fairy-Kekkai-Workshop
   ```

2. **创建虚拟环境**（推荐使用 venv）
   ```bash
   python -m venv .venv
   source .venv/bin/activate  # Unix/macOS
   .venv\Scripts\activate     # Windows
   ```

3. **安装依赖**
   ```bash
   pip install -r requirements.txt
   ```

4. **准备 OCR 工具**
   - 安装 VideOCR 依赖：`pip install . --group all`（在 [VideOCR](https://github.com/timminator/VideOCR) 源码目录中）
   - 编译 videocr CLI：`cd app/service/CLI && python deploy.py`（使用 Nuitka 打包为 `videocr-cli.exe`）
   - 将编译产物 `videocr-cli.exe` 复制到 `tools/` 目录
   - 下载 PaddleOCR 模型文件放到 `tools/OCR.model/` 目录

5. **准备 Whisper 工具**（仅 Windows）
   - 下载 Whisper 模型文件（ggml 格式）放到 `tools/Whisper.model/` 目录
   - 编译 Whisper C++ DLL: 使用 Visual Studio 2022 打开 `app/service/CLI/whisper/Whisper.vcxproj` 并编译 Release|x64 配置
   - 编译 WhisperNet CLI: `cd app/service/CLI/whispernet && dotnet publish -c Release -r win-x64 --self-contained`
   - 复制以下文件到 `tools/Whisper/` 目录：
     - 从 `app/service/CLI/whisper/x64/Release/` 复制 Whisper.dll 及其依赖
     - 从 WhisperNet CLI 发布文件夹复制 WhisperNetCLI.exe、WhisperNet.dll、ComLight.dll 等

6. **运行应用**
   ```bash
   python Fairy-Kekkai-Workshop.py
   ```

### 依赖说明

| 包名 | 版本 | 用途 |
|------|------|------|
| PySide6-Fluent-Widgets | 最新 | GUI 框架 |
| opencv-python | 最新 | 图像处理 |
| openai | 最新 | OpenAI/兼容 API |
| numpy | 最新 | 数值计算 |
| Pillow | 最新 | 图像处理 |
| requests | 最新 | HTTP 请求 |
| av | 最新 | 视频处理 |
| fast_ssim | 最新 | 图像相似度计算 |


**外部工具**（需手动准备）：
```bash
# VideOCR CLI（字幕提取）
# 安装 VideOCR 依赖：pip install . --group all
# 编译 videocr CLI：cd app/service/CLI && python deploy.py
# 将 videocr-cli.exe 复制到 tools/ 目录
# 下载 PaddleOCR 模型文件放到 tools/OCR.model/ 目录

# FFmpeg（视频压制）
# Windows: 下载编译版本或通过 scoop/chocolatey
# macOS: brew install ffmpeg
# Linux: sudo apt-get install ffmpeg

# yt-dlp（视频下载）
uv pip install yt-dlp
```

### VideOCR CLI 编译

VideOCR 是基于 Python 的 OCR 工具，支持 PaddleOCR 与 Google Lens 双引擎。本项目使用 Nuitka 将其打包为独立可执行文件 `videocr-cli.exe`。

**前置要求**：
- Python 3.9+
- Nuitka（`pip install nuitka`）
- VideOCR 依赖（`pip install . --group all`，在 [VideOCR](https://github.com/timminator/VideOCR) 源码目录中）
- C++ Build Tools（Nuitka 编译需要，如 Visual Studio 含 "Desktop development with C++" 工作负载）
- 7zip（需在 PATH 中可用）

**编译步骤**：
```bash
# 进入 CLI 目录
cd app/service/CLI

# 使用 Nuitka 打包（默认方式）
python deploy.py

# 或使用 PyInstaller 打包
# 修改 deploy.py 中 main() 调用 run_pyinstaller()
```

**产物位置**：`dist/videocr_cli/videocr-cli.exe`

**部署到生产环境**：
```bash
copy dist/videocr_cli/videocr-cli.exe tools/
```

**测试**：
```bash
# 无参数运行（显示帮助）
tools/videocr-cli.exe --help

# CPU OCR 测试
tools/videocr-cli.exe ^
  --video_path path/to/video.mp4 ^
  --output output.srt ^
  --ocr_engine paddleocr ^
  --lang japan ^
  --use_gpu false ^
  --paddleocr_path tools/PaddleOCR ^
  --supportFilesPath tools/OCR.model
```

**VideOCR CLI 主要参数**：
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--video_path` | （必填） | 视频文件路径 |
| `--output` | `subtitle.srt` | 输出 SRT 文件路径 |
| `--ocr_engine` | `paddleocr` | OCR 引擎（`paddleocr` 或 `google_lens`） |
| `--lang` | `en` | OCR 语言代码 |
| `--conf_threshold` | `75` | 置信度阈值（PaddleOCR 专用，0-100） |
| `--sim_threshold` | `80` | 字幕行合并相似度阈值（0-100） |
| `--ssim_threshold` | `92` | SSIM 帧去重阈值（0-100） |
| `--max_merge_gap` | `0.09` | 最大合并间隔（秒） |
| `--use_fullframe` | `false` | 是否使用完整帧进行 OCR |
| `--use_gpu` | `false` | 是否启用 GPU |
| `--use_angle_cls` | `false` | 是否启用方向分类（PaddleOCR 专用） |
| `--use_server_model` | `false` | 是否使用服务器端模型 |
| `--brightness_threshold` | `None` | 亮度阈值（0-255，用于过滤暗色背景） |
| `--subtitle_position` | `center` | 字幕位置（`center`/`left`/`right`/`any`） |
| `--frames_to_skip` | `1` | 跳帧数 |
| `--post_processing` | `false` | 是否启用后处理（自动插入缺失空格） |
| `--min_subtitle_duration` | `0.2` | 最小字幕持续时间（秒） |
| `--ocr_image_max_width` | `720` | OCR 图像最大宽度（像素） |
| `--crop_x/y/width/height` | `None` | 裁剪区域 1（像素） |
| `--crop_x2/y2/width2/height2` | `None` | 裁剪区域 2（双区域 OCR） |
| `--paddleocr_path` | `None` | PaddleOCR 可执行文件路径 |
| `--supportFilesPath` | `None` | PaddleOCR 模型文件目录 |
| `--tempDir` | `None` | 临时目录路径 |

**源码结构**：
| 文件 | 说明 |
|------|------|
| `videocr_cli.py` | CLI 入口，解析参数并调用 `save_subtitles_to_file` |
| `deploy.py` | Nuitka/PyInstaller 打包脚本 |
| `videocr/__init__.py` | 模块导出 |
| `videocr/api.py` | OCR API（`save_subtitles_to_file`） |
| `videocr/video.py` | 视频处理（帧提取、OCR、字幕生成） |
| `videocr/models.py` | 数据模型 |
| `videocr/utils.py` | 工具函数（SSIM、时间解析、Levenshtein 相似度等） |
| `videocr/lang_dictionaries.py` | PaddleOCR/Google Lens 语言代码映射 |
| `videocr/pyav_adapter.py` | PyAV 视频解码适配器 |

---

## 核心模块说明

### 1. 配置管理（`app/common/config.py`）

配置系统基于 `QConfig`，支持持久化存储。

```python
from app.common.config import cfg

# 读取配置
theme = cfg.get(cfg.themeMode)  # 获取主题模式

# 设置配置
cfg.set(cfg.dpiScale, 1.5)
```

**主要配置项**：
- `dpiScale`：DPI 缩放倍数
- `themeMode`：主题（深色/浅色）
- `downloadFormat`：视频格式
- `downloadQuality`：视频质量
- `promptTemplate`：翻译提示词模板
- `whisperCliPath`：WhisperNetCLI.exe 路径
- `whisperModelPath`：Whisper 模型路径
- AI 模型的 API Key（OpenAI、Deepseek、腾讯混元等）
- `deepseekModel`：Deepseek 模型选择（deepseek-v4-flash/deepseek-v4-pro）
- `deepseekReasoning`：Deepseek 深度思考模式开关
- `concurrentDownloads`：最大并发下载数
- `confidenceThreshold`：OCR 置信度阈值（0-100，默认 75）
- `ssimThreshold`：SSIM 去重阈值（0-100，默认 92）
- `simThreshold`：字幕合并相似度阈值（0-100，默认 80）
- `framesToSkip`：跳帧数（默认 1）
- `useTranslateContext`：是否启用 AI 翻译多轮对话上下文

### 2. 事件总线（`app/common/event_bus.py`）

全局事件分发机制，用于组件间解耦通信。这是整个应用的核心架构组件。

```python
from app.common.event_bus import event_bus

# 发送通知
event_bus.notification_service.show_success("标题", "消息内容")

# 监听事件
event_bus.download_requested.connect(on_download_started)
```

**核心事件信号**：
- `download_requested`：从项目触发下载任务
- `whisper_requested`：从项目触发语音识别任务
- `translate_requested`：从项目触发翻译任务
- `ffmpeg_requested`：从项目触发压制任务
- `add_video_signal`：加载视频到OCR界面
- `whisper_video_load_signal`：加载视频到Whisper界面
- `translate_update_signal`：翻译实时进度更新
- `ffmpeg_update_signal`：压制实时输出更新
- `whisper_update_signal`：语音识别实时输出更新
- `release_update_signal`：上传实时输出更新
- `download_finished_signal`：下载完成通知

**事件数据结构**（`app/common/events.py`）：
```python
@dataclass
class DownloadRequest:
    type: DownloadType
    url: str
    save_path: str
    quality: str = "best"
    project_name: str = ""
    episode_num: int = 0
    metadata: Optional[Dict] = None
```

**使用示例**：
```python
# 从项目详情页触发下载
event_bus.download_requested.emit(
    EventBuilder.download_video(video_url, episode_folder_path)
)

# 翻译界面监听项目请求
event_bus.translate_requested.connect(self.addTranslateFromProject)
```

### 3. 项目管理（`app/service/project_service.py`）

管理本地项目文件结构，支持多格式字幕和视频。这是整个应用的核心数据管理层。

```python
from app.service.project_service import project

# 创建项目
project.creat_files("项目名", 12, "原标题")

# 删除项目
project.delete_project("/path/to/project")

# 获取项目信息
progress = project.get_project_progress(project_id)

# 添加新集
project.addEpisode(card_id, episode_num, origin_title, trans_title, video_url, isTranslated)

# 删除集
project.deleteEpisode(card_id, episode_num)

# 修改集标题
project.change_subtitle(card_id, num, text, offset=0)

# 获取相邻文件路径
Project.get_previous_path(file_path)
Project.get_next_path(file_path)
```

**项目文件结构**：
```
项目名/
├── 标题.txt          # 存储项目元数据、每集标题、视频URL
├── icon.txt          # 项目图标路径
├── {原名}.txt        # 项目标识文件
└── 1/                # 第1集文件夹
    ├── 封面.jpg          # 从YouTube自动下载
    ├── 生肉.mp4          # 原始视频（下载）
    ├── 熟肉.mp4          # 嵌入字幕的视频（压制）
    ├── 原文.srt          # 原文字幕（手动/下载）
    ├── 原文_OCR.srt      # OCR提取的字幕
    ├── 原文_Whisper.srt  # 语音识别的字幕
    └── 译文.srt          # AI翻译的字幕
```

**标题.txt 格式**：
```
1
2
3
...
12

1
第1集标题
https://www.youtube.com/watch?v=xxx

2
第2集标题
https://www.youtube.com/watch?v=yyy

---
```

**项目进度追踪**：
- 返回5个维度的完成百分比：[封面, 原视频, 熟肉, 原字幕, 译文]
- 自动扫描所有集文件夹统计文件存在情况

**外部项目链接**：
- 支持链接外部目录作为项目
- 通过 `cfg.linkProject` 配置

### 4. 翻译服务（`app/service/translate_service.py`）

> 本节代码示例及 SDK 限制描述旧版 Python 实现。当前 C++ 版在 `cpp/src/service/translate_service.cpp` 的 `resolveProvider()` 中接入 Deepseek、GLM、Spark、混元、书生、ERNIE、Gemini 和自定义服务，统一通过 Qt HTTP 客户端调用；Spark、GLM 未因旧版 SDK 限制而禁用。实际请求能否成功取决于密钥、网络及服务端模型权限。

支持多个 AI 模型的流式翻译。

```python
from app.service.translate_service import TranslateThread, TranslateTask

# 创建翻译任务
task = TranslateTask(args={
    "srt_path": "/path/to/subtitle.srt",
    "output_path": "/path/to/output.srt",
    "origin_lang": "Japanese",
    "target_lang": "Chinese",
    "AI": "deepseek",
    "temperature": 0.7,
})

# 执行翻译
thread = TranslateThread(task)
thread.finished_signal.connect(on_finished)
thread.start()
```

**支持的 AI 模型**：
- ✅ Deepseek（最推荐，支持 v4-flash/v4-pro 模型切换和深度思考模式）
- ✅ 腾讯混元（HunyuanTurbos）
- ✅ 百度 ERNIE Speed 128K
- ✅ 书生（InternLM）
- ✅ Google Gemini 3 Flash
- ⚠️ 讯飞 Spark Lite（旧版 Python SDK 不兼容；C++ 版已接入 HTTP API）
- ⚠️ GLM-4.5 Flash（旧版 Python SDK 不兼容；C++ 版已接入 HTTP API）
- ✅ 自定义模型（兼容 OpenAI API 格式）

**Deepseek 专属功能**：
- 模型选择：`deepseek-v4-flash`（快速）或 `deepseek-v4-pro`（高质量）
- 深度思考模式：启用后模型会进行更深入的推理

**多轮对话上下文**：
- 启用时，系统将最近 2 轮对话历史附加到当前请求中，帮助 AI 保持术语一致性
- 禁用时，每次请求独立，适合短字幕或不同话题的字幕

**SRT 多行处理**：
- 发送给 AI 前，字幕内的换行符被替换为空格，确保一行一条
- 避免多行字幕导致 AI 编号错乱（旧 bug：AI 将第二行当成独立条目）

### 5. OCR 服务（`app/service/ocr_service.py`）

基于 [VideOCR](https://github.com/timminator/VideOCR) 的字幕提取服务，支持 PaddleOCR 与 Google Lens 双引擎，支持 GPU 加速和多参数调节。

```python
from app.service.ocr_service import OCRProcess, OCRTask

# 创建 OCR 任务
task = OCRTask(args={
    "video_path": "/path/to/video.mp4",
    "file_path": "/path/to/output.srt",
    "temp_dir": "/path/to/temp",
    "lang": "ja",
    "paddleocr_path": "tools/PaddleOCR",
    "supportFilesPath": "tools/OCR.model",
    "confidence_threshold": 75,    # 置信度阈值 (0-100)
    "ssim_threshold": 92,          # SSIM 去重阈值 (0-100)
    "sim_threshold": 80,           # 字幕合并相似度 (0-100)
    "frames_to_skip": 1,           # 跳过的帧数
    "ocr_image_max_width": 720,    # OCR 图像最大宽度
    "use_gpu": True,               # 是否使用 GPU
})

# 执行 OCR
process = OCRProcess(task)
process.finished_signal.connect(on_finished)
process.start()
```

**OCR 流程**：
1. 视频帧提取和 SSIM 去重过滤
2. 文本检测（PaddleOCR DBNet）
3. 文本识别（PaddleOCR CRNN 或 Google Lens）
4. 置信度过滤（丢弃低质量识别结果）
5. 字幕生成和合并（Levenshtein 相似度）

**OCR 高级参数说明**：

| 参数 | 默认值 | 范围 | 说明 |
|------|--------|------|------|
| `ssim_threshold` | 92 | 0-100 | SSIM 阈值，越高越保守（不去重），越低越激进（多去重） |
| `sim_threshold` | 80 | 0-100 | 字幕合并相似度阈值（Levenshtein），越高越不易合并 |
| `frames_to_skip` | 1 | 0+ | 每 N 帧取一帧，0=每帧都取（最慢但最全） |
| `ocr_image_max_width` | 720 | 1+ | OCR 输入图像最大宽度（像素） |
| `confidence_threshold` | 75 | 0-100 | 置信度过滤阈值（PaddleOCR 专用），低于此值的识别结果被丢弃 |
| `brightness_threshold` | None | 0-255 | 亮度阈值，用于过滤暗色背景噪点（None=禁用） |
| `max_merge_gap` | 0.09 | 0.0+ | 最大合并间隔（秒），增大可合并更多断句 |
| `min_subtitle_duration` | 0.2 | 0.0+ | 最小字幕持续时间（秒） |
| `post_processing` | false | - | 后处理：自动插入缺失空格（英语/西语等） |

**参数调整建议**：
- **漏句** → 降低 `frames_to_skip`，提高 `ssim_threshold`，降低 `ocr_image_max_width`
- **误识别多** → 提高 `confidence_threshold` 至 80~90
- **字幕断句多** → 提高 `sim_threshold` 至 85~95，增大 `max_merge_gap` 至 0.2~0.5

### 6. Whisper 语音识别服务（`app/service/whisper_service.py`）

以下是旧 Python / Const-me 服务的历史参考。当前 C++ 服务 `cpp/src/service/whisper_service.cpp` 使用官方 whisper.cpp v1.9.4，先经 FFmpeg 转音频，再执行 Silero VAD 与识别；配置、源码准备及编译说明见 `cpp/WHISPER.md`。

```python
from app.service.whisper_service import WhisperProcess, WhisperTask

# 创建 Whisper 任务
task = WhisperTask(args={
    "video_path": "/path/to/video.mp4",
    "output_path": "/path/to/output.srt",
    "model": "tools/Whisper.model/ggml-model-whisper-large.bin",
    "language": "auto",  # auto 表示自动检测，或指定 zh/ja/en
    "format": "srt",
    "gpu": "自动检测",
})

# 执行识别
process = WhisperProcess(task)
process.finished_signal.connect(on_finished)
process.start()
```

**Whisper CLI 进度输出格式**：
- CLI 输出 `PROGRESS:XX` 表示识别进度（XX 为百分比）
- 服务层解析进度并更新 UI 进度条
- 语言为 `auto` 时不传 `--language` 参数，让 Whisper 自动检测

**支持的语言**：
- 自动检测、中文、日语、英语、韩语、法语、德语、西班牙语

**输出格式**：
- SRT（字幕文件）
- TXT（纯文本）
- VTT（WebVTT格式）

### 7. 日志系统（`app/common/logger.py`）

结构化日志，自动保存到 AppData。支持在主页「关于」卡片中清空日志。

```python
from app.common.logger import Logger

logger = Logger("ModuleName", "category")
logger.info("消息")
logger.warning("警告")
logger.error("错误信息")
```

**日志位置**：`AppData/Log/`

### 8. 启动页（`app/view/main_window.py` / `cpp/src/view/main_window.cpp`）

带进度条和状态文字的启动页面，在应用初始化时显示。

```python
class LoadingSplashScreen(SplashScreen):
    def __init__(self, icon, parent=None):
        super().__init__(icon, parent)
        self.progressBar = ProgressBar(self, useAni=False)  # 禁用动画以支持同步进度
        self.statusLabel = BodyLabel("正在启动...", self)

    def setProgress(self, value: int, text: str = None):
        self.progressBar.setValue(value)
        if text:
            self.statusLabel.setText(text)
        QApplication.processEvents()
```

**加载阶段**：
- 10%: 初始化服务
- 30%: 读取设置
- 50%: 加载界面
- 80%: 初始化系统托盘
- 100%: 启动完成

**C++ 版对应实现**：

- 基础组件（美化库）：`third_party/Qt-Fluent-Widgets/qtfluentwidgets/window/splash_screen.h` / `splash_screen.cpp`，即 `qfw::SplashScreen`，1:1 复刻 `libs/qfluentwidgets_pro/window/splash_screen.py`。默认图标尺寸 `QSize(96, 96)`；图标阴影 `rgba(0,0,0,50)`、blur 15、offset `(0, 4)`（由构造参数 `enableShadow` 控制）；背景色随主题为 `32`/`255` 的纯色；对 `parent()` 安装事件过滤器，`Resize` 时自适应父窗口大小、`ChildAdded` 时重新置顶；macOS 隐藏标题栏；`finish()` 即 `close()`。库中新增文件需在 `qtfluentwidgets/CMakeLists.txt` 的 window 段与 `qtfluentwidgets.h` 的 Fluent Window 段登记。
- 应用层启动页：`cpp/src/view/main_window.cpp` 的 `LoadingSplashScreen`（声明在 `cpp/src/view/main_window.h`），用法与 Python 一致——禁用进度条动画（`qfw::ProgressBar(this, false)`）、宽度 320、状态文字使用 `Text` 单例、`setProgress()` 末尾调用 `QApplication::processEvents()` 使同步初始化期间进度即时刷新；主窗口构造中依次在 10/30/50/80/100 五个阶段更新进度，最后调用 `splashScreen_->finish()` 关闭启动页。
- 注意：Python 版 `setIcon()` 只替换内部图标并 `update()`，不会同步已存在的 `IconWidget`（只有 `setIconSize()` 才会改动图标控件尺寸）；C++ 版按原样保留该行为。

### 9. 主题切换（`app/view/main_window.py`）

标题栏最小化按钮左侧的主题切换按钮。

```python
def _initThemeButton(self):
    self.themeButton = TransparentToolButton(self.titleBar)
    self.themeButton.setFixedSize(self.titleBar.minBtn.size())
    self._updateThemeButtonIcon()
    self.themeButton.clicked.connect(self._toggleTheme)
    self.titleBar.buttonLayout.insertWidget(0, self.themeButton, 0, Qt.AlignTop)

def _toggleTheme(self):
    theme = Theme.LIGHT if isDarkTheme() else Theme.DARK
    cfg.set(cfg.themeMode, theme)
    setTheme(theme)
    self._updateThemeButtonIcon()
```

**图标逻辑**：
- 深色模式：显示太阳图标（切到浅色）
- 浅色模式：显示月亮图标（切到深色）

### 10. 多语言系统（`app/common/text.py`、`app/resource/i18n/`）

当前版本支持 9 种语言界面：简体中文、英语、日语、韩语、德语、西班牙语、法语、葡萄牙语、繁体中文，使用 Qt Linguist 管理翻译资源。当前项目统一通过 `Text` 类集中维护 UI 文案，业务代码应访问 `self.globalText.<属性名>`，避免散落的 `self.tr(...)` 或 `QCoreApplication.translate(...)`。

**核心文件**：
- `app/common/text.py` - 集中定义全部可翻译 UI 文案
- `app/resource/i18n/app.en_US.ts` - 英文翻译源文件
- `app/resource/i18n/app.ja_JP.ts` - 日语翻译源文件
- `app/resource/i18n/app.ko_KR.ts` - 韩语翻译源文件
- `app/resource/i18n/*.qm` - 运行时加载的编译后翻译文件
- `app/resource/resource.qrc` - Qt 资源清单
- `app/resource/resource_rc.py` - Qt 资源编译后的 Python 文件

**Text 使用规范**：
```python
from ..common.text import Text

class ExampleInterface(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.globalText = Text()
        self.titleLabel = TitleLabel(self.globalText.OCRSettings, self)
```

**禁止写法**：
```python
self.tr("开始时间")
QCoreApplication.translate("Example", "开始时间")
```

**新增文案流程**：
1. 在 `app/common/text.py` 的 `Text.__init__()` 中新增属性，例如 `self.StartTime = self.tr("开始时间")`。
2. 在界面或服务代码中通过 `self.globalText.StartTime` 使用。
3. 运行 `lupdate` 更新 `.ts` 文件。
4. 补全 `.ts` 中各语言 `<translation>`。
5. 运行 `lrelease` 生成 `.qm`。
6. 若 `.qm` 被打进 Qt 资源，重新生成 `app/resource/resource_rc.py`。

**翻译构建命令**：
```powershell
# 提取需要翻译的字符串
lupdate.exe Fairy-Kekkai-Workshop.pro

# 生成全部 .qm 翻译文件
lrelease.exe Fairy-Kekkai-Workshop.pro

# 如果 pyside6-lrelease 包装器异常，可直接调用 PySide6 工具
& "C:\Users\<User>\AppData\Local\Programs\Python\Python39\lib\site-packages\PySide6\lrelease.exe" app\resource\i18n\app.en_US.ts -qm app\resource\i18n\app.en_US.qm

# 重新生成 Qt 资源 Python 文件
& "C:\Users\<User>\AppData\Local\Programs\Python\Python39\lib\site-packages\PySide6\rcc.exe" app\resource\resource.qrc -o app\resource\resource_rc.py -g python
```

**语言配置**：
- 默认语言：中文（`Language.CHINESE_SIMPLIFIED`）
- 配置项：`cfg.language`（在 `app/common/config.py`）
- 运行时资源路径：`:/app/i18n/app.en_US.qm` 等

**状态文本规范**：
- `TaskStatus` 的枚举值必须保持稳定代码值，例如 `done`、`failed`，不能改成界面显示语言。
- 任务界面显示文本统一通过 `status_text()` 获取。
- `status_text()` 应在调用时即时创建 `Text()`，避免在翻译器安装前缓存中文文案。

**语言代码与显示文本规范**：
- 配置项保存稳定代码值，例如 OCR 使用 `japan`，翻译语言使用 `ja` / `zh`，AI 模型使用 `deepseek`。
- UI 下拉框显示文本使用 `self.globalText` 生成，例如 `{"ja": self.globalText.Japanese}`。
- 传给 OCR CLI 的 `lang` 必须是 OCR 语言代码，例如 `japan`。
- 传给 AI Prompt 的 `origin_lang` / `target_lang` 应转换为语言显示名，例如 `日语`、`中文`。
- 传给翻译服务选择器的 `AI` 必须保持服务 key，例如 `deepseek`，不能转成 `Deepseek` 或其它显示名。

**占位符要求**：
- `.ts` 翻译中的 `{}` 数量必须与 Python `.format(...)` 参数数量一致。
- 示例：`self.globalText.FilesAllFiles.format(self.file_extension)` 只传一个参数，所以翻译也只能有一个 `{}`。

**检查命令**：
```powershell
# 编译全部 Python 文件，检查语法错误
python -m compileall -q app

# 检查动态翻译调用残留
rg "self\.tr\(|QCoreApplication\.translate" app -g "*.py"
```

### 11. 批量任务系统（`app/components/dialog.py`）

批量任务对话框支持一次性为多集添加相同类型的任务。

**支持的批量任务类型**：
- 下载：有URL且无生肉.mp4的集
- 语音识别：有生肉.mp4且无原文_Whisper.srt的集
- 翻译：有原文字幕且无译文.srt的集
- 压制：有熟肉.mp4的集

**智能筛选机制**：
```python
def _check_eligible(self, task_type, folder_num, folder_path):
    """检查某集是否可添加该类型任务"""
    if task_type == "下载":
        video_url = project.project_video_url[self.card_id][idx]
        has_raw = os.path.exists(os.path.join(raw, "生肉.mp4"))
        return video_url and not has_raw
    elif task_type == "翻译":
        for src_name in ("原文.srt", "原文_OCR.srt", "原文_Whisper.srt"):
            if os.path.exists(os.path.join(raw, src_name)):
                return not os.path.exists(os.path.join(raw, "译文.srt"))
```

**使用流程**：
1. 在项目详情页点击"批量任务"按钮
2. 选择任务类型（下载/语音识别/翻译/压制）
3. 系统自动筛选符合条件的剧集
4. 勾选需要处理的剧集（支持全选/取消全选）
5. 点击"添加任务"，系统通过event_bus派发任务

**每张小卡片的快速加任务**（`app/view/project_detail_interface.py::FileItemWidget`、`cpp/src/components/file_item_widget.*`）：

详情页每个文件小卡片上都有快捷按钮，直接投递任务，无需先切页再选文件。

| 按钮 | 出现条件 | Python | C++ |
| --- | --- | --- | --- |
| OCR提取字幕 | 生肉.mp4 存在 | `add_video_signal` 让字幕界面装载视频，再 `switchToSampleCard("VideocrStackedInterfaces", 3)` 切页 | `add_video_signal`（由 `VideocrInterface` 接管并回填输入/输出路径）+ `navigation_requested{target:"ocr"}` 路由切页 |
| 语音识别 | 生肉.mp4 存在 | `whisper_requested(生肉.mp4, 原文_Whisper.srt)` | 同 |
| 翻译字幕 | 译文.srt 缺失且存在任一原文 | `translate_requested(原文.srt, 译文.srt)` | 同；原文.srt 缺失时回退到实际存在的原文（与批量任务 `dispatchTask` 一致） |
| 视频压制 | 熟肉.mp4 存在 | `ffmpeg_requested(熟肉.mp4, 熟肉_压制.mp4)` | 同 |

### 11. 文件映射系统（`app/components/base_function_interface.py`）

自动识别输入文件类型，生成对应的输出文件名。

**特殊文件名映射**：
```python
self.special_filename_mapping = {
    "生肉.mp4": "原文_OCR.srt",      # OCR界面
    "生肉.mp4": "原文_Whisper.srt",   # Whisper界面
    "原文.srt": "译文.srt",           # 翻译界面
    "熟肉.mp4": "封面.jpg",          # 上传界面
}
```

**相邻文件导航**：
- 支持快速访问上一集/下一集的相同类型文件
- 用于批量处理时的快速切换

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    主窗口 (MainWindow)                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │  启动页       │  │  导航栏       │  │  主题切换     │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
├─────────────────────────────────────────────────────────┤
│                    功能界面层 (View)                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ 项目管理  │ │ 视频下载  │ │ 字幕提取  │ │ 语音识别  │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ 智能翻译  │ │ 视频压制  │ │ 设置页面  │ │ 主页      │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
├─────────────────────────────────────────────────────────┤
│                    业务逻辑层 (Service)                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ 项目服务  │ │ 下载服务  │ │ 翻译服务  │ │ OCR服务   │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐               │
│  │ Whisper   │ │ FFmpeg    │ │ SRT处理   │               │
│  └──────────┘ └──────────┘ └──────────┘               │
├─────────────────────────────────────────────────────────┤
│                    事件总线 (EventBus)                   │
│           组件间解耦通信的核心协调层                      │
├─────────────────────────────────────────────────────────┤
│                    公共模块 (Common)                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │ 配置管理  │ │ 日志系统  │ │ 事件定义  │ │ 样式表    │ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ │
└─────────────────────────────────────────────────────────┘
```

### 核心设计模式

**1. 事件驱动架构**
- 使用 PySide6 的 Signal/Slot 机制
- 通过 event_bus 实现全局事件分发
- 模块间完全解耦，易于扩展

**2. 异步处理模式**
- 所有耗时操作使用 QThread 异步执行
- 避免阻塞 UI 线程
- 支持任务取消和进度更新

**3. 单例模式**
- 项目管理服务使用单例
- 配置管理使用单例
- 事件总线使用单例

**4. 模板方法模式**
- BaseFunctionInterface 定义功能界面模板
- BaseStackedInterfaces 定义堆叠界面模板
- 子类实现具体功能

### 数据流

**典型工作流数据流**：
```
项目详情页 → event_bus.download_requested → 下载界面 → DownloadService → yt-dlp
     ↓
文件生成 → 项目进度自动更新 → UI刷新
     ↓
用户点击"语音识别" → event_bus.whisper_requested → Whisper界面 → WhisperService → Whisper CLI
     ↓
字幕生成 → event_bus.translate_requested → 翻译界面 → TranslateService → AI API
     ↓
译文生成 → event_bus.ffmpeg_requested → 压制界面 → FFmpegService → FFmpeg
```

---

## 开发指南

### 添加新的 AI 翻译模型

1. **在 `app/service/translate_service.py` 中添加服务类**：

```python
class MyNewModelService(BaseTranslateService):
    def get_client(self):
        return OpenAI(
            api_key=cfg.get(cfg.myNewModelApiKey),
            base_url="https://api.example.com/v1",
        )

    def get_model_name(self) -> str:
        return "my-new-model"
```

2. **在 `SERVICES` 字典中注册**：

```python
SERVICES = {
    # ... 其他模型
    "my-new-model": MyNewModelService,
}
```

3. **在 `app/common/config.py` 中添加配置项**：

```python
myNewModelApiKey = ConfigItem(
    "MyNewModel", "ApiKey", "", restart=False
)
```

4. **在设置页面中添加 UI**（`app/view/setting_interface.py`）

### 添加新的项目功能

1. **在 `app/service/` 中创建服务类**
2. **在 `app/view/` 中创建对应的 UI 界面**
3. **通过 `event_bus` 连接服务与 UI**

**示例：添加新的功能界面**

```python
# 1. 创建功能界面类（继承 BaseFunctionInterface）
class MyFunctionInterface(BaseFunctionInterface):
    def __init__(self, parent=None):
        super().__init__(parent, "功能名称")
        self.file_extension = "*.mp4"
        self.default_output_suffix = "_output.mp4"

    def get_input_icon(self):
        return FIF.VIDEO

    def _create_settings_cards(self):
        # 添加设置卡片
        pass

    def _start_processing(self):
        # 处理逻辑
        args = self._get_args()
        self.addTask.emit(args)

    def _get_args(self):
        # 获取参数
        return {}

# 2. 创建堆叠界面类（继承 BaseStackedInterfaces）
class MyFunctionStackedInterfaces(BaseStackedInterfaces):
    def __init__(self, parent=None):
        super().__init__(
            parent=parent,
            main_interface_class=MyFunctionInterface,
            task_interface_class=MyTaskInterface,
            setting_interface_class=MySettingInterface,
            interface_name="功能名称",
        )

# 3. 在主窗口中注册
# 在 main_window.py 的导航栏中添加新的导航项
```

### 从项目触发功能

如果新功能需要从项目详情页触发：

1. **在事件总线中添加信号**（`app/common/event_bus.py`）：
```python
my_function_requested = Signal(str, str)  # input_path, output_path
```

2. **在项目详情页中添加触发逻辑**（`app/view/project_detail_interface.py`）：
```python
def _dispatch_task(self, task_type, folder_num, folder_path):
    if task_type == "我的功能":
        input_path = os.path.join(raw, "生肉.mp4")
        output_path = os.path.join(raw, "输出.mp4")
        event_bus.my_function_requested.emit(input_path, output_path)
```

3. **在功能界面中监听事件**：
```python
def _connect_signals(self):
    super()._connect_signals()
    event_bus.my_function_requested.connect(self.addTaskFromProject)

def addTaskFromProject(self, input_path, output_path):
    self.file_path = input_path
    self.inputFileCard.lineEdit.setText(input_path)
    self.outputFileCard.lineEdit.setText(output_path)
```

### 添加批量任务支持

如果新功能需要批量任务支持：

1. **在 BatchTaskDialog 中添加任务类型**（`app/components/dialog.py`）：
```python
TASK_TYPES = ["下载", "语音识别", "翻译", "压制", "我的功能"]
```

2. **添加筛选逻辑**：
```python
def _check_eligible(self, task_type, folder_num, folder_path):
    if task_type == "我的功能":
        has_input = os.path.exists(os.path.join(raw, "生肉.mp4"))
        has_output = os.path.exists(os.path.join(raw, "输出.mp4"))
        return has_input and not has_output
```

3. **添加派发逻辑**（`app/view/project_detail_interface.py`）：
```python
def _dispatch_task(self, task_type, folder_num, folder_path):
    elif task_type == "我的功能":
        input_path = os.path.join(raw, "生肉.mp4")
        output_path = os.path.join(raw, "输出.mp4")
        event_bus.my_function_requested.emit(input_path, output_path)
```

### 跨平台注意事项

- ✅ 使用 `pathlib.Path` 处理路径（自动适配 Windows/Unix）
- ✅ 使用 `subprocess` 执行外部工具时注意平台差异
- ❌ 避免硬编码路径分隔符（如 `\` 或 `/`）
- ❌ 避免 Windows 特定的 API（如 `os.environ["QT_SCALE_FACTOR"]`）

---

## 常见问题

### Q: 应用启动时显示 Shiboken 警告

**A**: 这是 PySide6 的正常警告，不影响功能。可以安全忽略。

### Q: 字幕提取失败

**A**:
1. 确保 `videocr-cli.exe` 存在于 `tools/` 目录
2. 确保 PaddleOCR 模型文件存在于 `tools/OCR.model/` 目录
3. 检查 VC++ 运行时是否已安装（需要 MSVCP140.dll 和 VCRUNTIME140.dll）
4. 检查 GPU 驱动是否支持 CUDA（如使用 GPU）
5. 如果使用 Google Lens 引擎，确保网络连接正常

### Q: 翻译功能不可用

**A**:
- 确保已配置相应 AI 服务的 API Key（在设置页面）
- 旧版 Python 的 Spark、GLM 曾受 SDK 兼容性限制；当前 C++ 版已接入其 HTTP API，请检查密钥、网络及模型访问权限
- 推荐使用 Deepseek 或腾讯混元（支持较好）
- Deepseek 深度思考模式会增加推理时间，但翻译质量更高

### Q: Whisper 语音识别失败

**A**:
1. 确保 WhisperNetCLI.exe 存在于 `tools/Whisper/` 目录
2. 确保所有依赖 DLL（Whisper.dll、WhisperNet.dll、ComLight.dll）在同一目录
3. 确保 Whisper 模型文件存在于 `tools/Whisper.model/` 目录
4. 语言设置为 `auto` 时，CLI 会自动检测语言
5. 检查 GPU 驱动是否支持 DirectML（如使用 GPU）

### Q: 视频压制很慢

**A**:
1. 使用硬件加速（需 FFmpeg 支持）：配置 `-hwaccel cuda` 或 `-hwaccel videotoolbox`
2. 降低视频质量或分辨率
3. 使用更快的编码器（`libx264` → `libx265` 或 `av1`）

### Q: 批量任务添加失败

**A**:
1. 检查项目文件结构是否完整（标题.txt、集文件夹）
2. 确保筛选条件正确（如下载任务需要视频URL）
3. 检查文件路径是否包含中文字符（某些工具不支持）

### Q: 项目进度显示不正确

**A**:
1. 刷新项目列表（点击"刷新项目列表"按钮）
2. 检查文件命名是否符合规范（生肉.mp4、译文.srt等）
3. 确保文件在正确的集文件夹中

### Q: 从项目触发任务没有反应

**A**:
1. 检查 event_bus 信号是否正确连接
2. 确保目标界面已初始化
3. 查看日志文件（AppData/Log/）获取详细错误信息

---

## 已知限制

| 功能 | 状态 | 备注 |
|------|------|------|
| 视频下载 | ✅ | 基于 yt-dlp，支持大多数平台 |
| 字幕提取 | ✅ | 基于 [VideOCR](https://github.com/timminator/VideOCR)，支持 PaddleOCR/Google Lens 引擎，仅 Windows |
| 语音识别 | ✅ | 官方 whisper.cpp v1.9.4 + Silero VAD，应用当前仅 Windows，支持实时进度 |
| 翻译 | ✅ | C++ 版通过 HTTP API 接入多个 AI 服务；旧版 Python SDK 限制见翻译服务章节 |
| 视频压制 | ✅ | 基于 FFmpeg，支持多种编码器 |
| B站上传 | ⚠️ | 功能已实现但因 API 版权问题未正式启用 |
| 实时预览 | ❌ | 当前不支持 |
| 批量处理 | ✅ | 支持批量任务，智能筛选

---

## 性能优化建议

1. **减少 UI 更新频率**：使用定时器而非直接更新
2. **缓存配置**：避免频繁读写 `config.json`
3. **异步处理**：所有长时间操作使用 QThread
4. **内存管理**：及时释放大对象引用

---

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- 使用 4 空格缩进
- 遵循 PEP 8
- 为公共 API 编写文档字符串
- 添加类型提示（Python 3.10+）

---

## 许可证

详见仓库根目录的 LICENSE 文件。

---

## 更新日志

> **格式约定**：CI（`.github/workflows/build-cpp.yml`）会把仓库根目录的
> `RELEASE_NOTES.md` 原样写入 GitHub Release body，`VersionService.getUpdateInfo()`
> （`app/service/version_service.py`）再从 body 中按以下规则解析：
>
> - `## 更新日志`：展示在更新对话框中。**其后必须紧跟另一个 `## ` 二级标题**，
>   解析正则为 `## 更新日志\n(.*?)(?=\n## )`，若作为末节会导致尾部 lookahead
>   失败、内容无法被提取。
> - `## 下载提示`：提取其中的 `[名称](URL)` Markdown 链接作为安装包下载源，
>   正则 `## 下载提示\n(.*?)(?=\n# |\Z)` 含 `\Z` 兜底，可作为末节。
> - 末尾可独占一行追加 `!OCRUPDATE!` 标记，触发按本地 `PADDLEOCR_VERSION`
>   匹配对应 CPU/GPU 安装包的逻辑；不需要时省略。
>
> 发布新版本前，请同步更新本节与 `RELEASE_NOTES.md`，确保二级标题与上述约定一致。

### v3.0.0（2026-09-29）

#### 重大变化 / Breaking Changes
- 主程序从 Python + PySide6 全量重写为 C++17 + Qt 6 + Qt-Fluent-Widgets 原生桌面应用，源码位于 `cpp/`
- 安装包不再附带 Python 运行时与依赖，旧版本升级可使用 Clear 包，无需因主程序大版本变化而卸载；Clear 不含外部工具与模型，需另行保留或补齐所需资源
- 新增 `cpp/` 原生源码树（`common` / `components` / `service` / `view`），原 Python 实现保留在 `app/` 作为移植对照

#### 新增 / Added
- 悬浮截图取词窗口 `cpp/src/components/floating_window.*`：屏幕区域框选 → OCR → AI 翻译，支持窗口绑定与跟随、置顶、锁定、鼠标穿透、背景透明与历史记录
- 屏幕一次性翻译 Runner `ScreenTranslateRunner`（`cpp/src/service/translate_service.*`），复用 Python 同款提示词与流式接口
- 启动维护 `cpp/src/components/startup_maintenance.*`：首次启动自动把软件目录中的项目迁移到独立数据目录，模态展示进度
- 旧版资源清理 `cpp/src/service/legacy_cleanup.*`：清理上一代 OCR 资源与 Python(Nuitka + PySide6) 残留，释放磁盘占用
- C++ 打包链路 `package-cpp-release.ps1`（CMake 构建 → windeployqt → Inno Setup）与发布管线 `.github/workflows/build-cpp.yml`（打 tag 自动发布 CPU / GPU CUDA 11.8 / CUDA 12.9 / Clear 四个变体）
- 移植对照表 `cpp/PORTING_MATRIX.md`

#### 修复 / Fixed
- 修复框选区域在高 DPI 屏幕下坐标偏移（按 `devicePixelRatioF()` 换算）
- 修复悬浮窗关闭后入口按钮未恢复的问题（`ocr_window_closed` → 重新启用）
- 修复 `qfw::FluentIcon` 缺少隐式 `QIcon` 转换导致的 `setIcon` 编译失败（统一改用 `.qicon()`）
- 修复 `qfw::RoundMenu` 菜单回调在菜单销毁后访问已释放内存导致的崩溃（`QTimer::singleShot(0, ...)` 延后一个事件循环）
- 修复换 OCR 引擎后「检查更新」匹配不到整包的问题（`version_service` 改为三级匹配：精确标识 → 机型/算力分段 → CPU 兜底）
- 修复跨代次升级时旧引擎压缩包漏清的问题（`ocr_migration.cpp` 增加正则兜底，不再依赖写死的清单）

#### 改进 / Improved
- OCR 引擎升级到 PaddleOCR-Standalone v3.7.0，安装包与 CI 变体同步更新
- 版本号统一由 `cpp/resources/setting_data.json` 的 `VERSION` 派生
- 屏幕 OCR / 屏幕翻译统一到 `QObject + QRunnable` + 全局事件总线架构
- 发布管线全面转向 C++：`build-cpp.yml` 改为 tag 触发并接管 GitHub Release 发布（预检 → 四变体并行构建 → 产物与标识校验 → 发布），下线 Python 版 `release.yml` / `deploy-windows.yml`
- 新增 `scripts/check-release-notes.py`：打 tag 前后都可校验 Release 正文契约（结构、四个安装包链接、链接版本号、换引擎必须带 `!OCRUPDATE!`），CI 预检与本地手动发布共用同一套规则

### v2.5.2（2026-08-13）

#### 重构 / Refactored
- Whisper 语音识别服务迁移至 `QRunnable` + `TaskInterface` 架构，统一任务调度与生命周期管理
- 翻译服务迁移至 `TaskInterface` 模式，复用基类批量操作与事件总线收口
- OCR 任务界面统一到 `TaskInterface` 基类，新增 `getTaskGeneratedFiles` 钩子供子类声明任务产物文件
- `BaseTaskInterface` 收口任务调度、事件总线、批量操作与卡片清理逻辑
- 国际化文案统一收敛到 `Text` 类（`app/common/text.py`），移除散落的 `self.tr()` 调用

#### 新增 / Added
- 空状态卡片组件 `EmptyStatusWidget`：任务列表为空时展示引导界面与 Logo
- 跨平台文件操作工具 `app/common/utils.py`：封装 `showInFolder` / `openUrl`

#### 修复 / Fixed
- 修复基类 `_removeCard` 清理不一致导致布局与卡片映射泄漏的问题
- 修复任务完成后「在文件夹中显示」功能（对齐 Easy-FFmpeg 实现，跨平台可用）

#### 改进 / Improved
- 完善各服务日志输出
- 任务卡片按钮提示与确认对话框文案全部纳入国际化管理

---

## 技术栈

- **主程序（C++ 版）**：C++17 + Qt 6 + Qt-Fluent-Widgets，CMake 3.21+ / MSVC 2022 构建
- **图像处理**：OpenCV 4.12
- **UI 框架（Python 参考实现）**：PySide6 + QFluentWidgets
- **视频处理**：FFmpeg + yt-dlp
- **字幕识别**：[VideOCR](https://github.com/timminator/VideOCR)（PaddleOCR / Google Lens）
- **语音识别**：[ggml-org/whisper.cpp](https://github.com/ggml-org/whisper.cpp)（旧 Python 参考采用 Const-me）
- **翻译**：多个云 API（OpenAI、Deepseek、腾讯混元等）
- **B 站上传**：Bilibili API
- **配置存储**：JSON + SQLite
- **日志**：内置 Logger
- **打包**：windeployqt + Inno Setup（C++ 版）/ uv + Nuitka（Python 版）
- **包管理**：uv（Python 参考实现）

---

**最后更新**：2026 年 8 月 16 日  
**维护者**：`Baby2016` `镀铬酸钾`
