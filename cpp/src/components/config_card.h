#pragma once

#include <QLabel>
#include <QVBoxLayout>
#include <qtfluentwidgets.h>

#include "common/config_keys.h"

namespace fkw {
// 绑定 config 的开关设置卡片：初始值来自 config，切换时写回 config。
qfw::SwitchSettingCard* boundSwitch(QWidget* owner, qfw::SettingCardGroup* group,
                                    ConfigKeys::Key key, qfw::FluentIconEnum icon,
                                    const QString& title, const QString& description);

class ChooseFileSettingCard : public qfw::SettingCard {
    Q_OBJECT
public:
    ChooseFileSettingCard(qfw::FluentIconEnum icon, const QString& title,
                          const QString& content, const QString& placeholder,
                          QWidget* parent = nullptr);
    qfw::LineEdit* lineEdit;
    qfw::PushButton* browseBtn;
};

class DictSettingCard : public qfw::SettingCard {
    Q_OBJECT
public:
    DictSettingCard(qfw::FluentIconEnum icon, const QString& title,
                    const QString& content, const QStringList& options,
                    QWidget* parent = nullptr);
    qfw::ComboBox* comboBox;
};

// 绑定 config 的字典设置卡片：labels 为显示项，values 为存储值（一一对应）。
// 当前选中项由 config 值反查 values 得到，切换时写回 config。
DictSettingCard* boundChoice(QWidget* owner, qfw::SettingCardGroup* group,
                             ConfigKeys::Key key, qfw::FluentIconEnum icon,
                             const QString& title, const QString& description,
                             const QStringList& labels, const QStringList& values,
                             bool numeric = false);

class LineEditSettingCard : public qfw::SettingCard {
    Q_OBJECT
public:
    LineEditSettingCard(qfw::FluentIconEnum icon, const QString& title,
                        const QString& content, const QString& value = {},
                        bool password = false, QWidget* parent = nullptr);
    LineEditSettingCard(const QVariant& icon, const QString& title,
                        const QString& content, const QString& value = {},
                        bool password = false, QWidget* parent = nullptr);
    qfw::LineEdit* lineEdit;
};

class PlainTextSettingCard : public qfw::SettingCard {
    Q_OBJECT
public:
    PlainTextSettingCard(qfw::FluentIconEnum icon, const QString& title,
                         const QString& content, QWidget* parent = nullptr);
    qfw::PlainTextEdit* editor;
};

class FloatRangeSettingCard : public qfw::SettingCard {
    Q_OBJECT
public:
    FloatRangeSettingCard(qfw::FluentIconEnum icon, const QString& title,
                          const QString& content, double value, double minimum,
                          double maximum, QWidget* parent = nullptr);
    qfw::Slider* slider;
    QLabel* valueLabel;
};

class AdvancedSettingInterface : public qfw::ScrollArea {
    Q_OBJECT
public:
    AdvancedSettingInterface(const QString& title, QWidget* parent = nullptr);
    qfw::SettingCardGroup* addGroup(const QString& title);
    void finish();
protected:
    QVBoxLayout* layout_;
};

class YTDLPSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit YTDLPSettingInterface(QWidget* parent = nullptr);
};
class OCRSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit OCRSettingInterface(QWidget* parent = nullptr);
};
class TranslateSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit TranslateSettingInterface(QWidget* parent = nullptr);
private:
    void checkCustomModel(LineEditSettingCard* baseUrlCard, qfw::PushSettingCard* target);
    bool checkingModel_ = false;
};
class WhisperSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit WhisperSettingInterface(QWidget* parent = nullptr);
};
class FFmpegSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit FFmpegSettingInterface(QWidget* parent = nullptr);
};
class ReleaseSettingInterface : public AdvancedSettingInterface {
    Q_OBJECT
public:
    explicit ReleaseSettingInterface(QWidget* parent = nullptr);
};
}
