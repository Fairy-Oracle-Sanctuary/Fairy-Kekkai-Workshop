#pragma once

#include <QMutex>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <memory>

#include "common/logger.h"
#include "service/task_base.h"

class QProcess;

namespace fkw {

// 对应 app/service/ffmpeg_service.py 的 FFmpegTask：纯数据类，字段对齐
// Easy-FFmpeg。公共字段（taskId/fileName/outputName/logPath/createTime）在
// TaskBase 中，这里只补充压制专有参数。Python 中任务对象被界面/卡片/Worker
// 共享引用，这里用 shared_ptr 表达同一语义。
struct FFmpegTask : TaskBase {
    FFmpegTask(const QString& videoPath, const QString& outputPath);

    QString saveFolder;
};

// 检测输入文件是否有音频流（同步探测，5 秒超时）。
// 与 Python 一致：只允许在 Worker 线程内调用，避免主线程阻塞。
bool probeHasAudio(const QString& videoPath);

// 根据配置修正输出文件扩展名（纯函数，无 I/O）。
QString adjustOutputFormat(const QString& outputFile);

// 根据配置构建 FFmpeg 参数列表（纯函数，不做任何探测）。
// hasAudio 必须由调用方显式传入（在 Worker 线程内探测）。
// 返回 {参数列表(不含可执行文件路径), 修正扩展名后的输出文件}。
QPair<QStringList, QString> buildFfmpegCommand(const QString& videoPath,
                                               const QString& outputFile,
                                               bool hasAudio);

// FFmpeg 压制执行引擎（对齐 Easy-FFmpeg：QEventLoop 同步化 + event_bus 上报）。
// 由 QThreadPool 调度并在 run() 结束后 deleteLater() 回收，
// 外部若需持有引用请使用 QPointer。
class FFmpegWorker : public TaskWorker {
    Q_OBJECT
public:
    explicit FFmpegWorker(std::shared_ptr<FFmpegTask> task);
    ~FFmpegWorker() override;

    void run() override;

    // 取消任务：标记并 kill 当前进程。
    // 取消的任务不 emit finishTaskSig（状态已由任务界面设为 Cancelled）。
    void cancel() override;

private:
    bool runStage(const QStringList& args);
    void tryParseDuration(const QString& data);
    void parseProgress(const QString& data);
    void handleStderr();
    void finish(bool success);
    // 回收当前进程并返回其退出码（进程不存在时返回 -1）。
    int destroyProcess();

    std::shared_ptr<FFmpegTask> task_;
    double duration_ = 0.0;
    double lastEmit_ = 0.0;
    QSharedPointer<Logger> taskLogger_;
    QProcess* process_ = nullptr;
    QMutex processMutex_;
    // 编码开始前累积 stderr，供时长解析反复求和所有 Duration
    QString stderrBuffer_;
    bool durationFrozen_ = false;
};

}  // namespace fkw
