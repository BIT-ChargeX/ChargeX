// 东软充电桩平台 —— 服务器（headless，无 GUI）
// 职责：SQLite 初始化 -> 启动 TCP 业务服务(默认9000) -> 处理三类客户端：
//        充电用户端(USER_*/STATION_*/ORDER_*)、PC管理端(ADMIN_*/PILE_*/SALES_*)
// 可选托管模拟终端：自动拉起 ChargingPileSim 提供桩上报（单跑 Server 即可演示功率曲线）。
// 用法：ChargingServer [数据库文件路径] [端口] [simPiles]
//       环境变量：SIM_PILES(默认自动=全部桩, 0关闭)、PILESIM_BIN(可执行文件路径)
//       如 ChargingServer charging_platform.db 9000

#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>

#include "common/DbManager.h"
#include "common/TcpServer.h"
#include "common/ApiDefs.h"
#include "common/MinioClient.h"
#include "common/SimSupervisor.h"

#include <QFileInfo>
#include <QDir>

// 搜索 ChargingPileSim 可执行文件：优先环境变量 PILESIM_BIN，
// 否则在可执行文件/工作目录向上逐级查找常见构建位置。
QString findPileSimBinary() {
    const QString envBin = qEnvironmentVariable("PILESIM_BIN").trimmed();
    if (!envBin.isEmpty() && QFileInfo::exists(envBin)) return envBin;

    QStringList baseDirs;
    baseDirs << QDir::currentPath();
    QDir dir(QCoreApplication::applicationDirPath());
    baseDirs << dir.absolutePath();
    for (int i = 0; i < 4; ++i) {
        if (!dir.cdUp()) break;
        baseDirs << dir.absolutePath();
    }

    const QStringList rels = {
        QStringLiteral("ChargingPileSim"),
        QStringLiteral("ChargingPileSim.app/Contents/MacOS/ChargingPileSim"),
        QStringLiteral("piledev/build/Desktop-Debug/ChargingPileSim"),
        QStringLiteral("piledev/build/Desktop-Debug/ChargingPileSim/ChargingPileSim"),
        QStringLiteral("piledev/build/Desktop-Debug/ChargingPileSim.app/Contents/MacOS/ChargingPileSim"),
    };
    for (const QString& base : baseDirs) {
        for (const QString& rel : rels) {
            const QString cand = QDir(base).filePath(rel);
            if (QFileInfo::exists(cand)) return cand;
        }
    }
    return QString();
}

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

    // 托管模拟终端台数：argv[3] > 环境变量 SIM_PILES；都不给则自动=全部桩(auto)
    // 0 表示关闭托管；>0 表示指定台数
    int simPiles = -1;   // -1 = 待自动解析（默认全部桩）
    if (argc > 3) {
        bool ok = false;
        const int n = QString::fromLocal8Bit(argv[3]).toInt(&ok);
        if (ok) simPiles = qBound(0, n, 200);
    }
    if (simPiles < 0) {
        const QByteArray envSim = qgetenv("SIM_PILES");
        if (!envSim.isEmpty()) {
            bool ok = false;
            const int n = QString::fromLatin1(envSim).toInt(&ok);
            if (ok) simPiles = qBound(0, n, 200);
        }
    }

    DbManager::init(dbPath);

    // 未显式指定台数时默认绑定“全部桩”，保证任意一台“在用”桩都在上报
    if (simPiles < 0) {
        QSqlQuery c(DbManager::threadDb());
        if (c.exec(QStringLiteral("SELECT COUNT(*) FROM piles;")) && c.next())
            simPiles = qBound(0, c.value(0).toInt(), 200);
        else
            simPiles = 0;
        qInfo().noquote()
            << QStringLiteral("[托管] 未指定台数，按全部桩绑定（%1 台）").arg(simPiles);
    }

    // 外部存储：MinIO（文件，如头像上传）。参数经环境变量注入：
    //   MINIO_ENDPOINT / MINIO_BUCKET
    MinioClient::configure(
        qEnvironmentVariable("MINIO_ENDPOINT", QStringLiteral("http://localhost:9010")),
        qEnvironmentVariable("MINIO_BUCKET", QStringLiteral("avatars")));

    TcpServer server;
    QObject::connect(&server, &TcpServer::logMessage, [](const QString& line) {
        qInfo().noquote() << line;
    });

    if (!server.start(port)) {
        qWarning() << "[ChargingServer] 启动失败：端口" << port << "被占用或监听失败";
        return 1;
    }

    qInfo().noquote() << QStringLiteral("============================================");

    // 托管模拟终端：Server 自动拉起 ChargingPileSim，使“单跑 Server”即有桩上报
    if (simPiles > 0) {
        const QString bin = findPileSimBinary();
        if (bin.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[托管] 未找到 ChargingPileSim 可执行文件，跳过托管"
                                  "（可用 PILESIM_BIN 指定路径）；功率曲线需另起模拟终端");
        } else {
            auto* sim = new SimSupervisor(&app);
            sim->configure(bin, QStringLiteral("127.0.0.1"), port,
                           QStringLiteral("sim-child"), simPiles);
            QObject::connect(&app, &QCoreApplication::aboutToQuit,
                             sim, &SimSupervisor::stop);
            sim->start();
        }
    } else {
        qInfo().noquote() << QStringLiteral("[托管] 已禁用（SIM_PILES=0）");
    }

    qInfo().noquote() << QStringLiteral(" 东软充电桩应用管理平台 - 业务服务器");
    qInfo().noquote() << QStringLiteral(" 监听：0.0.0.0:%1").arg(port);
    qInfo().noquote() << QStringLiteral(" 数据库：%1").arg(dbPath);
    qInfo().noquote() << QStringLiteral(" 接入方：充电用户端 / PC管理端");
    qInfo().noquote() << QStringLiteral(" Ctrl+C 退出");
    qInfo().noquote() << QStringLiteral("============================================");

    return app.exec();
}
