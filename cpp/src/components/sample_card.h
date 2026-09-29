#pragma once

#include <QIcon>
#include <QUrl>
#include <qtfluentwidgets.h>

namespace fkw {
class BaseSampleCard : public qfw::CardWidget {
    Q_OBJECT
public:
    BaseSampleCard(const QIcon& icon, const QString& title, const QString& content,
                   QWidget* parent = nullptr);
};

class SampleCardView : public QWidget {
    Q_OBJECT
public:
    explicit SampleCardView(const QString& title, QWidget* parent = nullptr);
    void addSampleCard(const QIcon& icon, const QString& title, const QString& content,
                       const QString& routeKey);
    void addOpenUrlCard(const QIcon& icon, const QString& title, const QString& content,
                        const QUrl& url);
signals:
    void routeRequested(const QString& routeKey);
private:
    qfw::FlowLayout* flowLayout_;
};
}
