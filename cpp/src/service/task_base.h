#pragma once

#include <QDateTime>
#include <QObject>
#include <QRunnable>
#include <QString>
#include <atomic>

namespace fkw {

// 任务纯数据基类：TaskCard / BaseTaskInterface 只依赖这里的公共字段，
// 各 service 的具体任务（FFmpegTask / TranslateTask / ...）继承后扩展自己的参数。
// 对齐 Python 各 service 任务对象的公共部分（fileName/outputName/logPath/createTime）。
//
// 任务 id 由基类统一分配。全局事件总线按 taskId 分发状态，各任务界面共享同一套
// id 才能避免不同类型的任务撞号（Python 里每个任务类各有一个 _id_counter，
// 都从 0 起算，跨类型会串台）。
struct TaskBase {
    TaskBase(QString input, QString output);
    virtual ~TaskBase() = default;

    int taskId = 0;
    QString inputPath;
    QString outputPath;
    QString fileName;
    QString outputName;
    QString logPath;
    QDateTime createTime;
    // 任务图标名，对应 :/app/images/icons/<iconName>.svg；为空则退回输入文件图标
    QString iconName;

    static std::atomic<int> idCounter;
};

// 任务执行引擎基类：由 QThreadPool 调度，run() 内通过 event_bus 上报进度与完成。
// 子类需 setAutoDelete(false)，并在 run() 末尾 deleteLater() 回收 —— QObject 归属
// 主线程，不能在线程池工作线程内析构。
class TaskWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit TaskWorker(QObject* parent = nullptr);
    ~TaskWorker() override;

    void run() override = 0;
    // 取消任务：标记取消并中断当前 I/O。取消的任务不 emit finishTaskSig
    // （状态已由任务界面设为 Cancelled）。
    virtual void cancel() = 0;

protected:
    std::atomic<bool> cancelled_{false};
};

}  // namespace fkw