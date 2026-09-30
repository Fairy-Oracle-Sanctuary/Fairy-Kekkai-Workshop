#include "components/dialog.h"

#include <QGridLayout>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QSizePolicy>

#include "common/app_data.h"
#include "common/text.h"
#include "common/text_format.h"
#include "components/notification_service.h"

namespace fkw {
BaseInputDialog::BaseInputDialog(const QString& title, int minimumWidth, QWidget* parent)
    : qfw::MessageBoxBase(parent) {
    auto* heading = new qfw::SubtitleLabel(wrapLongLatinRuns(title), this);
    QSizePolicy wrapPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    wrapPolicy.setHeightForWidth(true);
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    heading->setSizePolicy(wrapPolicy);
    heading->setToolTip(title);
    viewLayout->addWidget(heading);
    yesButton->setText(trText("确定"));
    cancelButton->setText(trText("取消"));
    widget->setMinimumWidth(minimumWidth);
}

AddProject::AddProject(QWidget* parent)
    : BaseInputDialog(trText("添加新项目"), 450, parent) {
    auto* grid = new QGridLayout();
    nameInput = new qfw::LineEdit(this);
    numInput = new qfw::LineEdit(this);
    titleInput = new qfw::LineEdit(this);
    nameInput->setPlaceholderText(trText("输入项目的名字"));
    numInput->setPlaceholderText(trText("输入这个系列一共几集"));
    titleInput->setPlaceholderText(trText("输入这个系列的原标题"));
    grid->addWidget(new qfw::StrongBodyLabel(trText("项目名称:"), this), 0, 0);
    grid->addWidget(nameInput, 0, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("总集数:"), this), 1, 0);
    grid->addWidget(numInput, 1, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("原标题:"), this), 2, 0);
    grid->addWidget(titleInput, 2, 1);
    grid->setColumnStretch(1, 1);
    viewLayout->addLayout(grid);
}
bool AddProject::validate() {
    bool countOk = false;
    const int count = numInput->text().toInt(&countOk);
    const bool valid = !nameInput->text().trimmed().isEmpty() &&
                       !titleInput->text().trimmed().isEmpty() && countOk && count > 0 && count < 128;
    nameInput->setError(nameInput->text().trimmed().isEmpty());
    numInput->setError(!countOk || count <= 0 || count >= 128);
    titleInput->setError(titleInput->text().trimmed().isEmpty());
    return valid;
}

EditProjectDialog::EditProjectDialog(const QString& name, const QString& title,
                                     const QString& icon, QWidget* parent)
    : BaseInputDialog(trText("编辑项目"), 450, parent) {
    auto* grid = new QGridLayout();
    nameInput = new qfw::LineEdit(this);
    titleInput = new qfw::LineEdit(this);
    iconInput = new qfw::ComboBox(this);
    nameInput->setPlaceholderText(trText("输入项目的名字"));
    titleInput->setPlaceholderText(trText("输入这个系列的原标题"));
    nameInput->setText(name);
    titleInput->setText(title);
    iconInput->addItems({trText("牛排"), trText("牛油果"), trText("番茄"),
                         trText("豆腐"), trText("紫薯"), trText("生菜")});
    const QStringList icons{QStringLiteral("牛排"), QStringLiteral("牛油果"),
        QStringLiteral("番茄"), QStringLiteral("豆腐"), QStringLiteral("紫薯"),
        QStringLiteral("生菜")};
    for (int i = 0; i < icons.size(); ++i)
        if (icon.endsWith(icons.at(i) + QStringLiteral(".svg")))
            iconInput->setCurrentIndex(i);
    grid->addWidget(new qfw::StrongBodyLabel(trText("项目名称:"), this), 0, 0);
    grid->addWidget(nameInput, 0, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("原标题:"), this), 1, 0);
    grid->addWidget(titleInput, 1, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("项目图标:"), this), 2, 0);
    grid->addWidget(iconInput, 2, 1);
    grid->setColumnStretch(1, 1);
    viewLayout->addLayout(grid);
}
QString EditProjectDialog::selectedIcon() const {
    const QStringList names{QStringLiteral("牛排"), QStringLiteral("牛油果"),
        QStringLiteral("番茄"), QStringLiteral("豆腐"), QStringLiteral("紫薯"),
        QStringLiteral("生菜")};
    return QStringLiteral(":/app/images/icons/") + names.at(iconInput->currentIndex())
           + QStringLiteral(".svg");
}
bool EditProjectDialog::validate() {
    const bool valid = !nameInput->text().trimmed().isEmpty() &&
                       !titleInput->text().trimmed().isEmpty();
    nameInput->setError(nameInput->text().trimmed().isEmpty());
    titleInput->setError(titleInput->text().trimmed().isEmpty());
    return valid;
}

PlaylistProjectDialog::PlaylistProjectDialog(QWidget* parent)
    : BaseInputDialog(trText("根据视频列表创建项目"), 500, parent) {
    auto* grid = new QGridLayout();
    urlInput = new qfw::LineEdit(this);
    nameInput = new qfw::LineEdit(this);
    titleInput = new qfw::LineEdit(this);
    urlInput->setPlaceholderText(trText("输入视频列表 URL"));
    nameInput->setPlaceholderText(trText("输入项目的名字"));
    titleInput->setPlaceholderText(trText("输入这个系列的原标题"));
    grid->addWidget(new qfw::StrongBodyLabel(trText("视频列表 URL"), this), 0, 0);
    grid->addWidget(urlInput, 0, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("项目名称"), this), 1, 0);
    grid->addWidget(nameInput, 1, 1);
    grid->addWidget(new qfw::StrongBodyLabel(trText("原标题"), this), 2, 0);
    grid->addWidget(titleInput, 2, 1);
    grid->setColumnStretch(1, 1);
    viewLayout->addLayout(grid);
}
bool PlaylistProjectDialog::validate() {
    const bool valid = !urlInput->text().trimmed().isEmpty() &&
                       !nameInput->text().trimmed().isEmpty() &&
                       !titleInput->text().trimmed().isEmpty();
    urlInput->setError(urlInput->text().trimmed().isEmpty());
    nameInput->setError(nameInput->text().trimmed().isEmpty());
    titleInput->setError(titleInput->text().trimmed().isEmpty());
    return valid;
}

ProjectProgressDialog::ProjectProgressDialog(const QString& title,
                                             const QVector<int>& percentages, QWidget* parent)
    : BaseInputDialog(title, 600, parent) {
    const QStringList names{trText("封面"), trText("原视频"), trText("翻译后的视频"),
        trText("原字幕"), trText("翻译后的字幕")};
    int sum = 0;
    for (int i = 0; i < names.size() && i < percentages.size(); ++i)
        sum += qBound(0, percentages.at(i), 100);
    const double average = sum / 5.0;

    auto* titleItem = viewLayout->takeAt(0);
    QWidget* heading = titleItem->widget();
    delete titleItem;
    auto* top = new QHBoxLayout();
    top->addWidget(heading, 1);
    auto* overall = new qfw::PillPushButton(
        QStringLiteral("%1%").arg(average, 0, 'f', 2), this);
    overall->setEnabled(false);
    top->addWidget(overall);
    viewLayout->addLayout(top);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(16);
    for (int i = 0; i < names.size(); ++i) {
        auto* ring = new qfw::ProgressRing(this, false);
        ring->setFixedSize(100, 100);
        ring->setRange(0, 100);
        ring->setTextVisible(true);
        ring->setValue(i < percentages.size() ? qBound(0, percentages.at(i), 100) : 0);
        auto* label = new qfw::StrongBodyLabel(names.at(i), this);
        label->setAlignment(Qt::AlignCenter);
        grid->addWidget(ring, 0, i, Qt::AlignCenter);
        grid->addWidget(label, 1, i, Qt::AlignCenter);
        grid->setColumnStretch(i, 1);
    }
    viewLayout->addLayout(grid);

    auto* totalBar = new qfw::ProgressBar(this, false);
    totalBar->setRange(0, 100);
    totalBar->setValue(static_cast<int>(average));
    viewLayout->addWidget(totalBar);
    yesButton->setText(trText("我知道了"));
    cancelButton->hide();
}

BatchDeleteDialog::BatchDeleteDialog(const QString& projectPath,
                                     const QStringList& titles, QWidget* parent,
                                     const QSet<int>& episodeScope)
    : BaseInputDialog(trText("批量删除文件"), 520, parent),
      projectPath_(projectPath), titles_(titles), episodeScope_(episodeScope) {
    fileType_ = new qfw::ComboBox(this);
    fileType_->addItems({QStringLiteral("封面.jpg"), QStringLiteral("生肉.mp4"),
        QStringLiteral("熟肉.mp4"), QStringLiteral("原文.srt"),
        QStringLiteral("原文_OCR.srt"), QStringLiteral("原文_Whisper.srt"),
        QStringLiteral("译文.srt")});
    viewLayout->addWidget(fileType_);
    auto* buttons = new QHBoxLayout();
    auto* all = new qfw::PushButton(trText("全选"), this);
    auto* none = new qfw::PushButton(trText("取消全选"), this);
    buttons->addWidget(all);
    buttons->addWidget(none);
    buttons->addStretch();
    viewLayout->addLayout(buttons);
    auto* scroll = new qfw::ScrollArea(this);
    auto* container = new QWidget(scroll);
    episodeLayout_ = new QVBoxLayout(container);
    episodeLayout_->setAlignment(Qt::AlignTop);
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(320);
    scroll->enableTransparentBackground();
    viewLayout->addWidget(scroll);
    yesButton->setText(trText("删除文件"));
    connect(fileType_, &qfw::ComboBox::currentIndexChanged,
            this, [this](int) { updateFiles(); });
    connect(all, &QPushButton::clicked, this, [this]() {
        for (const auto& choice : choices_) choice.first->setChecked(true);
    });
    connect(none, &QPushButton::clicked, this, [this]() {
        for (const auto& choice : choices_) choice.first->setChecked(false);
    });
    updateFiles();
}
void BatchDeleteDialog::updateFiles() {
    while (auto* item = episodeLayout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    choices_.clear();
    for (int i = 0; i < titles_.size(); ++i) {
        if (!episodeScope_.isEmpty() && !episodeScope_.contains(i + 1)) continue;
        const QString path = QDir(projectPath_).filePath(
            QString::number(i + 1) + QLatin1Char('/') + fileType_->currentText());
        if (!QFileInfo::exists(path)) continue;
        auto* choice = new qfw::CheckBox(
            QStringLiteral("第 %1 集  %2").arg(i + 1).arg(titles_.at(i)), this);
        episodeLayout_->addWidget(choice);
        choices_.append(qMakePair(choice, path));
    }
}
QStringList BatchDeleteDialog::selectedPaths() const {
    QStringList selected;
    for (const auto& choice : choices_)
        if (choice.first->isChecked()) selected << choice.second;
    return selected;
}
bool BatchDeleteDialog::validate() {
    if (!selectedPaths().isEmpty()) return true;
    NotificationService::warning(Text::instance().Info,
        Text::instance().NoFilesSelected, parentWidget());
    return false;
}

BatchTaskDialog::BatchTaskDialog(const QVector<Episode>& episodes, QWidget* parent,
                                 bool selectEligible)
    : BaseInputDialog(Text::instance().BatchAddTasks, 520, parent), episodes_(episodes),
      selectEligible_(selectEligible) {
    taskType_ = new qfw::ComboBox(this);
    taskType_->addItems({trText("下载"), trText("语音识别"), trText("翻译"), trText("压制")});
    viewLayout->addWidget(taskType_);
    auto* buttons = new QHBoxLayout();
    auto* all = new qfw::PushButton(Text::instance().SelectAll, this);
    auto* none = new qfw::PushButton(Text::instance().DeselectAll, this);
    buttons->addWidget(all);
    buttons->addWidget(none);
    buttons->addStretch();
    viewLayout->addLayout(buttons);
    auto* scroll = new qfw::ScrollArea(this);
    auto* container = new QWidget(scroll);
    episodeLayout_ = new QVBoxLayout(container);
    episodeLayout_->setAlignment(Qt::AlignTop);
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(320);
    scroll->enableTransparentBackground();
    viewLayout->addWidget(scroll);
    yesButton->setText(Text::instance().AddTask);
    yesButton->setEnabled(false);
    connect(taskType_, &qfw::ComboBox::currentIndexChanged,
            this, [this](int) { updateEpisodes(); });
    connect(all, &QPushButton::clicked, this, [this]() {
        for (const auto& choice : choices_) choice.first->setChecked(true);
    });
    connect(none, &QPushButton::clicked, this, [this]() {
        for (const auto& choice : choices_) choice.first->setChecked(false);
    });
    updateEpisodes();
}
void BatchTaskDialog::updateEpisodes() {
    while (auto* item = episodeLayout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    choices_.clear();
    yesButton->setEnabled(false);
    const int type = taskType_->currentIndex();
    for (const auto& episode : episodes_) {
        const QDir dir(episode.folderPath);
        bool eligible = false;
        if (type == 0)
            eligible = !episode.videoUrl.trimmed().isEmpty()
                && !QFileInfo::exists(dir.filePath(QStringLiteral("生肉.mp4")));
        else if (type == 1)
            eligible = QFileInfo::exists(dir.filePath(QStringLiteral("生肉.mp4")))
                && !QFileInfo::exists(dir.filePath(QStringLiteral("原文_Whisper.srt")));
        else if (type == 2)
            eligible = !QFileInfo::exists(dir.filePath(QStringLiteral("译文.srt")))
                && (QFileInfo::exists(dir.filePath(QStringLiteral("原文.srt")))
                    || QFileInfo::exists(dir.filePath(QStringLiteral("原文_OCR.srt")))
                    || QFileInfo::exists(dir.filePath(QStringLiteral("原文_Whisper.srt"))));
        else
            eligible = QFileInfo::exists(dir.filePath(QStringLiteral("熟肉.mp4")));
        if (!eligible) continue;
        auto* choice = new qfw::CheckBox(
            QStringLiteral("第 %1 集  %2").arg(episode.folderNum).arg(episode.title), this);
        connect(choice, &QCheckBox::stateChanged, this, [this](int) {
            for (const auto& item : choices_)
                if (item.first->isChecked()) {
                    yesButton->setEnabled(true);
                    return;
                }
            yesButton->setEnabled(false);
        });
        episodeLayout_->addWidget(choice);
        choices_.append(qMakePair(choice, qMakePair(episode.folderNum, episode.folderPath)));
        if (selectEligible_) choice->setChecked(true);
    }
}
QVector<BatchTaskDialog::Selection> BatchTaskDialog::selected() const {
    QVector<Selection> result;
    const int type = taskType_->currentIndex();
    for (const auto& choice : choices_)
        if (choice.first->isChecked())
            result.append({type, choice.second.first, choice.second.second});
    return result;
}
bool BatchTaskDialog::validate() {
    if (!selected().isEmpty()) return true;
    NotificationService::warning(Text::instance().Info,
        Text::instance().NoTasksSelected, parentWidget());
    return false;
}

EpisodeEditor::EpisodeEditor(const QString& heading, bool translated, QWidget* parent)
    : BaseInputDialog(heading, 500, parent) {
    auto* grid = new QGridLayout();
    originalInput = new qfw::LineEdit(this);
    urlInput = new qfw::LineEdit(this);
    originalInput->setPlaceholderText(trText("输入原标题"));
    urlInput->setPlaceholderText(trText("输入视频链接"));
    grid->addWidget(new qfw::StrongBodyLabel(trText("原标题"), this), 0, 0);
    grid->addWidget(originalInput, 0, 1);
    int row = 1;
    if (translated) {
        translatedInput = new qfw::LineEdit(this);
        translatedInput->setPlaceholderText(trText("输入译后标题"));
        grid->addWidget(new qfw::StrongBodyLabel(trText("译后标题"), this), row, 0);
        grid->addWidget(translatedInput, row++, 1);
    }
    grid->addWidget(new qfw::StrongBodyLabel(trText("视频链接"), this), row, 0);
    grid->addWidget(urlInput, row, 1);
    grid->setColumnStretch(1, 1);
    viewLayout->addLayout(grid);
}
bool EpisodeEditor::validate() {
    const bool originalOk = !originalInput->text().trimmed().isEmpty();
    const bool translatedOk = !translatedInput || !translatedInput->text().trimmed().isEmpty();
    const bool urlOk = !urlInput->text().trimmed().isEmpty();
    originalInput->setError(!originalOk);
    if (translatedInput) translatedInput->setError(!translatedOk);
    urlInput->setError(!urlOk);
    return originalOk && translatedOk && urlOk;
}

CustomMessageBox::CustomMessageBox(const QString& title, const QString& prompt,
                                   int minimumWidth, QWidget* parent)
    : BaseInputDialog(title, minimumWidth, parent) {
    viewLayout->addWidget(new qfw::BodyLabel(prompt, this));
    lineEdit = new qfw::LineEdit(this);
    lineEdit->setPlaceholderText(prompt);
    lineEdit->setClearButtonEnabled(true);
    viewLayout->addWidget(lineEdit);
}
bool CustomMessageBox::validate() {
    const bool valid = !lineEdit->text().trimmed().isEmpty();
    lineEdit->setError(!valid);
    return valid;
}
}
