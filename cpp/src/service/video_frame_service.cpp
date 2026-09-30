#include "service/video_frame_service.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QFileInfo>
#include <QMetaObject>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "common/app_data.h"

namespace fkw {
class FrameWorker : public QObject {
public:
    explicit FrameWorker(VideoFrameService* owner) : owner_(owner) {}
    void open(const QString& path, int generation);
    void readFrame(int frame, int generation, int request);
private:
    VideoFrameService* owner_;
    cv::VideoCapture capture_;
    int nextFrame_ = -1;
    void publishFrame(int frame, int generation, int request);
};
void FrameWorker::open(const QString& path, int generation) {
    capture_.release();
    nextFrame_ = -1;
    if (path.isEmpty() || generation != owner_->generation_.load()) return;
    const QByteArray bytes = path.toUtf8();
    if (!capture_.open(bytes.constData()) || !capture_.isOpened()) {
        QMetaObject::invokeMethod(owner_, [owner = owner_, generation] {
            if (generation == owner->generation_.load())
                emit owner->failed(trText("无法读取视频流"));
        }, Qt::QueuedConnection);
        return;
    }
    const double frameCount = capture_.get(cv::CAP_PROP_FRAME_COUNT);
    const double rate = capture_.get(cv::CAP_PROP_FPS);
    const int count = std::isfinite(frameCount) && frameCount > 0
        ? static_cast<int>(std::clamp(frameCount, 1.0,
              static_cast<double>(std::numeric_limits<int>::max()))) : 1;
    const double fps = std::isfinite(rate) && rate > 0 ? rate : 0.0;
    const double duration = fps > 0 ? count / fps : 0.0;
    QMetaObject::invokeMethod(owner_, [owner = owner_, generation, count, fps, duration] {
        if (generation != owner->generation_.load()) return;
        owner->totalFrames_ = count;
        owner->fps_ = fps;
        owner->duration_ = duration;
        emit owner->videoOpened(count, fps, duration);
    }, Qt::QueuedConnection);
    nextFrame_ = 0;
    publishFrame(0, generation, owner_->requestGeneration_.load());
}
void FrameWorker::readFrame(int frame, int generation, int request) {
    if (generation != owner_->generation_.load()
        || request != owner_->requestGeneration_.load()
        || !capture_.isOpened()) return;
    publishFrame(frame, generation, request);
}

void FrameWorker::publishFrame(int frame, int generation, int request) {
    if (generation != owner_->generation_.load()
        || request != owner_->requestGeneration_.load()) return;
    if (nextFrame_ != frame) {
        if (!capture_.set(cv::CAP_PROP_POS_FRAMES, frame)) {
            QMetaObject::invokeMethod(owner_, [owner = owner_, generation, request] {
                if (generation == owner->generation_.load()
                    && request == owner->requestGeneration_.load())
                    emit owner->failed(trText("无法定位视频帧"));
            }, Qt::QueuedConnection);
            return;
        }
    }
    cv::Mat pixels;
    if (!capture_.read(pixels) || pixels.empty()) {
        QMetaObject::invokeMethod(owner_, [owner = owner_, generation, request] {
            if (generation == owner->generation_.load()
                && request == owner->requestGeneration_.load())
                emit owner->failed(trText("无法加载视频帧"));
        }, Qt::QueuedConnection);
        return;
    }
    nextFrame_ = frame + 1;
    QImage image;
    if (pixels.type() == CV_8UC3) {
        image = QImage(pixels.data, pixels.cols, pixels.rows,
                       static_cast<qsizetype>(pixels.step), QImage::Format_BGR888).copy();
    } else if (pixels.type() == CV_8UC1) {
        image = QImage(pixels.data, pixels.cols, pixels.rows,
                       static_cast<qsizetype>(pixels.step), QImage::Format_Grayscale8).copy();
    } else if (pixels.type() == CV_8UC4) {
        image = QImage(pixels.data, pixels.cols, pixels.rows,
                       static_cast<qsizetype>(pixels.step), QImage::Format_RGBA8888).rgbSwapped();
    }
    QMetaObject::invokeMethod(owner_, [owner = owner_, generation, request, frame, image] {
        if (generation != owner->generation_.load()
            || request != owner->requestGeneration_.load()) return;
        if (image.isNull()) emit owner->failed(trText("无法加载视频帧"));
        else emit owner->frameReady(frame, image);
    }, Qt::QueuedConnection);
}

VideoFrameService::VideoFrameService(QObject* parent) : QObject(parent) {
    worker_ = new FrameWorker(this);
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
VideoFrameService::~VideoFrameService() {
    thread_.quit();
    thread_.wait();
}
void VideoFrameService::open(const QString& path) {
    const int generation = ++generation_;
    ++requestGeneration_;
    frameRequestActive_ = false;
    pendingFrame_ = -1;
    totalFrames_ = 0;
    fps_ = 0.0;
    duration_ = 0.0;
    const QString videoPath = path.trimmed();
    const bool exists = videoPath.isEmpty() || QFileInfo(videoPath).isFile();
    QMetaObject::invokeMethod(worker_, [worker = worker_, videoPath, generation, exists] {
        worker->open(exists ? videoPath : QString(), generation);
    }, Qt::QueuedConnection);
    if (!exists) emit failed(trText("视频文件不存在"));
}
void VideoFrameService::requestFrame(int frameNumber) {
    if (totalFrames_ < 1) return;
    const int frame = std::clamp(frameNumber, 0, totalFrames_ - 1);
    if (frameRequestActive_) {
        // Keep one latest target, rather than queuing every slider movement.
        pendingFrame_ = frame;
        return;
    }
    dispatchFrame(frame);
}
void VideoFrameService::dispatchFrame(int frame) {
    frameRequestActive_ = true;
    const int generation = generation_.load();
    const int request = ++requestGeneration_;
    QMetaObject::invokeMethod(worker_, [this, worker = worker_, frame, generation, request] {
        worker->readFrame(frame, generation, request);
        // publishFrame queues its result first, so it is displayed before dispatching
        // another request. Slow decoding cannot starve all intermediate previews.
        QMetaObject::invokeMethod(this, [this, frame, generation, request] {
            if (generation != generation_.load()
                || request != requestGeneration_.load()) return;
            frameRequestActive_ = false;
            const int pending = pendingFrame_;
            pendingFrame_ = -1;
            if (pending >= 0 && pending != frame) dispatchFrame(pending);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
} // namespace fkw
