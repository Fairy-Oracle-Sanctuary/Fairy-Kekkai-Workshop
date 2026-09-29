#include "components/file_item_widget.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include "components/notification_service.h"
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QUrl>
#include <functional>

#include "common/app_data.h"
#include "common/event_bus.h"
#include "components/dialog.h"
#include "common/text.h"
#include "common/text_format.h"

namespace fkw {
FileItemWidget::FileItemWidget(const QString& fileName, const QString& filePath,
                               qfw::FluentIconEnum icon, bool canDownload,
                               bool canExtract, bool canTranslate, bool canEncode,
                               bool canActivate, QWidget* parent,
                               const QString& videoUrl)
    : qfw::CardWidget(parent), filePath_(filePath) {
    setFixedHeight(50);
    setClickEnabled(true);
    setCursor(Qt::PointingHandCursor);
    const bool exists = QFileInfo::exists(filePath_);
    connect(this, &qfw::CardWidget::clicked, this, [this, exists]() {
        if (exists) {
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath_)))
                NotificationService::error(Text::instance().Error,
                    formatText(Text::instance().FailedToOpenFile, {filePath_}), this);
            return;
        }
        const QString source = QFileDialog::getOpenFileName(this, trText("选择文件"),
            QString(),
            QFileInfo(filePath_).fileName() + QStringLiteral(" (*") +
                QFileInfo(filePath_).suffix() + QStringLiteral(")"));
        if (source.isEmpty()) return;
        if (!QFile::copy(source, filePath_))
            NotificationService::error(Text::instance().Error,
                formatText(Text::instance().FileUploadFailed, {filePath_}), this);
        else {
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().FileUploaded,
                    {QFileInfo(filePath_).fileName()}), this);
            emit fileChanged();
        }
    });
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 5, 10, 5);
    row->setSpacing(5);
    auto* image = new qfw::IconWidget(qfw::FluentIcon(icon).qicon(), this);
    image->setFixedSize(24, 24);
    row->addWidget(image);
    row->addWidget(new qfw::BodyLabel(fileName, this));
    row->addStretch();
    auto* status = new QLabel(exists ? QStringLiteral("✓") : QStringLiteral("✗"), this);
    status->setStyleSheet(exists ? QStringLiteral("color: green; font-weight: bold;")
                                 : QStringLiteral("color: red; font-weight: bold;"));
    row->addWidget(status);
    auto addQuickTask = [this, row](bool visible, qfw::FluentIconEnum action,
                                    const QString& tooltip,
                                    const std::function<void()>& onClick) {
        if (!visible) return;
        auto* button = new qfw::TransparentToolButton(action, this);
        button->setFixedSize(32, 32);
        button->setToolTip(tooltip);
        connect(button, &QPushButton::clicked, this, onClick);
        row->addWidget(button);
    };
    if (!exists && canDownload && QFileInfo(filePath_).suffix().compare(
            QStringLiteral("mp4"), Qt::CaseInsensitive) == 0) {
        auto* download = new qfw::TransparentToolButton(qfw::FluentIconEnum::Download, this);
        download->setFixedSize(32, 32);
        download->setToolTip(trText("下载缺失的文件"));
        connect(download, &QPushButton::clicked, this, [this, videoUrl]() {
            CustomMessageBox dialog(trText("下载视频文件"), trText("请输入视频链接"), 450, window());
            dialog.lineEdit->setText(videoUrl);
            if (!dialog.exec()) return;
            const QString url = dialog.lineEdit->text().trimmed();
            if (url.isEmpty()) return;
            emit GlobalEventBus::instance().download_requested(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("video")},
                {QStringLiteral("url"), url},
                {QStringLiteral("save_path"), QFileInfo(filePath_).absolutePath()}});
        });
        row->addWidget(download);
    } else if (!exists && canDownload) {
        auto* download = new qfw::TransparentToolButton(qfw::FluentIconEnum::Download, this);
        download->setFixedSize(32, 32);
        download->setToolTip(trText("下载缺失的文件"));
        connect(download, &QPushButton::clicked, this, [this]() {
            CustomMessageBox dialog(trText("下载封面图片"), trText("请输入视频链接"), 450, window());
            if (!dialog.exec()) return;
            const QString url = dialog.lineEdit->text().trimmed();
            if (url.isEmpty()) return;
            emit GlobalEventBus::instance().download_requested(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("thumbnail")},
                {QStringLiteral("url"), url},
                {QStringLiteral("save_path"), QFileInfo(filePath_).absolutePath()}});
        });
        row->addWidget(download);
    }
    // 每张小卡片的「快速加任务」按钮（对齐 Python FileItemWidget.extractSubtitle /
    // extractWhisper / translateSubtitle）：直接投递任务，无需先切页再选文件
    addQuickTask(exists && canExtract, qfw::FluentIconEnum::Alignment,
                 Text::instance().OCRExtractSubtitles, [this]() {
        // 对齐 Python：先让字幕界面装载该视频，再切到字幕页
        // （Python 为 add_video_signal + switchToSampleCard，
        //  C++ 用 add_video_signal + navigation_requested 路由实现同一效果）
        emit GlobalEventBus::instance().add_video_signal(filePath_);
        emit GlobalEventBus::instance().navigation_requested(
            QJsonObject{{QStringLiteral("target"), QStringLiteral("ocr")}});
    });
    addQuickTask(exists && canExtract, qfw::FluentIconEnum::Microphone,
                 Text::instance().SRES, [this]() {
        // 对齐 Python extractWhisper：同目录 原文_Whisper.srt 作为输出
        const QFileInfo info(filePath_);
        emit GlobalEventBus::instance().whisper_requested(
            info.absoluteFilePath(),
            info.dir().filePath(QStringLiteral("原文_Whisper.srt")));
    });
    addQuickTask(!exists && canTranslate, qfw::FluentIconEnum::Globe,
                 Text::instance().TranslateSubtitles, [this]() {
        // 对齐 Python translateSubtitle：译文.srt 卡片上以同目录原文字幕为输入
        const QFileInfo info(filePath_);
        QString source = info.dir().filePath(QStringLiteral("原文.srt"));
        if (!QFileInfo::exists(source)) {
            // 原文.srt 缺失时回退到实际存在的原文（与批量任务 dispatchTask 一致，
            // 避免投递指向不存在文件的翻译任务）
            for (const char* name : {"原文_OCR.srt", "原文_Whisper.srt"}) {
                const QString candidate = info.dir().filePath(QString::fromUtf8(name));
                if (QFileInfo::exists(candidate)) {
                    source = candidate;
                    break;
                }
            }
        }
        emit GlobalEventBus::instance().translate_requested(
            source, info.absoluteFilePath());
    });
    if (canEncode) {
        auto* encode = new qfw::TransparentToolButton(qfw::FluentIconEnum::Video, this);
        encode->setToolTip(trText("视频压制"));
        encode->setFixedSize(32, 32);
        connect(encode, &QPushButton::clicked, this, [this]() {
            const QFileInfo info(filePath_);
            const QString output = info.dir().filePath(
                info.fileName() == QStringLiteral("熟肉.mp4")
                    ? QStringLiteral("熟肉_压制.mp4")
                    : info.completeBaseName() + QStringLiteral("_.mp4"));
            emit GlobalEventBus::instance().ffmpeg_requested(info.absoluteFilePath(), output);
        });
        row->addWidget(encode);
    }
    if (exists && canActivate) {
        auto* activate = new qfw::TransparentToolButton(qfw::FluentIconEnum::Accept, this);
        activate->setToolTip(trText("设为当前原文"));
        connect(activate, &QPushButton::clicked, this, [this]() {
            const QString target = QFileInfo(filePath_).absoluteDir().filePath(QStringLiteral("原文.srt"));
            if (QFileInfo::exists(target)) {
                qfw::MessageDialog confirm(trText("确认覆盖"), trText("覆盖当前原文字幕？"),
                                           window());
                confirm.yesButton->setText(Text::instance().OK);
                confirm.cancelButton->setText(Text::instance().Cancel);
                if (!confirm.exec()) return;
            }
            if (QFileInfo::exists(target) && !QFile::remove(target)) {
                NotificationService::error(trText("操作失败"), target, this); return;
            }
            if (!QFile::copy(filePath_, target))
                NotificationService::error(Text::instance().Error,
                    formatText(Text::instance().SettingFailed, {target}), this);
            else {
                NotificationService::success(Text::instance().Success,
                    formatText(Text::instance().SetAsCurrentOrigina2,
                        {QFileInfo(filePath_).fileName()}), this);
                emit fileChanged();
            }
        });
        row->addWidget(activate);
    }
    auto* open = new qfw::TransparentToolButton(qfw::FluentIconEnum::Folder, this);
    open->setToolTip(trText("打开文件位置"));
    open->setFixedSize(32, 32);
    connect(open, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileInfo(filePath_).absolutePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
            NotificationService::error(Text::instance().Error,
                formatText(Text::instance().CannotOpenFile, {folder}), this);
    });
    row->addWidget(open);
    auto* remove = new qfw::TransparentToolButton(qfw::FluentIconEnum::Delete, this);
    remove->setToolTip(trText("删除文件"));
    remove->setFixedSize(32, 32);
    remove->setEnabled(exists);
    connect(remove, &QPushButton::clicked, this, [this]() {
        const QString fileName = QFileInfo(filePath_).fileName();
        qfw::MessageDialog confirm(Text::instance().ConfirmDelete,
            formatText(Text::instance().AYSYWTDFTACBU, {fileName}), window());
        confirm.yesButton->setText(Text::instance().OK);
        confirm.cancelButton->setText(Text::instance().Cancel);
        if (!confirm.exec()) return;
        if (!QFile::remove(filePath_))
            NotificationService::error(Text::instance().Error,
                formatText(Text::instance().ErrorDeletingFile, {filePath_}), this);
        else {
            NotificationService::success(Text::instance().Success,
                formatText(Text::instance().FileDeleted, {fileName}), this);
            emit fileChanged();
        }
    });
    row->addWidget(remove);
}
}
