#include "service/video_preview.h"

#include <algorithm>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QStringList>
#include <QWidget>
#include <QVBoxLayout>

#include "common/app_data.h"
#include "common/text.h"
#include "common/text_format.h"

namespace fkw
{
    PreviewCanvas::PreviewCanvas(QWidget *parent) : qfw::BodyLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setMinimumHeight(240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setText(Text::instance().VPACBBTSVF);
        setMouseTracking(true);
    }

    QRect PreviewCanvas::displayRect() const
    {
        if (frame_.isNull())
            return {};
        const QSize scaled = frame_.size().scaled(size(), Qt::KeepAspectRatio);
        return QRect((width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
                     scaled.width(), scaled.height());
    }
    QPoint PreviewCanvas::framePosition(const QPoint &widgetPosition) const
    {
        const QRect area = displayRect();
        if (area.isEmpty())
            return {};
        const int x = (widgetPosition.x() - area.x()) * frame_.width() / area.width();
        const int y = (widgetPosition.y() - area.y()) * frame_.height() / area.height();
        return QPoint(std::clamp(x, 0, frame_.width() - 1),
                      std::clamp(y, 0, frame_.height() - 1));
    }

    void PreviewCanvas::setFrame(const QImage &image)
    {
        if (image.size() != frame_.size())
            clearSelection();
        frame_ = image;
        if (image.isNull())
            setSelectionEnabled(false);
        setText(image.isNull() ? Text::instance().VPACBBTSVF : QString());
        update();
    }
    void PreviewCanvas::clearSelection()
    {
        selection_ = {};
        previewRect_ = {};
        previewIndex_ = -1;
        boxes_.clear();
        selecting_ = false;
        hasDragStart_ = false;
        setSelectionEnabled(false);
        emit selectionChanged(boxes_);
        update();
    }
    void PreviewCanvas::setSelectionEnabled(bool enabled)
    {
        enabled_ = enabled && !frame_.isNull() && boxes_.size() < maxSelections_;
        setCursor(enabled_ ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    void PreviewCanvas::setMaxSelections(int count)
    {
        maxSelections_ = std::clamp(count, 1, 2);
        while (boxes_.size() > maxSelections_)
            boxes_.removeLast();
        previewIndex_ = -1;
        previewRect_ = {};
        emit selectionChanged(boxes_);
        update();
    }
    void PreviewCanvas::setPreviewRect(int index, const QRect& rect)
    {
        previewIndex_ = index;
        previewRect_ = rect;
        update();
    }
    void PreviewCanvas::setSelectionRect(int index, const QRect& rect)
    {
        if (index < 0 || index >= boxes_.size()) return;
        boxes_[index] = rect;
        previewIndex_ = -1;
        previewRect_ = {};
        emit selectionChanged(boxes_);
        update();
    }
    void PreviewCanvas::paintEvent(QPaintEvent *event)
    {
        qfw::BodyLabel::paintEvent(event);
        if (frame_.isNull())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRect area = displayRect();
        painter.drawImage(area, frame_);
        const qreal sx = qreal(area.width()) / frame_.width();
        const qreal sy = qreal(area.height()) / frame_.height();
        auto drawBox = [&](const QRect &box, const QColor& color, bool dashed, int number)
        {
            if (box.isEmpty()) return;
            painter.setPen(QPen(color, 2, dashed ? Qt::DashLine : Qt::SolidLine));
            painter.setBrush(Qt::NoBrush);
            const QRectF scaled(area.x() + box.x() * sx, area.y() + box.y() * sy,
                                box.width() * sx, box.height() * sy);
            painter.drawRect(scaled);
            if (number > 0)
                painter.drawText(scaled.topLeft() + QPointF(5, 18), QString::number(number));
        };
        const QColor colors[] = {Qt::red, Qt::green, Qt::blue};
        for (int i = 0; i < boxes_.size(); ++i)
            drawBox(i == previewIndex_ ? previewRect_ : boxes_[i],
                    colors[i % 3], i == previewIndex_, i + 1);
        drawBox(selection_, Qt::yellow, true, 0);
    }
    void PreviewCanvas::mousePressEvent(QMouseEvent *event)
    {
        if (!enabled_ || boxes_.size() >= maxSelections_ || event->button() != Qt::LeftButton)
        {
            qfw::BodyLabel::mousePressEvent(event);
            return;
        }
        selecting_ = true;
        hasDragStart_ = displayRect().contains(event->pos());
        if (hasDragStart_) start_ = framePosition(event->pos());
        selection_ = {};
        event->accept();
    }
    void PreviewCanvas::mouseMoveEvent(QMouseEvent *event)
    {
        if (!selecting_)
        {
            qfw::BodyLabel::mouseMoveEvent(event);
            return;
        }
        if (!hasDragStart_) {
            if (event->buttons().testFlag(Qt::LeftButton)
                && displayRect().contains(event->pos())) {
                start_ = framePosition(event->pos());
                hasDragStart_ = true;
            }
            event->accept();
            return;
        }
        selection_ = QRect(start_, framePosition(event->pos())).normalized();
        update();
        event->accept();
    }
    void PreviewCanvas::mouseReleaseEvent(QMouseEvent *event)
    {
        if (!selecting_ || event->button() != Qt::LeftButton)
        {
            qfw::BodyLabel::mouseReleaseEvent(event);
            return;
        }
        selecting_ = false;
        if (!hasDragStart_) {
            event->accept();
            return;
        }
        hasDragStart_ = false;
        selection_ = QRect(start_, framePosition(event->pos())).normalized();
        if (selection_.width() >= 7 && selection_.height() >= 7) {
            boxes_.append(selection_);
            setSelectionEnabled(false);
            emit selectionChanged(boxes_);
        }
        selection_ = {};
        update();
        event->accept();
    }
    VideoPreview::VideoPreview(QWidget *parent) : qfw::SimpleCardWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        canvas_ = new PreviewCanvas(this);
        layout->addWidget(canvas_);
        auto *controls = new QHBoxLayout();
        selectButton_ = new qfw::PushButton(
            qfw::FluentIcon(qfw::FluentIconEnum::Move).qicon(),
            Text::instance().SelectArea, this);
        clearButton_ = new qfw::PushButton(
            qfw::FluentIcon(qfw::FluentIconEnum::Cancel).qicon(),
            Text::instance().ClearSelection, this);
        selectButton_->setEnabled(false);
        clearButton_->setEnabled(false);
        controls->addWidget(selectButton_);
        controls->addWidget(clearButton_);
        controls->addStretch();
        layout->addLayout(controls);
        coordinates_ = new qfw::CaptionLabel(Text::instance().SCNS, this);
        coordinates_->setWordWrap(true);
        layout->addWidget(coordinates_);
        editorLayout_ = new QVBoxLayout();
        layout->addLayout(editorLayout_);
        connect(selectButton_, &QPushButton::clicked, this,
                [this]()
                {
                    canvas_->setSelectionEnabled(true);
                    coordinates_->setText(Text::instance().SCS);
                });
        connect(clearButton_, &QPushButton::clicked, canvas_, &PreviewCanvas::clearSelection);
        connect(canvas_, &PreviewCanvas::selectionChanged, this,
                [this](const QVector<QRect> &boxes)
                {
                    clearButton_->setEnabled(!boxes.isEmpty());
                    selectButton_->setEnabled(canvas_->hasFrame() && boxes.size() < maxCropBoxes_);
                    updateCoordinates(boxes);
                    rebuildEditors(boxes);
                    if (boxes.size() >= maxCropBoxes_)
                        canvas_->setSelectionEnabled(false);
                    emit cropSelected(boxes.size() >= maxCropBoxes_);
                });
    }
    void VideoPreview::setFrame(const QImage &image)
    {
        canvas_->setFrame(image);
        selectButton_->setEnabled(!image.isNull() && canvas_->selectionRects().size() < maxCropBoxes_);
    }
    void VideoPreview::setMaxCropBoxes(int count)
    {
        maxCropBoxes_ = std::clamp(count, 1, 2);
        canvas_->setMaxSelections(maxCropBoxes_);
    }
    void VideoPreview::reset()
    {
        canvas_->clearSelection();
        canvas_->setFrame({});
        selectButton_->setEnabled(false);
    }
    void VideoPreview::setError(const QString &message)
    {
        reset();
        canvas_->setText(message);
    }
    void VideoPreview::updateCoordinates(const QVector<QRect>& boxes)
    {
        if (boxes.isEmpty()) {
            coordinates_->setText(Text::instance().SCNS);
            return;
        }
        QStringList parts;
        for (int i = 0; i < boxes.size(); ++i) {
            const auto& rect = boxes[i];
            parts << formatText(Text::instance().AreaXYWH,
                {QString::number(i + 1), QString::number(rect.x()),
                 QString::number(rect.y()), QString::number(rect.width()),
                 QString::number(rect.height())});
        }
        coordinates_->setText(parts.join(QStringLiteral(" | ")));
    }
    QRect VideoPreview::editedRect(int index) const
    {
        if (index < 0 || index >= editors_.size()) return {};
        const auto& editor = editors_[index];
        const QSize size = canvas_->frameSize();
        const int x = std::clamp(editor.x->value(), 0, std::max(0, size.width() - 1));
        const int y = std::clamp(editor.y->value(), 0, std::max(0, size.height() - 1));
        const int w = std::clamp(editor.width->value(), 1, std::max(1, size.width() - x));
        const int h = std::clamp(editor.height->value(), 1, std::max(1, size.height() - y));
        return QRect(x, y, w, h);
    }
    void VideoPreview::rebuildEditors(const QVector<QRect>& boxes)
    {
        editors_.clear();
        while (auto* item = editorLayout_->takeAt(0)) {
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        for (int i = 0; i < boxes.size(); ++i) {
            auto* card = new qfw::SimpleCardWidget(this);
            auto* layout = new QVBoxLayout(card);
            layout->addWidget(new qfw::CaptionLabel(
                formatText(Text::instance().AreaCoordinates, {QString::number(i + 1)}), card));
            auto* flow = new qfw::FlowLayout();
            layout->addLayout(flow);
            const QRect rect = boxes[i];
            const QSize frame = canvas_->frameSize();
            auto makeField = [card, flow](const QString& label, int value, int maximum, int minimum) {
                auto* group = new QWidget(card);
                auto* column = new QVBoxLayout(group);
                column->setContentsMargins(0, 0, 0, 0);
                column->setSpacing(2);
                column->addWidget(new qfw::CaptionLabel(label, group));
                auto* spin = new qfw::SpinBox(group);
                spin->setRange(minimum, std::max(minimum, maximum));
                spin->setMinimumWidth(250);
                spin->setValue(value);
                column->addWidget(spin);
                flow->addWidget(group);
                return spin;
            };
            CoordEditor editor{
                makeField(QStringLiteral("X:"), rect.x(), frame.width() - 1, 0),
                makeField(QStringLiteral("Y:"), rect.y(), frame.height() - 1, 0),
                makeField(Text::instance().Width, rect.width(), frame.width(), 1),
                makeField(Text::instance().Height, rect.height(), frame.height(), 1)
            };
            editors_.append(editor);
            auto* apply = new qfw::PushButton(
                qfw::FluentIcon(qfw::FluentIconEnum::Accept).qicon(),
                Text::instance().Apply, card);
            flow->addWidget(apply);
            for (auto* spin : {editor.x, editor.y, editor.width, editor.height}) {
                connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                        [this, i](int) { canvas_->setPreviewRect(i, editedRect(i)); });
            }
            connect(apply, &QPushButton::clicked, this,
                    [this, i]() { canvas_->setSelectionRect(i, editedRect(i)); });
            editorLayout_->addWidget(card);
        }
    }
} // namespace fkw
