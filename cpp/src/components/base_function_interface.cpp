#include "components/base_function_interface.h"
#include <QDir>

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>

#include "common/app_data.h"
#include "common/config.h"
#include "common/config_keys.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"

namespace fkw
{
    BaseFunctionInterface::BaseFunctionInterface(const QString &functionName,
                                                 qfw::FluentIconEnum inputIcon, QWidget *parent)
        : qfw::ScrollArea(parent)
    {
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAcceptDrops(true);
        auto *view = new QWidget(this);
        mainLayout_ = new QVBoxLayout(view);
        setWidget(view);
        enableTransparentBackground();
        fileSelectionGroup_ = new qfw::SettingCardGroup(trText("文件选择"), view);
        inputFileCard_ = new ChooseFileSettingCard(inputIcon,
                                                   formatText(Text::instance().File, {functionName}),
                                                   formatText(Text::instance().SelectFileTo, {functionName}),
                                                   trText("选择文件..."), fileSelectionGroup_);
        outputFileCard_ = new ChooseFileSettingCard(qfw::FluentIconEnum::Save,
                                                    trText("输出文件"),
                                                    formatText(Text::instance().SetSavePathForFile, {functionName}),
                                                    trText("输出文件路径..."), fileSelectionGroup_);
        inputFileCard_->browseBtn->setObjectName(QStringLiteral("tutorial-input"));
        outputFileCard_->browseBtn->setObjectName(QStringLiteral("tutorial-output"));
        fileSelectionGroup_->addSettingCard(inputFileCard_);
        fileSelectionGroup_->addSettingCard(outputFileCard_);
        mainLayout_->addWidget(fileSelectionGroup_);
        settingsGroup_ = new qfw::SettingCardGroup(
            joinTranslatedLabel(functionName, trText("设置")), view);
        mainLayout_->addWidget(settingsGroup_);
        auto *actions = new QHBoxLayout();
        startButton_ = new qfw::PrimaryPushButton(qfw::FluentIcon(qfw::FluentIconEnum::Play).qicon(),
                                                  trText("添加任务"), view);
        startButton_->setObjectName(QStringLiteral("tutorial-start"));
        startButton_->setEnabled(false);
        auto *previous = new qfw::PushButton(qfw::FluentIcon(qfw::FluentIconEnum::Up).qicon(),
                                             trText("上一个"), view);
        auto *next = new qfw::PushButton(qfw::FluentIcon(qfw::FluentIconEnum::Down).qicon(),
                                         trText("下一个"), view);
        previous->setEnabled(false);
        next->setEnabled(false);
        actions->addWidget(startButton_);
        actions->addWidget(previous);
        actions->addWidget(next);
        actions->addStretch();
        mainLayout_->addLayout(actions);
        mainLayout_->addStretch();
        connect(inputFileCard_->browseBtn, &QPushButton::clicked, this, [this]() {
        auto& config = AppConfig::instance();
        QString start = config.value(ConfigKeys::lastOpenPath).toString();
        if (!QFileInfo(start).isDir())
            start = QDir::homePath();
        const QString pattern = fileFilter_.isEmpty()
            ? QString() : formatText(Text::instance().FilesAllFiles, {fileFilter_});
        const QString file = QFileDialog::getOpenFileName(this, trText("选择文件"), start,
                                                          pattern);
        if (file.isEmpty()) return;
        setInputPath(file);
        config.set(ConfigKeys::lastOpenPath, QFileInfo(file).absolutePath());
    });
        connect(outputFileCard_->browseBtn, &QPushButton::clicked, this, [this]()
                {
        const QString file = QFileDialog::getSaveFileName(this, trText("保存文件"),
                                                           outputFileCard_->lineEdit->text());
        if (!file.isEmpty()) outputFileCard_->lineEdit->setText(file); });
        connect(startButton_, &QPushButton::clicked, this, [this]()
                {
        QString error;
        if (!validateBeforeStart(&error))
        {
            if (!error.isEmpty())
                NotificationService::error(Text::instance().Error, error);
            return;
        }
        emit taskRequested(inputPath(), outputPath()); });
    }

    bool BaseFunctionInterface::validateBeforeStart(QString *errorMessage)
    {
        Q_UNUSED(errorMessage);
        return true;
    }

    QString BaseFunctionInterface::inputPath() const { return inputFileCard_->lineEdit->text(); }
    QString BaseFunctionInterface::outputPath() const { return outputFileCard_->lineEdit->text(); }
    void BaseFunctionInterface::setOutputSuffix(const QString &suffix) { suffix_ = suffix; }
    void BaseFunctionInterface::setFileFilter(const QString &filter) { fileFilter_ = filter; }

    void BaseFunctionInterface::setSpecialFilenameMapping(const QHash<QString, QString> &mapping)
    {
        specialFilenameMapping_ = mapping;
    }

    void BaseFunctionInterface::setInputPath(const QString &path)
    {
        inputFileCard_->lineEdit->setText(path);
        const QFileInfo info(path);
        // 对齐 Python：命中特殊文件名映射时用映射名，否则用 stem + 默认后缀
        const auto mapped = specialFilenameMapping_.constFind(info.fileName());
        const QString outputName = mapped != specialFilenameMapping_.constEnd()
                                       ? mapped.value()
                                       : info.completeBaseName() + suffix_;
        outputFileCard_->lineEdit->setText(info.dir().filePath(outputName));
        startButton_->setEnabled(info.isFile());
    }

    void BaseFunctionInterface::insertPreview(QWidget *preview)
    {
        mainLayout_->insertWidget(mainLayout_->count() - 2, preview);
    }

    void BaseFunctionInterface::updateTask(bool duplicated, const QStringList &paths,
                                           bool notify)
    {
        if (duplicated && notify)
        {
            NotificationService::error(Text::instance().Error, Text::instance().DuplicateTask);
        }
        else if (!duplicated && notify)
        {
            NotificationService::success(Text::instance().Success,
                                         formatText(Text::instance().TAS,
                                                    {paths.isEmpty() ? QString()
                                                                     : paths.constLast()}));
        }
    }
}
