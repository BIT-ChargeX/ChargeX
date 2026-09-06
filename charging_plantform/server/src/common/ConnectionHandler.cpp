#include "ConnectionHandler.h"
#include "ApiDefs.h"
#include "SessionManager.h"
#include "service/UserService.h"
#include "service/StationService.h"
#include "service/OrderService.h"
#include "service/AdminService.h"
#include "service/PileService.h"
#include "service/PileDeviceService.h"
#include "service/SalesService.h"

#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace {

constexpr int kMaxBody = 1024 * 1024;   // 单条消息最大 1MB

// 需要管理员会话(token)的命令
bool isAdminCommand(const QString& cmd) {
    return cmd == Api::CmdUserList
        || cmd == Api::CmdUserFreeze
        || cmd == Api::CmdPileMgmtList
        || cmd == Api::CmdPileMgmtReboot
        || cmd == Api::CmdPileMgmtSetStatus
        || cmd == Api::CmdPileMonSummary
        || cmd == Api::CmdOpsLogList
        || cmd == Api::CmdStationMgmtList
        || cmd == Api::CmdStationMgmtAdd
        || cmd == Api::CmdSalesSummary
        || cmd == Api::CmdPileRuntimeLogList;
}

QByteArray frameOf(const QJsonObject& obj) {
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<quint32>(body.size());
    frame.append(body);
    return frame;
}

} // namespace

ConnectionHandler::ConnectionHandler(QTcpSocket* socket, QObject* parent)
    : QObject(parent), m_socket(socket) {
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &ConnectionHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() { emit finished(); });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this]() {
        // 出错视为断开，交由其上层回收
        m_socket->abort();
        emit finished();
    });
}

void ConnectionHandler::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    processFrames();
}

void ConnectionHandler::processFrames() {
    while (true) {
        if (m_buffer.size() < 4) return;

        quint32 bodyLen;
        {
            QDataStream ds(m_buffer);
            ds.setByteOrder(QDataStream::BigEndian);
            ds >> bodyLen;
        }
        if (bodyLen == 0 || bodyLen > kMaxBody) {
            // 畸形帧：断开
            m_socket->abort();
            emit finished();
            return;
        }
        if (static_cast<quint32>(m_buffer.size()) < 4 + bodyLen) return;

        const QByteArray body = m_buffer.mid(4, static_cast<int>(bodyLen));
        m_buffer.remove(0, 4 + static_cast<int>(bodyLen));

        const QJsonObject req = QJsonDocument::fromJson(body).object();
        const QString cmd = req.value("cmd").toString();
        const int seq = req.value("seq").toInt();
        QJsonObject data = req.value("data").toObject();

        Api::Reply reply;
        bool authorized = true;

        if (isAdminCommand(cmd)) {
            QString operatorName;
            int adminId = 0;
            if (!SessionManager::instance().verify(data.value("token").toString(),
                                                   &adminId, &operatorName)) {
                authorized = false;
                reply = Api::err(Api::Forbidden, QStringLiteral("登录已失效，请重新登录"));
            } else {
                data["operator"] = operatorName;
            }
        }

        if (authorized) {
            if (cmd == Api::CmdUserLogin)              reply = UserService::login(data);
            else if (cmd == Api::CmdUserUpdateProfile) reply = UserService::updateProfile(data);
            else if (cmd == Api::CmdUserRecharge)      reply = UserService::recharge(data);
            else if (cmd == Api::CmdUserRechargeRecords) reply = UserService::rechargeRecords(data);
            else if (cmd == Api::CmdUserGetBalance)    reply = UserService::getBalance(data);
            else if (cmd == Api::CmdUserCarbonStats)   reply = UserService::carbonStats(data);
            else if (cmd == Api::CmdUserPointsDetail)  reply = UserService::pointsDetail(data);
            else if (cmd == Api::CmdUserPointsRedeem)  reply = UserService::redeemPoints(data);
            else if (cmd == Api::CmdStationNearby)     reply = StationService::nearby(data);
            else if (cmd == Api::CmdStationDetail)     reply = StationService::detail(data);
            else if (cmd == Api::CmdPileDetailList)    reply = StationService::pileDetailList(data);
            else if (cmd == Api::CmdOrderCheckUnfinished) reply = OrderService::checkUnfinished(data);
            else if (cmd == Api::CmdOrderReserve)      reply = OrderService::reserve(data);
            else if (cmd == Api::CmdOrderCreate)       reply = OrderService::create(data);
            else if (cmd == Api::CmdOrderSettle)       reply = OrderService::settle(data);
            else if (cmd == Api::CmdAdminLogin)        reply = AdminService::login(data);
            else if (cmd == Api::CmdAdminLogout)       reply = AdminService::logout(data);
            else if (cmd == Api::CmdUserList)          reply = AdminService::userList(data);
            else if (cmd == Api::CmdUserFreeze)        reply = AdminService::freezeUser(data);
            else if (cmd == Api::CmdPileMgmtList)      reply = PileService::list(data);
            else if (cmd == Api::CmdPileMgmtReboot)    reply = PileService::reboot(data);
            else if (cmd == Api::CmdPileMgmtSetStatus) reply = PileService::setStatus(data);
            else if (cmd == Api::CmdPileMonSummary)    reply = PileService::summary(data);
            else if (cmd == Api::CmdOpsLogList)        reply = PileService::opsLogList(data);
            else if (cmd == Api::CmdStationMgmtList)   reply = StationService::mgmtList(data);
            else if (cmd == Api::CmdStationMgmtAdd)    reply = StationService::addStation(data);
            else if (cmd == Api::CmdSalesSummary)      reply = SalesService::summary(data);
            else if (cmd == Api::CmdPileRuntimeLogList) reply = PileDeviceService::runtimeLogList(data);
            else if (cmd == Api::CmdPileDevHello)      reply = PileDeviceService::hello(data);
            else if (cmd == Api::CmdPileDevReport)     reply = PileDeviceService::report(data);
            else if (cmd == Api::CmdPileDevResult)     reply = PileDeviceService::result(data);
            else                                       reply = Api::err(Api::NotFound, QStringLiteral("未支持的命令 %1").arg(cmd));
        }

        QJsonObject resp;
        resp["cmd"] = cmd;
        resp["seq"] = seq;
        resp["code"] = reply.code;
        resp["msg"] = reply.code == Api::Ok
                          ? reply.data.take("msg").toString(QStringLiteral("ok"))
                          : reply.data.take("msg").toString(QString(Api::errorText(reply.code)));
        resp["data"] = reply.data;

        writeFrame(resp);
    }
}

void ConnectionHandler::writeFrame(const QJsonObject& obj) {
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(frameOf(obj));
    }
}
