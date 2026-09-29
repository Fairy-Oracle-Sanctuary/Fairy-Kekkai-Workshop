#pragma once

#include <QMutex>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <memory>
#include <optional>

#include "common/logger.h"
#include "service/task_base.h"

class QProcess;

namespace fkw {

// 对应 app/service/whisper_service.py 的 WhisperTask：公共字段（taskId/fileName/
// outputName/logPath/createTime）在 TaskBase 中，这里补充转录专有参数。
struct WhisperTask : TaskBase {
    WhisperTask(const QString& videoPath, const QString& outputPath);

    QString modelFile;                    // -m 模型文件路径
    QString language;                     // -l 语言；空或 "auto" 表示自动检测
    QString format = QStringLiteral("srt");  // -osrt/-otxt/-ovtt
    QString gpu;                          // 非空时附加 -gpu（含"自动检测"）
};

// 获取 whisper main.exe 路径（对齐 Python get_whisper_cli_path）：
// 配置路径存在则用之，否则回退仓库内 tools/whisper/main(.exe)。
QString getWhisperCliPath();

// 构建 Whisper main.exe 参数列表（纯函数，不含可执行文件路径）。
QStringList buildWhisperCommand(const WhisperTask& task);

// Whisper 转录执行引擎（对齐 Python WhisperWorker：QEventLoop 同步化 +
// event_bus 上报，进度按时间戳行换算百分比）。取消使用 kill 直接终止；
// 取消的任务不 emit finishTaskSig。由 QThreadPool 调度并在 run() 结束后
// deleteLater() 回收，外部若需持有引用请使用 QPointer。
class WhisperWorker : public TaskWorker {
    Q_OBJECT
public:
    explicit WhisperWorker(std::shared_ptr<WhisperTask> task);
    ~WhisperWorker() override;

    void run() override;
    void cancel() override;

private:
    // 探测视频总时长（秒），失败返回 0（对齐 Python _get_video_duration）。
    double probeVideoDuration();
    // 处理合并后的 stdout（进度 + 日志，对齐 Python _handle_stdout）。
    void handleStdout();
    // 解析时间戳行计算进度（0-100）；完成标志返回 100；非进度行返回空。
    std::optional<int> parseProgress(const QString& line);
    // Whisper CLI 无 -o 参数：输出自动生成在输入文件旁，重命名为任务输出，
    // 再按条件复制为 原文.srt（设为当前活动原文）。
    void activateOutput();
    // 将提取结果复制为 原文.srt（对齐 Python _activate_as_current）。
    void activateAsCurrent(const QString& outputFile, const QString& inputFile);
    // 回收当前进程并返回其退出码（进程不存在时返回 -1）。
    int destroyProcess();
    void finish(bool success);

    std::shared_ptr<WhisperTask> task_;
    double duration_ = 0.0;
    int lastProgress_ = -1;
    QSharedPointer<Logger> taskLogger_;
    QProcess* process_ = nullptr;
    QMutex processMutex_;
    QStringList outputLines_;  // 存储输出用于错误诊断
};

}  // namespace fkw
