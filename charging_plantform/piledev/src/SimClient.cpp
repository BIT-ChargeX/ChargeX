#include "SimClient.h"
#include "PileModel.h"

#include <QDataStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QDebug>

namespace {
constexpr int kReportMs = 5000;
constexpr int kReconnectMs = 3000;
constexpr int kTickSec = 5;
}

SimClient::SimClient(const QString& host, quint16 port, const QString& deviceId,
                     int pileCount, QObject* parent)
    : QObject(parent), m_host(host), m_port(port), m_deviceId(deviceId),
      m_pileCount(pileCount) {
    m_socket.setProxy(QNetworkProxy::NoProxy);
    connect(&m_socket, &QTcpSocket::connected, this, &SimClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &SimClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &SimClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { log(m_socket.errorString()); });

    m_reportTimer.setInterval(kReportMs);
    connect(&m_reportTimer, &QTimer::timeout, this, &SimClient::onReportTick);

    m_reconnectTimer.setInterval(kReconnectMs);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SimClient::onReconnectTick);
}

void SimClient::start() {
    tryConnect();
}

void SimClient::log(const QString& line) {
    qInfo().noquote() << line;
    emit logMessage(line);
}

void SimClient::tryConnect() {
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        log(QStringLiteral("[桩端] 连接服务器 %1:%2 …").arg(m_host).arg(m_port));
        m_socket.connectToHost(m_host, m_port);
    }
}

void SimClient::onConnected() {
    m_reconnectTimer.stop();
    log(QStringLiteral("[桩端] 已连接，发送 HELLO(device=%1, count=%2)")
            .arg(m_deviceId).arg(m_pileCount));
    m_reportTimer.stop();
    sendHello();
}

void SimClient::onDisconnected() {
    m_reportTimer.stop();
    log(QStringLiteral("[桩端] 与服务器断开，%1s 后重连").arg(kReconnectMs / 1000));
    m_reconnectTimer.start();
}

void SimClient::onReconnectTick() {
    tryConnect();
}

void SimClient::onReportTick() {
    doReport();
}

void SimClient::sendRequest(const QString& cmd, const QJsonObject& data, Callback cb) {
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        if (cb) cb(QJsonObject(), -1, QStringLiteral("服务器未连接"));
        return;
    }
    int seq = ++m_seq;
    if (cb) m_pending.insert(seq, cb);

    QJsonObject req;
    req["cmd"] = cmd;
    req["seq"] = seq;
    req["data"] = data;
    writeFrame(req);
}

void SimClient::writeFrame(const QJsonObject& obj) {
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<quint32>(body.size());
    frame.append(body);
    m_socket.write(frame);
}

void SimClient::onReadyRead() {
    m_buffer.append(m_socket.readAll());

    while (true) {
        if (m_buffer.size() < 4) return;
        quint32 bodyLen;
        {
            QDataStream ds(m_buffer);
            ds.setByteOrder(QDataStream::BigEndian);
            ds >> bodyLen;
        }
        if (bodyLen == 0 || bodyLen > 1024 * 1024) { m_socket.abort(); return; }
        if (static_cast<quint32>(m_buffer.size()) < 4 + bodyLen) return;

        const QByteArray body = m_buffer.mid(4, static_cast<int>(bodyLen));
        m_buffer.remove(0, 4 + static_cast<int>(bodyLen));

        const QJsonObject resp = QJsonDocument::fromJson(body).object();
        const int seq = resp.value("seq").toInt();
        const int code = resp.value("code").toInt();
        const QString msg = resp.value("msg").toString();
        const QJsonObject data = resp.value("data").toObject();

        if (m_pending.contains(seq)) {
            Callback cb = m_pending.take(seq);
            if (cb) cb(data, code, msg);
        }
    }
}

void SimClient::sendHello() {
    QJsonObject data;
    data["device_id"] = m_deviceId;
    data["pile_count"] = m_pileCount;

    sendRequest("PILE_DEV_HELLO", data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                log(QStringLiteral("[桩端] HELLO 失败：%1").arg(msg));
                m_reconnectTimer.start();
                return;
            }
            m_models.clear();
            const QJsonArray bound = resp.value("bound").toArray();
            for (const auto& v : bound) {
                const QJsonObject b = v.toObject();
                PileModel pm;
                pm.setStatic(b.value("pile_id").toInt(),
                             b.value("code").toString(),
                             b.value("type").toString(),
                             b.value("power").toDouble(),
                             b.value("status").toString());
                m_models.append(pm);
            }
            log(QStringLiteral("[桩端] 绑定 %1 台电桩").arg(m_models.size()));
            if (m_models.isEmpty()) {
                log(QStringLiteral("[桩端] 无可用电桩，稍后重试 HELLO"));
                m_reconnectTimer.start();
                return;
            }
            m_reportTimer.start();
        });
}

void SimClient::doReport() {
    if (m_models.isEmpty() || m_socket.state() != QAbstractSocket::ConnectedState) return;

    QJsonArray reports;
    for (auto& pm : m_models) {
        pm.tick(kTickSec);
        reports.append(pm.report());
    }

    QJsonObject data;
    data["device_id"] = m_deviceId;
    data["reports"] = reports;

    sendRequest("PILE_DEV_REPORT", data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                log(QStringLiteral("[桩端] 上报失败：%1").arg(msg));
                return;
            }
            handlePending(resp.value("pending").toArray());
        });
}

void SimClient::handlePending(const QJsonArray& pending) {
    if (pending.isEmpty()) return;

    for (const auto& v : pending) {
        const QJsonObject p = v.toObject();
        const int pileId = p.value("pile_id").toInt();
        const int cmdId = p.value("cmd_id").toInt();
        const QString cmd = p.value("cmd").toString();

        for (auto& pm : m_models) {
            if (pm.pileId() != pileId) continue;
            const QString before = pm.status();
            pm.apply(cmd, p.value("data").toObject());
            log(QStringLiteral("[桩端] 电桩 %1(%2) 执行 %3：%4 → %5")
                    .arg(pileId).arg(pm.code()).arg(cmd).arg(before).arg(pm.status()));

            // 执行回执
            QJsonObject res;
            res["device_id"] = m_deviceId;
            res["pile_id"] = pileId;
            res["cmd_id"] = cmdId;
            res["result"] = QStringLiteral("ok");
            res["detail"] = QStringLiteral("已执行%1").arg(cmd);
            sendRequest("PILE_DEV_RESULT", res);
            break;
        }
    }
}
