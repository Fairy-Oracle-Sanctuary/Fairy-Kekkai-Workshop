#pragma once

#include <QList>
#include <QString>
#include "components/base_stacked_interface.h"
#include "components/task_card.h"
#include "service/download_service.h"

class QVBoxLayout;
class QNetworkAccessManager;
class QNetworkReply;
namespace fkw {
// 下载条目状态（下载页内部使用）
enum class PreviewTaskStatus { Waiting, Processing, Succeeded, Failed, Cancelled };
class DownloadItemWidget;
struct DownloadEntry;
class DownloadInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit DownloadInterface(QWidget* parent = nullptr);
    ~DownloadInterface() override;
private:
    void addDownloadTask(const DownloadRequest& request);
    void startNextDownloads();
    void retryDownload(DownloadEntry* entry);
    void removeTask(DownloadEntry* entry);
    void updateTaskUI(DownloadEntry* entry);
    void filterTasks(const QString& filter);
    void updateYtDlp();
    QList<DownloadEntry*> tasks_;
    QWidget* emptyRegion_ = nullptr;
    QWidget* taskRegion_ = nullptr;
    QVBoxLayout* taskList_ = nullptr;
    qfw::PushButton* updateButton_ = nullptr;
    QNetworkAccessManager* network_ = nullptr;
    QString filter_ = QStringLiteral("all");
    int nextId_ = 0;
    bool updating_ = false;
};
class DownloadStackedInterface : public BaseStackedInterfaces {
    Q_OBJECT
public:
    explicit DownloadStackedInterface(QWidget* parent = nullptr);
};
}
