#pragma once

#include <atomic>
#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>

namespace fkw {
class FrameWorker;
class VideoFrameService : public QObject {
    Q_OBJECT
public:
    explicit VideoFrameService(QObject* parent = nullptr);
    ~VideoFrameService() override;
    void open(const QString& path);
    void requestFrame(int frameNumber);
    int totalFrames() const { return totalFrames_; }
    double fps() const { return fps_; }
    double duration() const { return duration_; }
signals:
    void videoOpened(int totalFrames, double fps, double duration);
    void frameReady(int frameNumber, const QImage& frame);
    void failed(const QString& message);
private:
    friend class FrameWorker;
    QThread thread_;
    FrameWorker* worker_ = nullptr;
    std::atomic<int> generation_{0};
    std::atomic<int> requestGeneration_{0};
    int totalFrames_ = 0;
    double fps_ = 0.0;
    double duration_ = 0.0;
};
} // namespace fkw
