#include "components/pager.h"
#include <QHBoxLayout>
#include <QIntValidator>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include "common/text.h"
#include "common/text_format.h"
#include <algorithm>

namespace fkw {
PageButton::PageButton(int page, bool selected, QWidget* parent)
    : QWidget(parent), page_(page), selected_(selected) {
    setFixedSize(35, 35);
    setCursor(Qt::PointingHandCursor);
}
void PageButton::setSelected(bool selected) {
    if (selected_ == selected) return;
    selected_ = selected;
    update();
}
void PageButton::enterEvent(enterEvent_QEnterEvent* event) {
    QWidget::enterEvent(event);
    hovered_ = true;
    update();
}
void PageButton::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    hovered_ = false;
    update();
}
void PageButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) emit clicked(page_);
    QWidget::mouseReleaseEvent(event);
}
void PageButton::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(Qt::NoPen);
    const QRect area = rect().adjusted(1, 1, -1, -1);
    const bool dark = qfw::isDarkTheme();
    if (selected_) {
        const QColor accent = qfw::QConfig::instance().themeColor();
        painter.setBrush(dark ? accent.lighter(140) : accent);
    }
    else if (hovered_) {
        const int pen = dark ? 255 : 0;
        const int fill = dark ? 46 : 224;
        painter.setPen(QColor(pen, pen, pen, 64));
        painter.setBrush(QColor(fill, fill, fill));
    }
    painter.drawRoundedRect(area, 6, 6);
    painter.setPen(selected_ ? (dark ? Qt::black : Qt::white)
                             : (dark ? Qt::white : Qt::black));
    painter.drawText(area, Qt::AlignCenter, QString::number(page_));
}

Pager::Pager(int pages, int maxVisible, QWidget* parent)
    : QWidget(parent), pages_(pages), maxVisible_(maxVisible),
      layout_(new QHBoxLayout(this)) {
    firstButton_ = new qfw::TransparentToolButton(this);
    lastButton_ = new qfw::TransparentToolButton(this);
    previousButton_ = new qfw::TransparentToolButton(
        qfw::FluentIcon(qfw::FluentIconEnum::CareLeftSolid).qicon(), this);
    nextButton_ = new qfw::TransparentToolButton(
        qfw::FluentIcon(qfw::FluentIconEnum::CareRightSolid).qicon(), this);
    firstEllipsis_ = new qfw::BodyLabel(QStringLiteral("..."), this);
    lastEllipsis_ = new qfw::BodyLabel(QStringLiteral("..."), this);
    firstEllipsis_->setFixedWidth(20);
    lastEllipsis_->setFixedWidth(20);
    firstEllipsis_->setAlignment(Qt::AlignCenter);
    lastEllipsis_->setAlignment(Qt::AlignCenter);
    firstEllipsis_->hide();
    lastEllipsis_->hide();
    const auto& t = Text::instance();
    jumpLabel_ = new qfw::BodyLabel(t.JumpTo, this);
    countLabel_ = new qfw::BodyLabel(
        formatText(t.PageOfPagesTotal, {QString::number(pages_)}), this);
    jumpEdit_ = new qfw::LineEdit(this);
    jumpEdit_->setValidator(new QIntValidator(jumpEdit_));
    jumpEdit_->setAlignment(Qt::AlignCenter);
    jumpEdit_->setFixedWidth(64);
    layout_->setSpacing(4);
    layout_->setContentsMargins(0, 0, 0, 0);
    firstButton_->setToolTip(t.GoToFirstPage);
    lastButton_->setToolTip(t.GoToLastPage);
    previousButton_->setToolTip(t.PreviousPage);
    nextButton_->setToolTip(t.NextPage);
    connect(firstButton_, &QPushButton::clicked, this, [this] {
        if (pages_ >= 1) onClicked(1);
    });
    connect(lastButton_, &QPushButton::clicked, this, [this] { onClicked(pages_); });
    connect(previousButton_, &QPushButton::clicked, this,
            [this] { onClicked(currentPage_ - 1); });
    connect(nextButton_, &QPushButton::clicked, this,
            [this] { onClicked(currentPage_ + 1); });
    connect(jumpEdit_, &QLineEdit::returnPressed, this, &Pager::jumpToPage);
    connect(this, &Pager::currentPageChanged, this,
            [this](int page) { jumpEdit_->setText(QString::number(page)); });
    connect(&qfw::QConfig::instance(), &qfw::QConfig::themeChanged,
            this, &Pager::updateThemeIcons);
    updateThemeIcons();
    updateButtons();
}
void Pager::updateThemeIcons() {
    const QString suffix = qfw::isDarkTheme() ? QStringLiteral("white")
                                               : QStringLiteral("black");
    firstButton_->setIcon(QIcon(QStringLiteral(":/app/images/controls/SkipStartFill_")
                                + suffix + QStringLiteral(".svg")));
    lastButton_->setIcon(QIcon(QStringLiteral(":/app/images/controls/SkipEndFill_")
                               + suffix + QStringLiteral(".svg")));
    firstButton_->setIconSize(QSize(20, 20));
    lastButton_->setIconSize(QSize(20, 20));
}
void Pager::addPage(int page, bool selected) {
    auto* button = new PageButton(page, selected, this);
    connect(button, &PageButton::clicked, this, &Pager::onClicked);
    layout_->addWidget(button);
}
void Pager::updateButtons() {
    firstEllipsis_->hide();
    lastEllipsis_->hide();
    while (auto* item = layout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            if (qobject_cast<PageButton*>(widget)) widget->deleteLater();
        }
        delete item;
    }
    layout_->addWidget(firstButton_);
    layout_->addWidget(previousButton_);
    const bool canGoBack = currentPage_ > 1;
    firstButton_->setEnabled(canGoBack);
    previousButton_->setEnabled(canGoBack);
    int start = std::max(1, currentPage_ - maxVisible_ / 2);
    int end = std::min(pages_, start + maxVisible_ - 1);
    if (end - start + 1 < maxVisible_)
        start = std::max(1, end - maxVisible_ + 1);
    if (start > 2) {
        addPage(1);
        layout_->addWidget(firstEllipsis_);
        firstEllipsis_->show();
    } else if (start == 2) addPage(1);
    for (int page = start; page <= end; ++page)
        addPage(page, page == currentPage_);
    if (end < pages_ - 1) {
        layout_->addWidget(lastEllipsis_);
        lastEllipsis_->show();
        addPage(pages_);
    } else if (end == pages_ - 1) addPage(pages_);
    layout_->addWidget(nextButton_);
    layout_->addWidget(lastButton_);
    const bool canGoNext = currentPage_ < pages_;
    nextButton_->setEnabled(canGoNext);
    lastButton_->setEnabled(canGoNext);
    layout_->addWidget(jumpLabel_);
    layout_->addWidget(jumpEdit_);
    layout_->addWidget(countLabel_);
    for (auto* widget : {firstButton_, previousButton_, nextButton_, lastButton_})
        widget->show();
    jumpLabel_->show();
    jumpEdit_->show();
    countLabel_->show();
    emit currentPageChanged(currentPage_);
}
void Pager::onClicked(int page) {
    currentPage_ = page;
    updateButtons();
}
void Pager::setPages(int number) {
    if (pages_ == number) return;
    pages_ = number;
    countLabel_->setText(formatText(Text::instance().PageOfPagesTotal,
                                    {QString::number(pages_)}));
    updateButtons();
}
void Pager::setCurrentPage(int page) {
    if (page > pages_) return;
    onClicked(page);
}
void Pager::setMaxVisible(int number) {
    if (maxVisible_ == number || maxVisible_ > pages_) return;
    maxVisible_ = number;
    updateButtons();
}
void Pager::jumpToPage() {
    bool valid = false;
    const int page = jumpEdit_->text().toInt(&valid);
    if (valid && page >= 1 && page <= pages_) onClicked(page);
}
}  // namespace fkw
