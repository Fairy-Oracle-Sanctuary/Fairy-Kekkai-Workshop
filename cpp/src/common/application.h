#pragma once
#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>
#include <QString>

namespace fkw {
class SingletonApplication : public QApplication {
    Q_OBJECT
public:
    SingletonApplication(int& argc, char** argv, const QString& key);
    bool isRunning() const { return isRunning_; }
    bool ownsInstance() const { return ownsInstance_; }
    bool restartRequested() const { return restartRequested_; }
    void requestRestart();
    void releaseInstance();
    bool sendMessage(const QString& message);
signals:
    void messageSig(const QString& message);
private:
    void onNewConnection();
    QString key_;
    int timeout_ = 1000;
    bool isRunning_ = false;
    bool ownsInstance_ = false;
    bool restartRequested_ = false;
    QLocalServer server_;
    QSharedMemory memory_;
};
}  // namespace fkw
