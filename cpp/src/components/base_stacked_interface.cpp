#include "components/base_stacked_interface.h"

namespace fkw
{
    BaseStackedInterfaces::BaseStackedInterfaces(QWidget *parent) : QWidget(parent)
    {
        pivot_ = new qfw::Pivot(this);
        pivot_->setObjectName(QStringLiteral("tutorial-page-tabs"));
        stackedWidget_ = new QStackedWidget(this);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(30, 0, 30, 30);
        layout->addWidget(pivot_, 0, Qt::AlignHCenter);
        layout->addWidget(stackedWidget_);
        resize(780, 800);
        connect(stackedWidget_, &QStackedWidget::currentChanged, this, [this](int index)
                {
        if (auto* page = stackedWidget_->widget(index)) pivot_->setCurrentItem(page->objectName()); });
    }

    void BaseStackedInterfaces::addSubInterface(QWidget *widget, const QString &objectName,
                                                const QString &text)
    {
        widget->setObjectName(objectName);
        stackedWidget_->addWidget(widget);
        pivot_->addItem(objectName, text, [this, widget](bool)
                        { stackedWidget_->setCurrentWidget(widget); });
        if (stackedWidget_->count() == 1)
            pivot_->setCurrentItem(objectName);
    }

    void BaseStackedInterfaces::showPage(int index)
    {
        if (index >= 0 && index < stackedWidget_->count())
            stackedWidget_->setCurrentIndex(index);
    }
}
