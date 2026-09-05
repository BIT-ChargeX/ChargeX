// 充电桩模拟端（ChargingPileSim）程序入口
// 模拟“真实物理世界”的充电桩，作为第三个接入方经 TCP 连接 ChargingServer：
//  HELLO 绑定 → 每5s REPORT(状态/SOC/功率) → 取待办控制指令 → 执行 → RESULT 回执
// 用法：ChargingPileSim [服务器IP] [端口] [device_id] [pile_count]
//       默认 127.0.0.1 9000 dev-01 8

#include <QCoreApplication>
#include <QByteArray>

#include "SimClient.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ChargingPileSim"));

    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 9000;
    QString deviceId = QStringLiteral("dev-01");
    int pileCount = 8;

    if (argc > 1 && argv[1][0] != '\0') host = QString::fromLocal8Bit(argv[1]);
    if (argc > 2) {
        bool ok = false;
        const int p = QString::fromLocal8Bit(argv[2]).toInt(&ok);
        if (ok && p > 0 && p <= 65535) port = static_cast<quint16>(p);
    }
    if (argc > 3 && argv[3][0] != '\0') deviceId = QString::fromLocal8Bit(argv[3]);
    if (argc > 4) {
        bool ok = false;
        const int n = QString::fromLocal8Bit(argv[4]).toInt(&ok);
        if (ok && n > 0 && n <= 200) pileCount = n;
    }

    SimClient sim(host, port, deviceId, pileCount);
    sim.start();

    return app.exec();
}
