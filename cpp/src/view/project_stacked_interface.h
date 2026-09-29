#pragma once

#include <QStackedWidget>
#include <QWidget>

class QShowEvent;
namespace fkw {
class ProjectInterface;
class ProjectDetailInterface;
class ProjectStackedInterface : public QWidget {
    Q_OBJECT
public:
    explicit ProjectStackedInterface(QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* event) override;
private:
    QStackedWidget* stacked_;
    ProjectInterface* projects_;
    ProjectDetailInterface* detail_;
};
}
