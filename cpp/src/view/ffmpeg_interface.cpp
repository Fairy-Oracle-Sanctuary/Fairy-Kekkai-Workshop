#include "view/ffmpeg_interface.h"
#include "view/ffmpeg_task_interface.h"
#include "common/app_data.h"
#include "common/event_bus.h"
#include "common/text_format.h"

namespace fkw {
FFmpegInterface::FFmpegInterface(QWidget* parent)
    : BaseFunctionInterface(trText("压制"), qfw::FluentIconEnum::Video, parent) {
    setObjectName(QStringLiteral("ffmpegInterface"));
    setOutputSuffix(QStringLiteral("_compressed.mp4"));
    setFileFilter(QStringLiteral("*.mp4;*.flv;*.mkv;*.avi;*.wmv;*.mpg;*.avs;*.mov"));
    setSpecialFilenameMapping(
        {{QStringLiteral("熟肉.mp4"), QStringLiteral("熟肉_compressed.mp4")}});
    settingsGroup_->hide();
    fileSelectionGroup_->adjustSize();
    fileSelectionGroup_->setFixedHeight(fileSelectionGroup_->height());
    connect(&GlobalEventBus::instance(), &GlobalEventBus::ffmpeg_requested,
            this, [this](const QString& input, const QString& output) {
        emit taskRequested(input, output);
    });
}

FFmpegStackedInterfaces::FFmpegStackedInterfaces(QWidget* parent)
    : BaseStackedInterfaces(parent) {
    setObjectName(QStringLiteral("FFmpegStackedInterfaces"));
    auto* main = new FFmpegInterface(this);
    auto* tasks = new FFmpegTaskInterface(this);
    addSubInterface(main, QStringLiteral("mainInterface"), trText("视频压制"));
    addSubInterface(tasks, QStringLiteral("taskInterface"), joinTranslatedLabel(trText("视频压制"), trText("任务")));
    addSubInterface(new FFmpegSettingInterface(this), QStringLiteral("settingInterface"),
                    trText("高级设置"));
    connect(main, &BaseFunctionInterface::taskRequested, tasks,
            [tasks](const QString& input, const QString& output) {
                tasks->addTask(input, output);
            });
    connect(tasks, &BaseTaskInterface::returnTask, main,
            [main](bool duplicated, const QStringList& paths, bool notify) {
                main->updateTask(duplicated, paths, notify);
            });
}
}
