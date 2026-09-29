#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace fkw::ocr {
// 配置里被自动纠正的一项 OCR 路径
struct PathFix {
    QString key;       // config 项名：PaddleocrPath / supportFilesPath
    QString previous;  // 纠正前的旧值
    QString current;   // 纠正后的新值
};

// 一处需要清理的旧版 OCR 资源
struct ObsoleteResource {
    QString path;      // 绝对路径
    QString name;      // 条目名
    QString reason;    // 判定为旧版的原因，用于日志与界面提示
    qint64 bytes = 0;  // 占用空间，用于展示释放量
    bool directory = true;  // true：目录；false：文件（工作流下载的压缩包）
};

// 旧版资源清理结果
struct CleanupResult {
    QStringList removed;    // 已删除的条目
    QStringList failed;     // 删除失败的条目（含原因）
    qint64 freedBytes = 0;  // 释放的字节数
};

// 启动体检：配置里的 PaddleOCR / 识别模型路径若指向旧版本（或目标已不存在），
// 直接改回当前版本默认值。读不到根目录 PADDLEOCR 时不做任何改动。
QVector<PathFix> fixOcrPaths();

// 软件目录（根目录、tools、downloads 各一层）里是否存在不属于当前版本的
// PaddleOCR 或识别模型。只做名字比对，不统计体积，供启动时秒判是否需要弹窗。
bool hasObsoleteResources();

// 完整扫描一遍旧版资源，附带每项的占用空间，供后台清理任务使用。
QVector<ObsoleteResource> scanObsoleteResources();

// 后台清理任务：删除扫描到的旧版资源，过程中上报进度
class ResourceCleaner : public QObject {
    Q_OBJECT
public:
    explicit ResourceCleaner(QObject* parent = nullptr);
    const CleanupResult& result() const { return result_; }
public slots:
    void run();
signals:
    void itemStarted(const QString& name, int index, int total);
    void progressed(int percent);
    void itemFinished(const QString& path, bool ok, const QString& reason);
    void done();
private:
    CleanupResult result_;
};
}  // namespace fkw::ocr
