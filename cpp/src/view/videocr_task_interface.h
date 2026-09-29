#pragma once
#include <QRect>
#include <QVector>
#include "components/base_task_interface.h"
namespace fkw {
class OcrTaskInterface : public BaseTaskInterface {
    Q_OBJECT
public:
    explicit OcrTaskInterface(QWidget* parent = nullptr);
    // 视频预览框选的裁剪区域（视频像素坐标），在 addTask 前由主界面写入
    void setCropRects(const QVector<QRect>& rects);
protected:
    std::shared_ptr<TaskBase> createTask(const QString& input,
                                         const QString& output) override;
    TaskWorker* createWorker(const std::shared_ptr<TaskBase>& task) override;
    QString taskTypeText() const override;
    QString logName() const override;
    void emitLegacyFinished(bool success, TaskCard* card) override;
    QStringList taskGeneratedFiles(const std::shared_ptr<TaskBase>& task) const override;
private:
    QVector<QRect> cropRects_;
};
}
