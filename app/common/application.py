import sys
import time

from PySide6.QtCore import QIODevice, QSharedMemory, Signal
from PySide6.QtNetwork import QLocalServer, QLocalSocket
from PySide6.QtWidgets import QApplication

from .event_bus import event_bus


class SingletonApplication(QApplication):
    """Singleton application"""

    messageSig = Signal(object)

    def __init__(self, argv: list, key: str):
        super().__init__(argv)
        self.key = key
        self.timeout = 1000
        self.server = QLocalServer(self)

        # cleanup (only needed for unix)
        QSharedMemory(key).attach()
        self.memory = QSharedMemory(self)
        self.memory.setKey(key)

        # 多进程同时启动时，交替尝试 attach 和 create
        # 谁先 create 成功谁是主实例，另一个 attach 到它
        is_attached = False
        is_creator = False
        for _ in range(30):  # 最多等待 3 秒
            if self.memory.attach():
                is_attached = True
                break
            if self.memory.create(1):
                is_creator = True
                break
            # create 也失败（已存在），detach 清理状态后重试
            self.memory.detach()
            time.sleep(0.1)

        if is_attached:
            self.isRunning = True
            self.sendMessage("\n".join(argv[1:]) if len(argv) > 1 else "show")
            sys.exit()

        if not is_creator:
            err = self.memory.errorString()
            print(f"Singleton: failed to attach or create: {err}", file=sys.stderr)
            raise RuntimeError(err)

        self.isRunning = False
        self.server.newConnection.connect(self.__onNewConnection)
        QLocalServer.removeServer(key)
        self.server.listen(key)

    def __onNewConnection(self):
        socket = self.server.nextPendingConnection()
        socket.setParent(self)  # 防止 GC
        socket.readyRead.connect(lambda s=socket: self.__onReadyRead(s))

    def __onReadyRead(self, socket):
        data = socket.readAll().data().decode("utf-8")
        event_bus.appMessageSig.emit(data)
        socket.disconnectFromServer()
        socket.deleteLater()

    def sendMessage(self, message: str):
        """send message to another application"""
        if not self.isRunning:
            return

        # connect to another application
        socket = QLocalSocket(self)
        socket.connectToServer(self.key, QIODevice.WriteOnly)
        if not socket.waitForConnected(self.timeout):
            print(socket.errorString(), file=sys.stderr)
            return

        # send message
        socket.write(message.encode("utf-8"))
        if not socket.waitForBytesWritten(self.timeout):
            print(socket.errorString(), file=sys.stderr)
            return

        socket.disconnectFromServer()
