#pragma once
#include <QString>
#include "common/app_data.h"

namespace fkw {
// Values match the Python TaskStatus and Easy-FFmpeg protocol.
enum class TaskStatus {
    Waiting = 0, Pending = 1, Processing = 2, Failed = 3,
    Succeeded = 4, Cancelling = 5, Cancelled = 6
};
inline QString statusText(TaskStatus status, const QString& processingText = {}) {
    switch (status) {
    case TaskStatus::Waiting:
    case TaskStatus::Pending: return trText("等待中");
    case TaskStatus::Processing: return processingText.isEmpty()
        ? trText("处理中") : processingText;
    case TaskStatus::Failed: return trText("失败");
    case TaskStatus::Succeeded: return trText("已完成");
    case TaskStatus::Cancelling: return trText("正在取消...");
    case TaskStatus::Cancelled: return trText("已取消");
    }
    return QString::number(static_cast<int>(status));
}
}  // namespace fkw
