#pragma once

#include <QString>
#include <qtfluentwidgets.h>

namespace fkw {
class FileItemWidget : public qfw::CardWidget {
    Q_OBJECT
public:
    FileItemWidget(const QString& fileName, const QString& filePath,
                   qfw::FluentIconEnum icon, bool canDownload,
                   bool canExtract, bool canTranslate, bool canEncode,
                   bool canActivate, QWidget* parent = nullptr,
                   const QString& videoUrl = {});
signals:
    void fileChanged();
private:
    QString filePath_;
};
}
