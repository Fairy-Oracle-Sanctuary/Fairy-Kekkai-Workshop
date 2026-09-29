#pragma once
#include <memory>
#include "components/base_task_interface.h"
#include "service/ffmpeg_service.h"

namespace fkw {
class FFmpegTaskInterface : public BaseTaskInterface {
    Q_OBJECT
public:
    explicit FFmpegTaskInterface(QWidget* parent = nullptr);
protected:
    std::shared_ptr<TaskBase> createTask(const QString& input,
                                         const QString& output) override;
    TaskWorker* createWorker(const std::shared_ptr<TaskBase>& task) override;
    QString taskTypeText() const override;
    void emitLegacyFinished(bool success, TaskCard* card) override;
};
}
