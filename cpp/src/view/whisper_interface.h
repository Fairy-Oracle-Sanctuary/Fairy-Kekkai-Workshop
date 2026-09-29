#pragma once
#include "components/base_function_interface.h"
#include "components/base_stacked_interface.h"
namespace fkw {
class WhisperInterface : public BaseFunctionInterface {
    Q_OBJECT
public:
    explicit WhisperInterface(QWidget* parent = nullptr);
protected:
    bool validateBeforeStart(QString* errorMessage) override;
};
class WhisperStackedInterfaces : public BaseStackedInterfaces {
    Q_OBJECT
public:
    explicit WhisperStackedInterfaces(QWidget* parent = nullptr);
};
}
