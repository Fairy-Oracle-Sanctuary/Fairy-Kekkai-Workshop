#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace fkw::legacy {
// 一处需要清理的上一代 Python / Whisper 残留
struct ResidueItem {
    QString path;      // 绝对路径
    QString name;      // 条目名
    QString reason;    // 判定为残留的原因，用于日志与界面提示
    qint64 bytes = 0;  // 占用空间，用于展示释放量
    bool directory = true;  // true：目录；false：文件
};

// 残留清理结果
struct ResidueResult {
    QStringList removed;    // 已删除的条目
    QStringList failed;     // 删除失败的条目（含原因）
    qint64 freedBytes = 0;  // 释放的字节数
};

// 是否有顶层 Python 依赖或 tools/whisper 内已被完整新版替代的旧文件。
// 只做名字比对，不统计体积，供启动时秒判是否需要弹窗。
bool hasLegacyResidue();

// 完整扫描一遍旧版残留，附带每项占用空间，供后台清理任务使用。
QVector<ResidueItem> scanLegacyResidue();

// 后台清理任务：删除扫描到的旧版残留，过程中上报进度
class ResidueCleaner : public QObject {
    Q_OBJECT
public:
    explicit ResidueCleaner(QObject* parent = nullptr);
    const ResidueResult& result() const { return result_; }
public slots:
    void run();
signals:
    void itemStarted(const QString& name, int index, int total);
    void progressed(int percent);
    void itemFinished(const QString& path, bool ok, const QString& reason);
    void done();
private:
    ResidueResult result_;
};
}  // namespace fkw::legacy
