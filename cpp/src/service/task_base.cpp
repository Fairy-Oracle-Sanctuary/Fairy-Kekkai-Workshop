#include "service/task_base.h"

#include "common/utils.h"

namespace fkw {

std::atomic<int> TaskBase::idCounter{0};

TaskBase::TaskBase(QString input, QString output)
    : taskId(++idCounter),
      inputPath(std::move(input)),
      outputPath(std::move(output)),
      fileName(pathBasename(inputPath)),
      outputName(pathBasename(outputPath)),
      createTime(QDateTime::currentDateTime()) {}

TaskWorker::TaskWorker(QObject* parent) : QObject(parent) {
    // 关闭线程池自动删除，改由 run() 末尾 deleteLater() 投递到主线程回收
    setAutoDelete(false);
}

TaskWorker::~TaskWorker() = default;

}  // namespace fkw