#pragma once

#include <QHash>
#include <QStringList>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

#include "components/config_card.h"

namespace fkw
{
    class BaseFunctionInterface : public qfw::ScrollArea
    {
        Q_OBJECT
    public:
        BaseFunctionInterface(const QString &functionName, qfw::FluentIconEnum inputIcon,
                              QWidget *parent = nullptr);
        QString inputPath() const;
        QString outputPath() const;
        virtual void setInputPath(const QString &path);
        void setOutputSuffix(const QString &suffix);
        void setFileFilter(const QString &filter);
        // 特殊文件名映射（对齐 Python special_filename_mapping）：
        // 命中输入文件名时直接使用映射的输出文件名，否则用默认后缀拼接。
        void setSpecialFilenameMapping(const QHash<QString, QString> &mapping);
    public slots:
        // 任务界面 returnTask 回路：重复任务报错，新任务提示添加成功
        void updateTask(bool duplicated, const QStringList &paths, bool notify);
    signals:
        void taskRequested(const QString &inputPath, const QString &outputPath);

    protected:
        QVBoxLayout *mainLayout_;
        qfw::SettingCardGroup *fileSelectionGroup_;
        qfw::SettingCardGroup *settingsGroup_;
        ChooseFileSettingCard *inputFileCard_;
        ChooseFileSettingCard *outputFileCard_;
        qfw::PrimaryPushButton *startButton_;
        void insertPreview(QWidget *preview);
        // 点击“添加任务”前的校验钩子（对齐 Python _start_processing 的前置检查）。
        // 返回 false 时中止本次添加，errorMessage 非空则弹出错误通知。
        virtual bool validateBeforeStart(QString *errorMessage);

    private:
        QString suffix_;
        QString fileFilter_;
        QHash<QString, QString> specialFilenameMapping_;
    };
}
