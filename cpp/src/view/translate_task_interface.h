#pragma once
#include "components/base_task_interface.h"
namespace fkw {
class TranslateTaskInterface : public BaseTaskInterface {
    Q_OBJECT
public:
    explicit TranslateTaskInterface(QWidget* parent = nullptr);
protected:
    std::shared_ptr<TaskBase> createTask(const QString& input,
                                         const QString& output) override;
    TaskWorker* createWorker(const std::shared_ptr<TaskBase>& task) override;
    QString taskTypeText() const override;
    void emitLegacyFinished(bool success, TaskCard* card) override;
    QStringList taskGeneratedFiles(const std::shared_ptr<TaskBase>& task) const override;
};
}