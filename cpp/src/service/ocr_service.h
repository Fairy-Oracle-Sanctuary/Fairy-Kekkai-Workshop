#pragma once

#include <QMutex>
#include <QRect>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <memory>
#include <optional>

#include "common/logger.h"
#include "service/task_base.h"

class QProcess;

namespace fkw {

// 对应 app/service/ocr_service.py 的 OCRTask：公共字段（taskId/fileName/
// outputName/logPath/createTime）在 TaskBase 中，这里补充 OCR 专有参数快照
//（对应 Python 任务对象里的 args 字典，创建时取配置，运行期不变）。
struct OcrTask : TaskBase {
    OcrTask(const QString& videoPath, const QString& outputPath);

    QString lang;               // --lang
    QString timeStart;          // --time_start
    QString timeEnd;            // --time_end（为空则不传）
    bool useGpu = true;         // --use_gpu
    bool useAngleCls = false;   // --use_angle_cls
    bool useServerModel = true;  // --use_server_model
    bool useDualZone = false;   // --use_dual_zone
    int simThreshold = 65;      // --sim_threshold
    double maxMergeGapSec = 0.1;     // --max_merge_gap
    double ssimThreshold = 90;       // --ssim_threshold
    int ocrImageMaxWidth = 1280;     // --ocr_image_max_width
    int framesToSkip = 2;            // --frames_to_skip
    bool postProcessing = false;     // --post_processing
    double minSubtitleDurationSec = 0.2;  // --min_subtitle_duration
    int confidenceThreshold = 30;    // --conf_threshold
    // Python 侧硬编码项（_get_args：use_fullframe=False / subtitle_position="center"）
    bool useFullframe = false;
    QString subtitlePosition = QStringLiteral("center");

    QString paddleocrPath;      // --paddleocr_path（为空则不传）
    QString supportFilesPath;   // --supportFilesPath（为空则不传）
    QString tempDir;            // --tempDir（为空则不传；也是运行前清理的临时目录）

    // 裁剪区域（对应 Python args["--crop_*"]，来自视频预览的框选，像素坐标）
    QVector<QRect> cropRects;
};

// 构建 videocr-cli 参数列表（纯函数，不含可执行文件路径）。
QStringList buildOcrCommand(const OcrTask& task);

// OCR 字幕提取执行引擎（对齐 Python OCRWorker：QEventLoop 同步化 + event_bus
// 上报，进度按三段式输出换算 0-33/33-53/53-66/66-100）。取消使用 taskkill
// 强杀进程树（videocr-cli 会派生子进程）；取消的任务不 emit finishTaskSig。
class OcrWorker : public TaskWorker {
    Q_OBJECT
public:
    explicit OcrWorker(std::shared_ptr<OcrTask> task);
    ~OcrWorker() override;

    void run() override;
    void cancel() override;

private:
    // 处理合并后的 stdout（进度 + 日志，对齐 Python _handle_stdout）。
    void handleStdout();
    // Step 1/3 相同进度行去重（对齐 Python _should_emit_line）。
    bool shouldEmitLine(const QString& line);
    // 解析三段式输出映射为 0-100 进度；非进度行返回空（对齐 _parse_progress）。
    std::optional<int> parseProgress(const QString& line);
    // 判断是否为错误输出行（对齐 Python _is_error_line）。
    static bool isErrorLine(const QString& line);
    // 将提取结果复制为 原文.srt（对齐 Python _activate_as_current）。
    void activateAsCurrent(const QString& outputFile, const QString& inputFile);
    // 回收当前进程并返回其退出码（进程不存在时返回 -1）。
    int destroyProcess();
    void finish(bool success);

    std::shared_ptr<OcrTask> task_;
    QSharedPointer<Logger> taskLogger_;
    QProcess* process_ = nullptr;
    QMutex processMutex_;
    QStringList outputLines_;        // 存储输出用于错误诊断
    QString lastStep1Progress_;      // Step 1/3 去重（Python _last_step1_progress）
    int lastProgress_ = -1;
};

// 屏幕区域 OCR（对应 Python ScreenOCRThread）：截取屏幕指定区域 → 保存临时
// 图片 → paddleocr.exe ocr → 解析 ppocr INFO 文本 → 通过 event_bus 上报。
// 悬浮窗（floating_window）尚未移植，本类先作为可复用组件落地：调用方用
// QThreadPool 启动，run() 结束自动 deleteLater()（外部持有引用请用 QPointer）。
class ScreenOcrRunner : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit ScreenOcrRunner(const QRect& rect, QObject* parent = nullptr);
    ~ScreenOcrRunner() override;

    void run() override;
    void cancel();

private:
    void execute();
    QRect rect_;  // 屏幕全局坐标
    std::atomic<bool> cancelled_{false};
    QSharedPointer<Logger> logger_;
};

}  // namespace fkw
