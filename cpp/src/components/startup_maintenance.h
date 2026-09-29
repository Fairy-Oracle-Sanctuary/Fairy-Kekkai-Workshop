#pragma once

#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <qtfluentwidgets.h>

#include "components/dialog.h"
#include "service/legacy_cleanup.h"
#include "service/ocr_migration.h"
#include "service/project_relocate.h"

class QThread;

namespace fkw {
// 启动时的一次性维护：先把软件目录里的老项目搬到独立数据目录，再清掉旧版 OCR 资源，
// 最后清掉上一代 Python 版遗留在安装目录里的依赖。
// 三件事都没必要做时不弹任何窗；只展示真正需要执行的部分。
// 返回项目搬迁结果，供调用方回写链接表与排序表；未搬迁时 records 为空。
projects::RelocateReport runStartupMaintenance(QWidget* parent);

// 后台任务：在同一个线程里串行完成「搬迁项目」「清理旧版 OCR 资源」「清理上一代残留」
// 三个阶段，全程通过信号把进度送回界面线程，不阻塞启动。
class StartupMaintenanceWorker : public QObject {
    Q_OBJECT
public:
    StartupMaintenanceWorker(QStringList projectSources, QString projectTarget, bool cleanOcr,
                             bool cleanLegacy, QObject* parent = nullptr);
public slots:
    void run();
signals:
    void projectItemStarted(const QString& source, int index, int total);
    void projectProgressed(int percent);
    void projectItemFinished(const QString& source, const QString& destination, bool ok,
                             const QString& message);
    void ocrScanStarted();
    void ocrItemStarted(const QString& name, int index, int total);
    void ocrProgressed(int percent);
    void ocrItemFinished(const QString& path, bool ok, const QString& reason);
    void ocrFinished(int removed, int failed, qint64 freedBytes);
    void legacyScanStarted();
    void legacyItemStarted(const QString& name, int index, int total);
    void legacyProgressed(int percent);
    void legacyItemFinished(const QString& path, bool ok, const QString& reason);
    void legacyFinished(int removed, int failed, qint64 freedBytes);
    void done();
private:
    QStringList projectSources_;
    QString projectTarget_;
    bool cleanOcr_ = false;
    bool cleanLegacy_ = false;
};

// 启动维护对话框：模态展示整体进度，过程中忽略一切关闭操作
class StartupMaintenanceDialog : public BaseInputDialog {
    Q_OBJECT
public:
    StartupMaintenanceDialog(const QStringList& projectSources, const QString& projectTarget,
                             bool cleanOcr, bool cleanLegacy, QWidget* parent = nullptr);
    ~StartupMaintenanceDialog() override;
    const projects::RelocateReport& report() const { return report_; }
    void start();
protected:
    void reject() override;
private:
    void appendLog(const QString& text);
    // 把某个阶段的百分比映射进它在整条进度条上占用的区间
    void setProgress(const QPair<int, int>& window, int percent);
    void finish();
    projects::RelocateReport report_;
    QStringList sources_;
    bool cleanOcr_ = false;
    bool cleanLegacy_ = false;
    QPair<int, int> projectWindow_{0, 100};
    QPair<int, int> ocrWindow_{0, 100};
    QPair<int, int> legacyWindow_{0, 100};
    int total_ = 0;
    int failed_ = 0;
    int ocrRemoved_ = 0;
    int ocrFailed_ = 0;
    qint64 ocrFreedBytes_ = 0;
    int legacyRemoved_ = 0;
    int legacyFailed_ = 0;
    qint64 legacyFreedBytes_ = 0;
    bool running_ = true;
    qfw::SubtitleLabel* heading_ = nullptr;
    qfw::BodyLabel* currentLabel_ = nullptr;
    qfw::ProgressBar* bar_ = nullptr;
    qfw::PlainTextEdit* log_ = nullptr;
    QThread* thread_ = nullptr;
};
} // namespace fkw
