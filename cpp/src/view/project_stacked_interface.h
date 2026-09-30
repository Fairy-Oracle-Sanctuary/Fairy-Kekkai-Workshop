#pragma once

#include <QMap>
#include <QStackedWidget>
#include <QWidget>
#include <qtfluentwidgets.h>

class QShowEvent;
namespace fkw {
class ProjectInterface;
class ProjectStackedInterface : public QWidget {
    Q_OBJECT
public:
    explicit ProjectStackedInterface(QWidget* parent = nullptr);
    void openProject(const QString& path);
    void showProjectList() { selectList(); }
protected:
    void showEvent(QShowEvent* event) override;
private:
    void selectTab(const QString& key);
    void selectList();
    void closeTab(const QString& key);
    void updateProjectTab(const QString& oldPath, const QString& newPath);
    void removeProjectTab(const QString& path);
    QString keyForPath(const QString& path) const;
    qfw::TabBar* tabBar_ = nullptr;
    QStackedWidget* stacked_ = nullptr;
    ProjectInterface* projects_ = nullptr;
    QMap<QString, QWidget*> pages_;
};
}
