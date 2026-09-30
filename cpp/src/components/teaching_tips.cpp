#include "components/teaching_tips.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QFrame>
#include <QScrollArea>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <utility>
#include <qtfluentwidgets.h>
#include "common/config.h"
#include "common/text.h"
#include "components/base_stacked_interface.h"
#include "view/project_stacked_interface.h"

namespace fkw {
namespace {
// C++ TeachingTip 接收 FlyoutViewBase，内层仍使用 Python 同款 SimpleCardWidget。
class TutorialView : public qfw::FlyoutViewBase {
public:
    TutorialView() {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        auto* card = new qfw::SimpleCardWidget(this);
        outer->addWidget(card);
        contentLayout = new QVBoxLayout(card);
        contentLayout->setContentsMargins(16, 16, 16, 16);
        contentLayout->setSpacing(12);
        setFixedWidth(440);
    }
    void addWidget(QWidget* widget, int stretch = 0,
                   Qt::Alignment align = Qt::AlignLeft) override {
        contentLayout->addWidget(widget, stretch, align);
    }
    QVBoxLayout* contentLayout = nullptr;
};
}

TeachingTipManager::TeachingTipManager(
    QWidget* window, std::function<void(const QString&)> openRoute,
    std::function<QWidget*(const QString&)> routePage)
    : QObject(window), window_(window), openRoute_(std::move(openRoute)),
      routePage_(std::move(routePage)) {
    const auto& t = Text::instance();
    steps_ = {{t.WTFKW, t.TourWelcome, QStringLiteral("home"), QStringLiteral("tutorial-home-info")},
              {t.Project, t.TourFolder, QStringLiteral("settings"), QStringLiteral("tutorial-project-folder")},
              {t.NewProject, t.TourNewProject, QStringLiteral("projects"), QStringLiteral("tutorial-project-new")},
              {t.ProjectManagement, t.TourImport, QStringLiteral("projects"), QStringLiteral("tutorial-project-import")},
              {t.VideoListURL, t.TourPlaylist, QStringLiteral("projects"), QStringLiteral("tutorial-project-playlist")},
              {t.ProjectManagement, t.TourTabs, QStringLiteral("projects"), QStringLiteral("tutorial-project-tabs")},
              {t.AddDownloadTask, t.TourDownload, QStringLiteral("download"), QStringLiteral("tutorial-download-add")},
              {t.DownloadVideo, t.TourDownloadStatus, QStringLiteral("download"), QStringLiteral("tutorial-download-status")}};
#ifdef Q_OS_WIN
    steps_.append({t.OCRRecognition, t.TourOcrInput, QStringLiteral("ocr"), QStringLiteral("tutorial-input")});
    steps_.append({t.RecognitionLanguage, t.TourOcrLanguage, QStringLiteral("ocr"), QStringLiteral("tutorial-ocr-language")});
    steps_.append({t.AddTask, t.TourSubmit, QStringLiteral("ocr"), QStringLiteral("tutorial-start")});
    steps_.append({t.ModelPath, t.TourWhisperModel, QStringLiteral("whisper"), QStringLiteral("tutorial-setting-Whisper-ModelPath"), 2});
    steps_.append({t.SpeechRecognition, t.TourWhisperInput, QStringLiteral("whisper"), QStringLiteral("tutorial-input")});
    steps_.append({t.AddTask, t.TourSubmit, QStringLiteral("whisper"), QStringLiteral("tutorial-start")});
#endif
    steps_.append({t.APIKey, t.TourApiKey, QStringLiteral("translate"), QStringLiteral("tutorial-setting-Translate-DeepseekApiKey"), 2});
    steps_.append({t.AITranslation, t.TourSubtitleInput, QStringLiteral("translate"), QStringLiteral("tutorial-input")});
    steps_.append({t.TargetLanguage, t.TourTranslateOptions, QStringLiteral("translate"), QStringLiteral("tutorial-setting-Translate-TargetLang")});
    steps_.append({t.AIModel, t.TourModel, QStringLiteral("translate"), QStringLiteral("tutorial-setting-Translate-AiModel")});
    steps_.append({t.AddTask, t.TourSubmit, QStringLiteral("translate"), QStringLiteral("tutorial-start")});
    steps_.append({t.VideoEncoding, t.SelectAVideoFileInTh, QStringLiteral("ffmpeg"), QStringLiteral("tutorial-input")});
    steps_.append({t.VideoEncoding, t.TourOutput, QStringLiteral("ffmpeg"), QStringLiteral("tutorial-output")});
    steps_.append({t.AddTask, t.TourSubmit, QStringLiteral("ffmpeg"), QStringLiteral("tutorial-start")});
    steps_.append({t.AddTask, t.TourTasks, QStringLiteral("translate"), QStringLiteral("tutorial-page-tabs"), 1});
    steps_.append({t.SC, t.TourReplay, QStringLiteral("settings"), QStringLiteral("tutorial-replay")});
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &TeachingTipManager::showTip);
    positionTimer_.setSingleShot(true);
    connect(&positionTimer_, &QTimer::timeout, this, &TeachingTipManager::updatePosition);
}

QWidget* TeachingTipManager::prepareStep() {
    if (!window_ || currentStep_ >= steps_.size()) return nullptr;
    const auto& step = steps_.at(currentStep_);
    openRoute_(step.route);
    QWidget* page = routePage_(step.route);
    if (!page) return nullptr;
    if (auto* functions = qobject_cast<BaseStackedInterfaces*>(page)) functions->showPage(step.page);
    if (auto* projects = qobject_cast<ProjectStackedInterface*>(page)) projects->showProjectList();
    QWidget* anchor = page->findChild<QWidget*>(step.anchor);
    if (!anchor) return nullptr;
    // 路径设置卡片的实际动作位于其按钮上，提示直接挂按钮。
    if (qobject_cast<qfw::PushSettingCard*>(anchor))
        if (auto* button = anchor->findChild<QPushButton*>()) anchor = button;
    // 按控件身份定位，避免语言变化或已有项目数量变化导致误指。
    for (QWidget* ancestor = anchor->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
        if (auto* scroll = qobject_cast<QScrollArea*>(ancestor)) scroll->ensureWidgetVisible(anchor, 24, 24);
    return anchor;
}

void TeachingTipManager::restartTour(int delayMs) {
    timer_.stop();
    closeTip();
    currentStep_ = 0;
    if (delayMs > 0) {
        prepareStep();
        timer_.start(delayMs);
    } else showCurrentTip();
}

void TeachingTipManager::closeTip() {
    positionTimer_.stop();
    QObject::disconnect(targetDestroyed_);
    for (const auto& object : tracked_) if (object) object->removeEventFilter(this);
    tracked_.clear();
    if (highlight_) highlight_->hide();
    if (tip_) {
        if (window_) window_->removeEventFilter(tip_);
        tip_->close();
    }
    tip_.clear();
    target_.clear();
    positionManager_ = nullptr;
}

void TeachingTipManager::showCurrentTip() {
    if (!window_) return;
    if (currentStep_ >= steps_.size()) {
        finishTour();
        return;
    }
    prepareStep();
    timer_.start(300); // 与 Python 一致：等待页面切换完成。
}

void TeachingTipManager::showTip() {
    if (!window_) return;
    target_ = prepareStep();
    if (!target_ || !target_->isVisible()) {
        // 不把找不到的控件降级成泛泛的整窗提示。
        ++currentStep_;
        showCurrentTip();
        return;
    }
    const auto& step = steps_.at(currentStep_);
    const auto& t = Text::instance();
    auto* view = new TutorialView;
    auto* title = new qfw::StrongBodyLabel(step.title, view);
    auto* content = new qfw::BodyLabel(step.content, view);
    content->setWordWrap(true);
    view->contentLayout->addWidget(title);
    view->contentLayout->addWidget(content);
    view->contentLayout->addWidget(new qfw::BodyLabel(
        QStringLiteral("%1/%2").arg(currentStep_ + 1).arg(steps_.size()), view));
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    // 延后一轮再关闭提示，避免在按钮自身信号栈中销毁所属窗口。
    if (currentStep_ > 0) {
        auto* previous = new qfw::TransparentPushButton(t.Previous2, view);
        connect(previous, &QPushButton::clicked, this, [this]() {
            QTimer::singleShot(0, this, &TeachingTipManager::previousStep);
        });
        buttons->addWidget(previous);
    }
    auto* skip = new qfw::TransparentPushButton(t.Skip, view);
    connect(skip, &QPushButton::clicked, this, [this]() {
        QTimer::singleShot(0, this, &TeachingTipManager::finishTour);
    });
    buttons->addWidget(skip);
    const bool last = currentStep_ == steps_.size() - 1;
    auto* next = new qfw::PillPushButton(last ? t.Finish : t.Next2, view);
    connect(next, &QPushButton::clicked, this, [this, last]() {
        QTimer::singleShot(0, this, [this, last]() {
            if (last) finishTour();
            else nextStep();
        });
    });
    buttons->addWidget(next);
    view->contentLayout->addLayout(buttons);
    view->adjustSize();
    const QRect targetRect(target_->mapToGlobal(QPoint()), target_->size());
    const QRect available = window_->screen()->availableGeometry();
    const bool below = available.bottom() - targetRect.bottom() > view->sizeHint().height() + 60;
    const auto tail = below ? qfw::TeachingTipTailPosition::Top
                            : qfw::TeachingTipTailPosition::Bottom;
    tip_ = new qfw::TeachingTip(view, target_, -1, tail, window_);
    positionManager_ = qfw::TeachingTipManager::make(tail);
    positionManager_->setParent(tip_);
    if (!highlight_) {
        highlight_ = new QFrame(window_);
        highlight_->setAttribute(Qt::WA_TransparentForMouseEvents);
        highlight_->setStyleSheet(QStringLiteral("QFrame { border: 2px solid #009faa; border-radius: 6px; background: transparent; }"));
    }
    for (QWidget* object = target_; object; object = object->parentWidget()) {
        object->installEventFilter(this);
        tracked_.append(object);
    }
    targetDestroyed_ = connect(target_, &QObject::destroyed, this, [this]() {
        closeTip();
        QTimer::singleShot(0, this, &TeachingTipManager::showCurrentTip);
    });
    tip_->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    tip_->setWindowModality(Qt::ApplicationModal);
    tip_->show();
    updatePosition();
}

bool TeachingTipManager::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Show:
    case QEvent::Hide:
        if (tip_) positionTimer_.start(0);
        break;
    default: break;
    }
    return QObject::eventFilter(watched, event);
}

void TeachingTipManager::updatePosition() {
    if (!tip_ || !target_ || !window_ || !positionManager_) return;
    tip_->adjustSize();
    tip_->move(positionManager_->position(tip_));
    QRect rect(window_->mapFromGlobal(target_->mapToGlobal(QPoint())), target_->size());
    // 高亮框按所有祖先的可见区域裁切，不覆盖导航或视口外的区域。
    for (QWidget* ancestor = target_->parentWidget(); ancestor && ancestor != window_;
         ancestor = ancestor->parentWidget()) {
        const QRect visible(window_->mapFromGlobal(ancestor->mapToGlobal(QPoint())), ancestor->size());
        rect = rect.intersected(visible);
    }
    highlight_->setGeometry(rect.adjusted(-3, -3, 3, 3));
    highlight_->setVisible(target_->isVisible() && !rect.isEmpty());
    highlight_->raise();
    tip_->raise();
}

void TeachingTipManager::nextStep() {
    closeTip();
    ++currentStep_;
    showCurrentTip();
}
void TeachingTipManager::previousStep() {
    closeTip();
    if (currentStep_ > 0) --currentStep_;
    showCurrentTip();
}
void TeachingTipManager::finishTour() {
    timer_.stop();
    closeTip();
    AppConfig::instance().set(ConfigKeys::isFirstRun, false);
    if (window_) openRoute_(QStringLiteral("home"));
}
} // namespace fkw
