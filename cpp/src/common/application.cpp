#include "common/application.h"
#include <QIODevice>
#include <QLocalSocket>
#include <QThread>
#include "common/event_bus.h"

namespace fkw {
SingletonApplication::SingletonApplication(int& argc, char** argv, const QString& key)
    : QApplication(argc, argv), key_(key), server_(this), memory_(key, this) {
    bool attached = false;
    bool creator = false;
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (memory_.attach()) { attached = true; break; }
        if (memory_.create(1)) { creator = true; break; }
        if (memory_.isAttached()) memory_.detach();
        QThread::msleep(100);
    }
    if (attached) {
        isRunning_ = true;
        QStringList args = arguments();
        if (!args.isEmpty()) args.removeFirst();
        sendMessage(args.isEmpty() ? QStringLiteral("show")
                                   : args.join(QLatin1Char('\n')));
        return;
    }
    if (!creator) return;
    ownsInstance_ = true;
    connect(&server_, &QLocalServer::newConnection,
            this, &SingletonApplication::onNewConnection);
    QLocalServer::removeServer(key_);
    server_.listen(key_);
    connect(this, &SingletonApplication::messageSig,
            &GlobalEventBus::instance(), &GlobalEventBus::appMessageSig);
}
void SingletonApplication::onNewConnection() {
    while (auto* socket = server_.nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QString message = QString::fromUtf8(socket->readAll());
            emit messageSig(message);
            socket->disconnectFromServer();
            socket->deleteLater();
        });
    }
}
void SingletonApplication::requestRestart() {
    restartRequested_ = true;
    quit();
}
void SingletonApplication::releaseInstance() {
    if (!ownsInstance_) return;
    server_.close();
    QLocalServer::removeServer(key_);
    if (memory_.isAttached()) memory_.detach();
    ownsInstance_ = false;
}
bool SingletonApplication::sendMessage(const QString& message) {
    if (!isRunning_) return false;
    QLocalSocket socket(this);
    socket.connectToServer(key_, QIODevice::WriteOnly);
    if (!socket.waitForConnected(timeout_)) return false;
    socket.write(message.toUtf8());
    const bool written = socket.waitForBytesWritten(timeout_);
    socket.disconnectFromServer();
    return written;
}
}  // namespace fkw
