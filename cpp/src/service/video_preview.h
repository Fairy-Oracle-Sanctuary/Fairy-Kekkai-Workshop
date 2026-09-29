#pragma once

#include <QImage>
#include <QRect>
#include <QVector>
#include <qtfluentwidgets.h>

class QVBoxLayout;

namespace fkw {
class PreviewCanvas : public qfw::BodyLabel {
    Q_OBJECT
public:
    explicit PreviewCanvas(QWidget* parent = nullptr);
    void setFrame(const QImage& image);
    void clearSelection();
    void setSelectionEnabled(bool enabled);
    void setMaxSelections(int count);
    void setPreviewRect(int index, const QRect& rect);
    void setSelectionRect(int index, const QRect& rect);
    QVector<QRect> selectionRects() const { return boxes_; }
    QSize frameSize() const { return frame_.size(); }
    bool hasFrame() const { return !frame_.isNull(); }
signals:
    void selectionChanged(const QVector<QRect>& boxes);
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    QRect displayRect() const;
    QPoint framePosition(const QPoint& widgetPosition) const;
    QImage frame_;
    QRect selection_;
    QVector<QRect> boxes_;
    QRect previewRect_;
    int previewIndex_ = -1;
    QPoint start_;
    int maxSelections_ = 1;
    bool selecting_ = false;
    bool hasDragStart_ = false;
    bool enabled_ = false;
};

class VideoPreview : public qfw::SimpleCardWidget {
    Q_OBJECT
public:
    explicit VideoPreview(QWidget* parent = nullptr);
    void setFrame(const QImage& image);
    void reset();
    void setError(const QString& message);
    void setMaxCropBoxes(int count);
    QVector<QRect> selectionRects() const { return canvas_->selectionRects(); }
    QRect selectionRect() const {
        const auto boxes = selectionRects();
        return boxes.isEmpty() ? QRect() : boxes.first();
    }
    bool hasFrame() const { return canvas_->hasFrame(); }
signals:
    void cropSelected(bool selected);
private:
    struct CoordEditor {
        qfw::SpinBox* x;
        qfw::SpinBox* y;
        qfw::SpinBox* width;
        qfw::SpinBox* height;
    };
    void updateCoordinates(const QVector<QRect>& boxes);
    void rebuildEditors(const QVector<QRect>& boxes);
    QRect editedRect(int index) const;
    PreviewCanvas* canvas_;
    qfw::PushButton* selectButton_;
    qfw::PushButton* clearButton_;
    qfw::CaptionLabel* coordinates_;
    QVBoxLayout* editorLayout_;
    QVector<CoordEditor> editors_;
    int maxCropBoxes_ = 1;
};
} // namespace fkw
