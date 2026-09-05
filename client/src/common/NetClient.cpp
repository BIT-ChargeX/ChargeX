#include "NetClient.h"
#include "common/ApiDefs.h"
#include <QJsonDocument>
#include <QDataStream>
#include <QNetworkProxy>

NetClient& NetClient::instance() {
    static NetClient inst;
    return inst;
}

NetClient::NetClient(QObject* parent) : QObject(parent) {
    m_socket.setProxy(QNetworkProxy::NoProxy);
    connect(&m_socket, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::connected, this, &NetClient::onSocketConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &NetClient::onSocketDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &NetClient::onSocketError);

    m_reconnectTimer.setInterval(3000);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &NetClient::onReconnectTick);
}

void NetClient::connectToServer(const QString& host, quint16 port) {
    m_host = host;
    m_port = port;
    m_manualConnect = true;
    tryConnect();
}

void NetClient::disconnectFromServer() {
    m_manualConnect = false;
    m_reconnectTimer.stop();
    m_socket.abort();
}

NetClient::State NetClient::state() const {
    if (m_socket.state() == QAbstractSocket::ConnectedState) return State::Connected;
    if (m_socket.state() == QAbstractSocket::ConnectingState
        || m_socket.state() == QAbstractSocket::HostLookupState) return State::Connecting;
    return State::Disconnected;
}

bool NetClient::isConnected() const {
    return state() == State::Connected;
}

void NetClient::emitStateChanged() {
    emit stateChanged(static_cast<int>(state()));
}

void NetClient::tryConnect() {
    if (!m_manualConnect || m_host.isEmpty()) return;
    if (m_socket.state() != QAbstractSocket::UnconnectedState) return;
    m_socket.connectToHost(m_host, m_port);
    emitStateChanged();
    m_reconnectTimer.start();
}

void NetClient::onSocketConnected() {
    m_reconnectTimer.stop();
    emitStateChanged();
    emit connected();
}

void NetClient::onSocketDisconnected() {
    emitStateChanged();
    emit disconnected();
    if (m_manualConnect) m_reconnectTimer.start();
}

void NetClient::onSocketError() {
    emit errorOccurred(m_socket.errorString());
    if (m_socket.state() == QAbstractSocket::UnconnectedState && m_manualConnect) {
        m_reconnectTimer.start();
    }
}

void NetClient::onReconnectTick() {
    if (m_manualConnect && m_socket.state() == QAbstractSocket::UnconnectedState) {
        tryConnect();
    }
}

void NetClient::writeFrame(const QJsonObject& obj) {
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<quint32>(body.size());
    frame.append(body);
    m_socket.write(frame);
}

void NetClient::sendRequest(const QString& cmd, const QJsonObject& data, ResponseCallback cb) {
    if (state() != State::Connected) {
        // 服务器未连接：本地直接回失败，避免请求静默丢失
        if (cb) {
            QJsonObject empty;
            cb(empty, Api::LocalNetError, QStringLiteral("服务器未连接，请确认服务端已启动"));
        }
        return;
    }

    int seq = ++m_seqCounter;
    if (cb) m_pending.insert(seq, cb);

    QJsonObject req;
    req["cmd"] = cmd;
    req["seq"] = seq;
    req["data"] = data;
    writeFrame(req);
}

void NetClient::onReadyRead() {
    m_buffer.append(m_socket.readAll());

    while (true) {
        if (m_buffer.size() < 4) return;

        quint32 bodyLen;
        QDataStream ds(m_buffer);
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> bodyLen;

        if (static_cast<quint32>(m_buffer.size()) < 4 + bodyLen) return;

        QByteArray body = m_buffer.mid(4, bodyLen);
        m_buffer.remove(0, 4 + bodyLen);

        QJsonObject resp = QJsonDocument::fromJson(body).object();
        int seq = resp.value("seq").toInt();
        int code = resp.value("code").toInt();
        QString msg = resp.value("msg").toString();
        QJsonObject data = resp.value("data").toObject();

        if (m_pending.contains(seq)) {
            ResponseCallback cb = m_pending.take(seq);
            if (cb) cb(data, code, msg);
        }
    }
}
