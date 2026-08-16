## 更新日志

### 重构 / Refactored
- Whisper 语音识别服务迁移至 QRunnable + TaskInterface 架构，统一任务调度与生命周期管理
  Whisper speech recognition migrated to QRunnable + TaskInterface architecture
- 翻译服务迁移至 TaskInterface 模式，复用基类批量操作与事件总线
  Translation service migrated to TaskInterface pattern
- OCR 任务界面统一到 TaskInterface 基类，新增 getTaskGeneratedFiles 钩子
  OCR task interface unified under TaskInterface base class with getTaskGeneratedFiles hook
- 国际化文案统一收敛到 Text 类，移除散落的 self.tr() 调用
  i18n strings consolidated into the Text class, removing scattered self.tr() calls
- 主程序与 videocr CLI 依赖隔离，新建 paddleocr 副本断开 import 牵连，避免打包时带入 av/fast_ssim/scipy 等重型依赖
  Isolated main app from videocr CLI dependencies via a paddleocr copy, preventing av/fast_ssim/scipy from being pulled into the build

### 新增 / Added
- 空状态卡片组件：任务列表为空时展示引导界面
  Empty status card component showing guidance when the task list is empty
- 跨平台文件操作工具，封装 showInFolder / openUrl
  Cross-platform file utility wrapping showInFolder / openUrl
- SingletonApplication 单例机制：基于 QSharedMemory + QLocalServer 实现进程间通信，第二个实例启动时自动激活主窗口
  SingletonApplication via QSharedMemory + QLocalServer: second instance activates the main window through IPC

### 修复 / Fixed
- 修复基类 _removeCard 清理不一致导致布局与卡片映射泄漏的问题
  Fixed layout and card mapping leak caused by inconsistent _removeCard cleanup
- 修复任务完成后「在文件夹中显示」功能
  Fixed "Show in Folder" after task completion
- 修复 PaddleOCR 模型路径正反斜杠混合导致找不到模型文件的问题
  Fixed PaddleOCR model path not found due to mixed forward/backward slashes
- 修复 SegmentedWidget 误传 QWidget 作为 routeKey 触发 Shiboken copy-convert 警告
  Fixed Shiboken copy-convert warning caused by passing QWidget as routeKey to SegmentedWidget
- 修复 _currentFilter 用 currentItem() 与 QWidget 比较永远为 False 导致标签页过滤失效的问题
  Fixed tab filter always returning "all" due to currentItem() vs QWidget comparison always being False

### 改进 / Improved
- 完善各服务日志输出
  Improved logging across services
- 任务卡片按钮提示与确认对话框文案全部纳入国际化管理
  Task card tooltips and confirmation dialogs fully internationalized
- 过滤 qfluentwidgets 内部 QFont::setPointSize(-1) 无害警告，保持控制台输出整洁
  Filter qfluentwidgets internal QFont::setPointSize(-1) harmless warnings for cleaner console output

## 下载提示

| 平台 / Platform | 类型 / Type | 安装包 / Installer |
| --- | --- | --- |
| Windows 10/11 | CPU | [Fairy-Kekkai-Workshop-v2.5.2-CPU-v1.5.1-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.2/Fairy-Kekkai-Workshop-v2.5.2-CPU-v1.5.1-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 11.8, Nvidia 10 系列) | [Fairy-Kekkai-Workshop-v2.5.2-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.2/Fairy-Kekkai-Workshop-v2.5.2-GPU-v1.5.1-CUDA-11.8-Windows-x86_64-Setup.exe) |
| Windows 10/11 | GPU (CUDA 12.9, Nvidia 16 - 50 系列) | [Fairy-Kekkai-Workshop-v2.5.2-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.2/Fairy-Kekkai-Workshop-v2.5.2-GPU-v1.5.1-CUDA-12.9-Windows-x86_64-Setup.exe) |

- 如果你已安装过上个版本（增量升级包）：
  If you have installed the previous version (incremental upgrade package):
  [Fairy-Kekkai-Workshop-v2.5.2-Clear-Windows-x86_64-Setup.exe](https://github.com/Fairy-Oracle-Sanctuary/Fairy-Kekkai-Workshop/releases/download/v2.5.2/Fairy-Kekkai-Workshop-v2.5.2-Clear-Windows-x86_64-Setup.exe)
- mac 版本无变动，直接下载上一个版本即可
  macOS version unchanged, download the previous version directly
- 迅雷链接 / Thunder Drive: https://pan.xunlei.com/s/VOl2n0KP6LH3zXUqcYX1iYUAA1?pwd=yzim#

## 使用说明 / Usage

- **Windows**：根据显卡选择对应版本运行安装包，按向导完成安装（需管理员权限）。
  Choose the version matching your GPU and run the installer, then follow the setup wizard (administrator privileges required).
