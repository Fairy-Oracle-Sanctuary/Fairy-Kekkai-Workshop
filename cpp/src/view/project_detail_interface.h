#pragma once

#include <QStringList>
#include <QSet>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

namespace fkw {
class ProjectDetailInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit ProjectDetailInterface(QWidget* parent = nullptr);
    void loadProject(const QString& path);
    QString projectPath() const { return path_; }
    void reloadCurrentProject(bool notify = false);
    void showPage(int page);
signals:
    void backToProjectList();
    void projectLoaded(const QString& path);
private:
    void beginLoad(int page);
    void showLoadingView();
    // 项目损坏时的修复入口视图：列出异常并提供「一键修复」，修复前不进入正常页面
    void showRepairView(const QStringList& issues);
    void addEpisode(int number);
    void removeEpisode(int number);
    void editEpisode(int number);
    void dispatchTask(int taskType, const QString& folderPath, const QString& videoUrl);
    QVBoxLayout* layout_;
    QWidget* content_ = nullptr;
    QString path_;
    int currentPage_ = 1;
    unsigned loadGeneration_ = 0;
    bool notifyOnLoad_ = false;
    bool notifyOnRefresh_ = false;
    QSet<int> expandedEpisodes_;
    QSet<int> selectedEpisodes_;
    bool infoExpanded_ = false;
    int pendingScrollPosition_ = -1;
};
}
