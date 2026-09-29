#pragma once
#include <QRect>
#include <QVector>
#include "components/base_function_interface.h"
#include "components/base_stacked_interface.h"
namespace fkw {
class VideoPreview;
class VideocrInterface : public BaseFunctionInterface {
    Q_OBJECT
public:
    explicit VideocrInterface(QWidget* parent = nullptr);
    void setInputPath(const QString& path) override;
    // 视频预览框选的裁剪区域（视频像素坐标），供任务界面构建 OCR 命令
    QVector<QRect> cropRects() const;
public slots:
    // 提取页面日志框（对齐 Python VideocrInterface._log_message）：
    // 前缀 [hh:mm:ss]，isError 时红色，isFlush 时覆盖上一行并滚动到底部
    void logMessage(const QString& message, bool isError = false,
                    bool isFlush = false);
    void clearLog();
protected:
    bool validateBeforeStart(QString* errorMessage) override;
    void showEvent(QShowEvent* event) override;
private:
    VideoPreview* preview_ = nullptr;
    qfw::TextBrowser* logText_ = nullptr;
};
class VideocrStackedInterfaces : public BaseStackedInterfaces {
    Q_OBJECT
public:
    explicit VideocrStackedInterfaces(QWidget* parent = nullptr);
};
}
