// 东软充电桩平台 —— 服务器（headless，无 GUI）
// 职责：SQLite 初始化 -> 启动 TCP 业务服务(默认9000) -> 处理三类客户端：
//        充电用户端(USER_*/STATION_*/ORDER_*)、PC管理端(ADMIN_*/PILE_*/SALES_*)
// 用法：ChargingServer [数据库文件路径] [端口]
//       如 ChargingServer charging_platform.db 9000

#include <QCoreApplication>
#include <QDebug>

#include "common/DbManager.h"
#include "common/TcpServer.h"
#include "common/ApiDefs.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ChargingServer"));

    QString dbPath = QString(Api::kDbFile);
    quint16 port = static_cast<quint16>(Api::kPort);

    if (argc > 1 && argv[1][0] != '\0') dbPath = QString::fromLocal8Bit(argv[1]);
    if (argc > 2) {
        bool ok = false;
        const int p = QString::fromLocal8Bit(argv[2]).toInt(&ok);
        if (ok && p > 0 && p <= 65535) port = static_cast<quint16>(p);
    }

    DbManager::init(dbPath);

    TcpServer server;
    QObject::connect(&server, &TcpServer::logMessage, [](const QString& line) {
        qInfo().noquote() << line;
    });

    if (!server.start(port)) {
        qWarning() << "[ChargingServer] 启动失败：端口" << port << "被占用或监听失败";
        return 1;
    }

    qInfo().noquote() << QStringLiteral("============================================");
    qInfo().noquote() << QStringLiteral(" 东软充电桩应用管理平台 - 业务服务器");
    qInfo().noquote() << QStringLiteral(" 监听：0.0.0.0:%1").arg(port);
    qInfo().noquote() << QStringLiteral(" 数据库：%1").arg(dbPath);
    qInfo().noquote() << QStringLiteral(" 接入方：充电用户端 / PC管理端");
    qInfo().noquote() << QStringLiteral(" Ctrl+C 退出");
    qInfo().noquote() << QStringLiteral("============================================");

    return app.exec();
}
