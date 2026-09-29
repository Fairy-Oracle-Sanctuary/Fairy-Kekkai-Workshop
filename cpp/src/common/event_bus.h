#pragma once
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariant>
#include <QWidget>

namespace fkw {
// Signal names and payloads mirror app/common/event_bus.py.
class GlobalEventBus : public QObject {
    Q_OBJECT
public:
    static GlobalEventBus& instance();
    QPointer<QWidget> projectInterface;
    QPointer<QWidget> projectDetailInterface;
    bool isShuttingDown = false;
signals:
    void appMessageSig(const QString& message);
    void switchToSampleCard(const QString& route, int index);
    void openUrl(const QString& url);
    void checkUpdateSig();

    void updateTaskStatusSig(int taskId, int progress, const QVariant& status,
                             const QString& size, double time,
                             const QString& bitrate, double speed);
    void finishTaskSig(int taskId, bool success, const QString& logPath);
    void deleteTaskSig(int taskId, bool deleteFile);
    void cancelTaskSig(int taskId);
    void retryTaskSig(int taskId);
    void taskStageChangedSig(int taskId, const QString& stage);
    void taskCountChanged(int count);
    void hasFailedTasks(bool failed);
    void taskLogSignal(const QString& logName, const QString& message,
                       bool isError, bool isFlush);

    void download_requested(const QJsonObject& event);
    void download_progress(const QJsonObject& event);
    void download_finished_signal(bool success, const QString& path);
    void download_list_finished_signal(bool success, const QString& path);
    void ocr_finished_signal(bool success, const QString& path);
    void add_video_signal(const QString& path);

    void translate_finished_signal(bool success, const QStringList& result);
    void translate_requested(const QString& input, const QString& output);
    void translate_update_signal(const QString& taskId, const QString& chunk);
    void ffmpeg_finished_signal(bool success, const QString& path);
    void ffmpeg_requested(const QString& input, const QString& output);
    void ffmpeg_update_signal(const QString& taskId, const QString& chunk);
    void release_finished_signal(bool success, const QString& path);
    void release_requested(const QString& path);
    void release_update_signal(const QString& taskId, const QString& chunk);
    void whisper_finished_signal(bool success, const QString& path);
    void whisper_requested(const QString& input, const QString& output);
    void whisper_video_load_signal(const QString& path);
    void whisper_update_signal(const QString& taskId, const QString& chunk);

    void project_created(const QJsonObject& project);
    void project_deleted(const QJsonObject& project);
    void project_updated(const QJsonObject& project);
    void fileDeletedSignal();
    void file_operation(const QJsonObject& operation);
    void navigation_requested(const QJsonObject& navigation);
    void log_window_closed();
    void log_message(const QString& logName, const QString& message);
    void logsCleared();
    void notification(const QJsonObject& message);
    void ocr_window_closed();
    void screen_ocr_started();
    void screen_ocr_finished(bool success, const QString& result);
    void screen_ocr_log(const QString& line);
    void screen_translate_finished(bool success, const QString& result);
private:
    GlobalEventBus() = default;
};
}  // namespace fkw
