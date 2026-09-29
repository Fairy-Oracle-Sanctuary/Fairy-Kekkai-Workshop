#pragma once
#include <QWidget>
#include <qtfluentwidgets.h>

class QHBoxLayout;
class QLabel;
namespace fkw {

class PageButton : public QWidget {
    Q_OBJECT
public:
    explicit PageButton(int page, bool selected = false, QWidget* parent = nullptr);
    void setSelected(bool selected);
    bool isSelected() const { return selected_; }
signals:
    void clicked(int page);
protected:
    void enterEvent(enterEvent_QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    int page_;
    bool selected_;
    bool hovered_ = false;
};

class Pager : public QWidget {
    Q_OBJECT
public:
    explicit Pager(int pages, int maxVisible, QWidget* parent = nullptr);
    int currentPage() const { return currentPage_; }
    int pages() const { return pages_; }
    int maxVisible() const { return maxVisible_; }
    void setPages(int number);
    void setCurrentPage(int page);
    void setMaxVisible(int number);
    void jumpToPage();
signals:
    void currentPageChanged(int page);
private:
    void addPage(int page, bool selected = false);
    void updateButtons();
    void onClicked(int page);
    void updateThemeIcons();

    int pages_;
    int currentPage_ = 0;
    int maxVisible_;
    QHBoxLayout* layout_ = nullptr;
    qfw::TransparentToolButton* firstButton_ = nullptr;
    qfw::TransparentToolButton* lastButton_ = nullptr;
    qfw::TransparentToolButton* previousButton_ = nullptr;
    qfw::TransparentToolButton* nextButton_ = nullptr;
    qfw::BodyLabel* firstEllipsis_ = nullptr;
    qfw::BodyLabel* lastEllipsis_ = nullptr;
    qfw::BodyLabel* jumpLabel_ = nullptr;
    qfw::BodyLabel* countLabel_ = nullptr;
    qfw::LineEdit* jumpEdit_ = nullptr;
};
}  // namespace fkw
