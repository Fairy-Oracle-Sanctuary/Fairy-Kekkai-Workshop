#pragma once
#include <QString>
namespace fkw {
struct PaddleOcrModelDirs {
    QString detection;
    QString recognition;
    QString classification;
};
PaddleOcrModelDirs resolveModelDirs(const QString& language, bool useServerModel,
                                    const QString& supportFilesPath = {});
}
