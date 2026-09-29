#pragma once

#include <QResizeEvent>
#include <qtfluentwidgets.h>

namespace fkw {
class TopButtonCard : public qfw::SimpleCardWidget {
    Q_OBJECT
public:
    explicit TopButtonCard(QWidget* parent = nullptr);
    qfw::PushButton* newProjectButton;
    qfw::PushButton* importProjectButton;
    qfw::PushButton* newFromPlaylistButton;
    qfw::PrimaryPushButton* refreshButton;
};

class ProjectCard : public qfw::CardWidget {
    Q_OBJECT
public:
    ProjectCard(const QString& title, const QString& content, const QString& path,
                const QIcon& icon, bool linked = false, QWidget* parent = nullptr);
    QString path() const { return path_; }
    // 按项目体检结果刷新徽标、修复按钮与「打开项目」的可用状态。
    // level：0 正常，1 待修复，2 失联（项目目录已不存在）
    // blocked：存在结构性损坏，修复前不允许打开
    void setHealth(int level, int issueCount, bool blocked);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
signals:
    void openProject(const QString& path);
    void editProject(const QString& path);
    void moveToTop(const QString& path);
    void removeProject(const QString& path, bool unlink);
    void repairProject(const QString& path);
private:
    void updateCardHeight();
    QString path_;
    QPoint dragStart_;
    qfw::BodyLabel* titleLabel_ = nullptr;
    qfw::CaptionLabel* contentLabel_ = nullptr;
    QVBoxLayout* labelsLayout_ = nullptr;
    qfw::InfoBadge* healthBadge_ = nullptr;
    qfw::PushButton* repairButton_ = nullptr;
    qfw::PrimaryPushButton* openButton_ = nullptr;
    bool heightUpdateScheduled_ = false;
};
}
