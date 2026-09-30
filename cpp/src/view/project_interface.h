#pragma once

#include <QVBoxLayout>
#include <qtfluentwidgets.h>

namespace fkw {
class ProjectInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    explicit ProjectInterface(QWidget* parent = nullptr);
    void refreshProjectList();
signals:
    void openProjectDetail(const QString& path);
    void projectChanged(const QString& oldPath, const QString& newPath);
    void projectRemoved(const QString& path);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    // 项目存在异常时拦截打开，提示用户先执行一键修复
    void tryOpenProject(const QString& path);
    // 一键智能修复：确认后自动补齐分集目录与 标题.txt 记录，失败自动回滚
    void repairProject(const QString& path);
    QWidget* cardsContainer_ = nullptr;
    QVBoxLayout* cardsLayout_;
};
}
