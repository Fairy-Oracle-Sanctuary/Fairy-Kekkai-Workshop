#pragma once

#include <QStackedWidget>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

namespace fkw
{
    class BaseStackedInterfaces : public QWidget
    {
        Q_OBJECT
    public:
        explicit BaseStackedInterfaces(QWidget *parent = nullptr);
        void addSubInterface(QWidget *widget, const QString &objectName, const QString &text);
        void showPage(int index);

    protected:
        qfw::Pivot *pivot_;
        QStackedWidget *stackedWidget_;
    };
}
