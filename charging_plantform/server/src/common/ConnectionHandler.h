#pragma once
#include <QObject>
#include <QByteArray>

class QTcpSocket;

// 单连接处理器：socket 随本对象 moveToThread 到专属线程，
// 在事件循环中按 4字节大端长度头 + JSON 逐帧解析并分发业务。
// 帧格式与客户端 NetClient 对齐（见 interface_protocol.md）。
class ConnectionHandler : public QObject {
    Q_OBJECT
public:
    explicit ConnectionHandler(QTcpSocket* socket, QObject* parent = nullptr);

public slots:
    void onReadyRead();

signals:
    void finished();

private:
    void processFrames();
    void writeFrame(const QJsonObject& obj);

    QTcpSocket* m_socket;
    QByteArray m_buffer;
};
