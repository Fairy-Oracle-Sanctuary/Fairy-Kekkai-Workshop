#pragma once
#include <qtfluentwidgets.h>
#include <QVBoxLayout>
#include <QVector>

namespace fkw {
class BaseInputDialog : public qfw::MessageBoxBase {
    Q_OBJECT
public:
    BaseInputDialog(const QString& title, int minimumWidth = 400, QWidget* parent = nullptr);
};
class AddProject : public BaseInputDialog {
    Q_OBJECT
public:
    explicit AddProject(QWidget* parent = nullptr);
    qfw::LineEdit* nameInput;
    qfw::LineEdit* numInput;
    qfw::LineEdit* titleInput;
    bool validate() override;
};
class EditProjectDialog : public BaseInputDialog {
    Q_OBJECT
public:
    EditProjectDialog(const QString& name, const QString& title,
                      const QString& icon, QWidget* parent = nullptr);
    qfw::LineEdit* nameInput;
    qfw::LineEdit* titleInput;
    qfw::ComboBox* iconInput;
    QString selectedIcon() const;
    bool validate() override;
};
class PlaylistProjectDialog : public BaseInputDialog {
    Q_OBJECT
public:
    explicit PlaylistProjectDialog(QWidget* parent = nullptr);
    qfw::LineEdit* urlInput;
    qfw::LineEdit* nameInput;
    qfw::LineEdit* titleInput;
    bool validate() override;
};
class ProjectProgressDialog : public BaseInputDialog {
    Q_OBJECT
public:
    ProjectProgressDialog(const QString& title, const QVector<int>& percentages,
                          QWidget* parent = nullptr);
};
class BatchDeleteDialog : public BaseInputDialog {
    Q_OBJECT
public:
    BatchDeleteDialog(const QString& projectPath, const QStringList& titles,
                      QWidget* parent = nullptr);
    QStringList selectedPaths() const;
    bool validate() override;
private:
    void updateFiles();
    QString projectPath_;
    QStringList titles_;
    qfw::ComboBox* fileType_;
    QVBoxLayout* episodeLayout_;
    QVector<QPair<qfw::CheckBox*, QString>> choices_;
};
class BatchTaskDialog : public BaseInputDialog {
    Q_OBJECT
public:
    struct Episode {
        int folderNum;
        QString folderPath;
        QString title;
        QString videoUrl;
    };
    struct Selection {
        int taskType;
        int folderNum;
        QString folderPath;
    };
    explicit BatchTaskDialog(const QVector<Episode>& episodes, QWidget* parent = nullptr);
    QVector<Selection> selected() const;
    bool validate() override;
private:
    void updateEpisodes();
    QVector<Episode> episodes_;
    qfw::ComboBox* taskType_;
    QVBoxLayout* episodeLayout_;
    QVector<QPair<qfw::CheckBox*, QPair<int, QString>>> choices_;
};
class EpisodeEditor : public BaseInputDialog {
    Q_OBJECT
public:
    EpisodeEditor(const QString& heading, bool translated, QWidget* parent = nullptr);
    qfw::LineEdit* originalInput;
    qfw::LineEdit* translatedInput = nullptr;
    qfw::LineEdit* urlInput;
    bool validate() override;
};
class CustomMessageBox : public BaseInputDialog {
    Q_OBJECT
public:
    CustomMessageBox(const QString& title, const QString& prompt,
                     int minimumWidth = 500, QWidget* parent = nullptr);
    qfw::LineEdit* lineEdit;
    bool validate() override;
};
}
