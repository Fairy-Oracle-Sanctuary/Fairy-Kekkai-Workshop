#pragma once
#include "components/base_function_interface.h"
#include "components/base_stacked_interface.h"
namespace fkw {
class FFmpegInterface : public BaseFunctionInterface {
    Q_OBJECT
public:
    explicit FFmpegInterface(QWidget* parent = nullptr);
};
class FFmpegStackedInterfaces : public BaseStackedInterfaces {
    Q_OBJECT
public:
    explicit FFmpegStackedInterfaces(QWidget* parent = nullptr);
};
}
