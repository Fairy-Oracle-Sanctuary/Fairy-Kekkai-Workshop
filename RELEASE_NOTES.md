## 更新日志

### 新增 / Added
- 项目管理支持标签页，可同时打开多个项目，并分别切换和关闭。
  Project tabs allow multiple projects to stay open, with independent switching and closing.
- 新增组件定位的新手引导，说明具体按钮、输入框和操作步骤；可在设置中重新播放。
  Added a guided tutorial anchored to actual controls, with detailed steps and replay from Settings.
- Whisper 切换到 whisper.cpp，支持 Vulkan GPU 推理与 CPU 推理；新增 Silero VAD 语音检测及相关设置，减少静音片段干扰。
  Migrated to whisper.cpp with Vulkan GPU and CPU inference, adding Silero VAD and related settings to reduce interference from silent sections.

### 改进 / Improved
- 项目详情页改用可折叠内容和分页，减少页面拥挤，保留批量操作。
  Project details use collapsible content and pagination to reduce clutter while retaining batch actions.
- 视频预览支持拖动过程中持续更新，并合并待处理取帧请求；松手后定位最终位置。复杂编码视频的更新速度仍受解码耗时影响。
  Video previews update during dragging and coalesce pending frame requests, then seek to the final position on release. Complex codecs remain limited by decoding speed.
- 四种安装包均包含新版 Whisper 运行文件及 `ggml-silero-v6.2.0.bin`；Clear 包也包含这两项更新。Whisper 语音识别模型需自行保留或下载。
  All four installers include the new Whisper runtime and `ggml-silero-v6.2.0.bin`, including Clear. Whisper speech recognition models must be retained or downloaded separately.
- 发布工作流增加依赖缓存与编译缓存，减少后续构建的重复下载和编译耗时。
  Release workflows cache dependencies and compilation results to reduce repeated work in subsequent builds.

### 修复 / Fixed
- 启动时迁移软件目录中的项目，也包含文件不完整的项目，减少损坏项目遗漏。
  Startup migration also relocates incomplete projects from the installation directory.
- 新版 Whisper 文件齐全时，启动维护自动清理旧版 Whisper 可执行文件及旧依赖。
  Startup maintenance removes obsolete Whisper executables and dependencies when the new runtime is complete.
- 缩短主页 OCR 版本显示，完整信息保留在提示中，避免文字过长导致布局溢出。
  Shortened the home OCR version label, retaining full details in its tooltip to prevent layout overflow.

## 下载提示

| 平台 / Platform | 类型 / Type | 安装包 / Installer |
| --- | --- | --- |
| Windows 10/11 | CPU | [Fairy-Kekkai-Workshop-v3.1.0-CPU-v3.7.0-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.1.0/Fairy-Kekkai-Workshop-v3.1.0-CPU-v3.7.0-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 11.8) | [Fairy-Kekkai-Workshop-v3.1.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.1.0/Fairy-Kekkai-Workshop-v3.1.0-GPU-v3.7.0-CUDA-11.8-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 12.9) | [Fairy-Kekkai-Workshop-v3.1.0-GPU-v3.7.0-CUDA-12.9-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.1.0/Fairy-Kekkai-Workshop-v3.1.0-GPU-v3.7.0-CUDA-12.9-Windows-x86_64-Setup.exe) |
| Windows 10/11 | Clear 增量包 | [Fairy-Kekkai-Workshop-v3.1.0-Clear-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v3.1.0/Fairy-Kekkai-Workshop-v3.1.0-Clear-Windows-x86_64-Setup.exe) |

- 从 3.0.0 升级可使用 Clear 包，保留已安装的 PaddleOCR v3.7.0 引擎与 OCR 模型。CPU / GPU 类型指 PaddleOCR 变体；四种包中的 Whisper 均支持 Vulkan GPU 与 CPU 推理。
  Upgrade from 3.0.0 using Clear while retaining PaddleOCR v3.7.0 and its models. CPU/GPU package names refer to PaddleOCR variants; Whisper supports Vulkan GPU and CPU inference in all four packages.
- 从 2.x（Python 版）升级也可使用 Clear，无需因主程序大版本变化而卸载。Clear 不含 PaddleOCR 引擎与 OCR 模型；如需更换到 v3.7.0 引擎，可选择对应整包。启动维护会迁移项目并清理旧资源。
  Clear also supports upgrades from 2.x without uninstalling solely because of a major application version change. Clear excludes PaddleOCR and OCR models; choose a full package to obtain the v3.7.0 engine. Startup maintenance migrates projects and cleans obsolete resources.
- Whisper 的 Vulkan 版本需要可用的 Vulkan 驱动与运行库；CPU 模式可在设置中关闭 GPU 推理。
  The Vulkan Whisper runtime requires a working Vulkan driver and loader. Disable GPU inference in Settings to use CPU mode.
- macOS 版本无变动，请使用上一版对应安装包。
  macOS is unchanged; use the previous macOS package.

## 使用说明 / Usage

- Windows：根据 PaddleOCR 所需的 CPU / CUDA 版本选择安装包，升级安装前关闭正在运行的软件。
  Windows: select the appropriate PaddleOCR CPU/CUDA package and close the application before upgrading.
- 新手引导可在设置中重新播放。使用 Whisper 前需配置语音识别模型路径；VAD 模型随安装包提供。
  Replay the tutorial from Settings. Configure a speech recognition model before using Whisper; the VAD model is included.
