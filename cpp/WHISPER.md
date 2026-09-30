# 官方 Whisper 引擎与 VAD

当前 C++ 应用调用官方 [ggml-org/whisper.cpp v1.9.4](https://github.com/ggml-org/whisper.cpp/releases/tag/v1.9.4)，固定源码提交 `927cfce34f31707e17f2bff35c349632fb9e2c3a`。`app/service/CLI/whisper` 是旧 Const-me 引擎的历史参考，当前应用不再调用它。

## 准备源码和模型

在仓库根目录执行：

```powershell
python cpp/tools/setup_whisper.py
```

该脚本只下载源码与 `tools/Whisper.model/ggml-silero-v6.2.0.bin`，不编译。源码目录 `third_party/whisper.cpp` 被 Git 忽略，可由脚本重建。脚本验证固定的源码提交，遇到已有其它版本会报错，不会覆盖工作区。

Whisper 转录模型与 Silero VAD 模型是两个不同文件。已有 `ggml-model-whisper-medium.bin` / `ggml-model-whisper-large.bin` 可在高级设置中选择；实际加载兼容性以新引擎运行结果为准。需要官方多语言 small 模型时：

```powershell
python cpp/tools/setup_whisper.py --download-small
```

此项额外下载约 466 MB，文件是 `tools/Whisper.model/ggml-small.bin`，下载后在高级设置中选择它。

## 编译与放置（由开发者执行）

独立编译引擎，不并入 Qt 应用的 CMake 工程。以下是 CPU 版本：

```powershell
cmake -S third_party/whisper.cpp -B build/whisper -DWHISPER_BUILD_TESTS=OFF
cmake --build build/whisper --config Release --target whisper-cli
```

NVIDIA CUDA 版本在配置命令中加 `-DGGML_CUDA=ON`，需要 CUDA Toolkit；Vulkan 版本加 `-DGGML_VULKAN=ON`，需要 Vulkan SDK。更换后端时使用不同构建目录，例如 `build/whisper-cuda`、`build/whisper-vulkan`。GPU 开关只控制调用方式，不能给 CPU 构建增加 GPU 能力。

将生成的 `whisper-cli.exe` 和本次构建所需的 DLL 放到 `tools/whisper/`，或在高级设置里选择编译结果的 `whisper-cli.exe`。不要仅给旧 `main.exe` 改名；旧 Const-me DLL 不能替代新引擎的 DLL。打包时同时带上 Silero VAD 模型，转录模型可独立提供。

旧配置中指向本软件 `tools/whisper/main.exe` 的默认路径，会在读取时映射到新的 `tools/whisper/whisper-cli.exe`。其它自定义路径需要手动选择新引擎。

## 识别流程与默认设置

1. FFmpeg 将视频/音频的第一条音轨转为 16 kHz 单声道 PCM WAV，放入任务独占的系统临时目录。
2. 官方 CLI 执行 Silero VAD，仅识别检测到的语音。默认阈值 `0.5`、分段静音 `500 ms`、最长语音片段 `30 s`，边界填充 `200 ms`、重叠 `100 ms`。
3. 默认 `--max-context 0`，减少先前错误文本对后续识别的影响；高级设置可关闭此选项以恢复文本上下文。
4. 官方 VAD 将识别时间映射回原音频时间轴，软件不自行删去静音后拼接时间轴。
5. 结果通过 `-of` 写入临时目录，确认正常退出且结果文件存在后，原子写入用户输出路径。取消或失败不替换已有字幕；临时文件自动清理。

输出支持 SRT/TXT/VTT，扩展名跟随所选格式。仅 SRT 可作为项目当前 `原文.srt`。旧配置里的 JSON 输出映射到 SRT。CPU 模式明确传 `--no-gpu`；启用 GPU 时由引擎自动选择可用后端设备，不使用旧 DirectCompute 显卡名称参数。

VAD 不能保证完全滤除音乐，也可能漏掉轻声或短促话语。漏字时可降低阈值或关闭 VAD 对照；误检过多时可提高阈值。临时 WAV 需要约 115 MB/小时磁盘空间，官方 CLI 还会将音频读入内存。

## 验证

本机已安装 Vulkan SDK `1.4.363.0`，位于 `D:\VulkanSDK\1.4.363.0`。用 Visual Studio 2022 / MSVC 19.44 编译 Vulkan + CPU Release 版本，构建目录 `build/whisper-vulkan/`。配置命令为：

```powershell
$env:VULKAN_SDK = 'D:\VulkanSDK\1.4.363.0'
cmake -S third_party/whisper.cpp -B build/whisper-vulkan -G "Visual Studio 17 2022" -A x64 -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF -DGGML_CUDA=OFF -DGGML_VULKAN=ON -DBUILD_SHARED_LIBS=ON
cmake --build build/whisper-vulkan --config Release --target whisper-cli --parallel 8
```

已部署 `whisper-cli.exe`、`whisper.dll`、`ggml.dll`、`ggml-base.dll`、`ggml-cpu.dll`、`ggml-vulkan.dll` 到 `tools/whisper/`，六个文件的 SHA256 均与构建产物一致。原 CPU 引擎备份在 `tools/whisper/backup-cpu-20260930-190830/`；更早的 Const-me `main.exe` 和 `Whisper.dll` 在 `tools/whisper/backup-20260930-185346/`。

已有 medium 模型和 Silero VAD 均可加载。官方 JFK 样本前置 5 秒静音后，Vulkan GPU 模式与部署后的 `--no-gpu` CPU 模式均正常退出并输出相同字幕，SRT 从 `00:00:05,140` 开始。GPU 日志确认 `using Vulkan0 backend`，设备为 NVIDIA GeForce RTX 4060 Laptop GPU；CPU 日志确认 `use gpu = 0`。日志与字幕在 `build/whisper-smoke/vulkan.log`、`vulkan-cpu.log` 及相应 SRT 中。

发布此构建时，用户无需安装开发 SDK，但需要显卡驱动提供 Vulkan 支持和 `vulkan-1.dll`。此构建的 DLL 使用链接式 Vulkan 后端，即使传 `--no-gpu`，启动仍需要 Vulkan Loader；缺少 Loader 的系统可使用保留的纯 CPU 引擎包。CPU 后端使用 AVX2，较老 CPU 的兼容性需另行构建验证。此次仅在本机 NVIDIA GPU 验证，没有在 AMD / Intel 显卡测试。短样本测试只确认功能正确，不代表长视频速度或精度已改善。

Qt 应用没有重新编译，长视频精度尚未测量。其余待验证：长静音视频、纯静音、无音轨、中文/空格路径、TXT/VTT、取消转换/转录及 VAD 开关。翻译源已更新，应用编译前运行 `lrelease Fairy-Kekkai-Workshop.pro`。
