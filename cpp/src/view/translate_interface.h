#pragma once
#include "components/base_function_interface.h"
#include "components/base_stacked_interface.h"
namespace fkw {
class TranslationInterface : public BaseFunctionInterface {
    Q_OBJECT
public:
    explicit TranslationInterface(QWidget* parent = nullptr);
protected:
    bool validateBeforeStart(QString* errorMessage) override;
private:
    void loadSubtitlePreview(const QString& path);
    qfw::TableWidget* previewTable_ = nullptr;
    qfw::BodyLabel* statistics_ = nullptr;
};
class TranslateStackedInterfaces : public BaseStackedInterfaces {
    Q_OBJECT
public:
    explicit TranslateStackedInterfaces(QWidget* parent = nullptr);
};
}