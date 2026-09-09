// 东软充电桩平台 —— 服务器（headless，无 GUI）
// 职责：SQLite 初始化 -> 启动 TCP 业务服务(默认9000) -> 处理三类客户端：
//        充电用户端(USER_*/STATION_*/ORDER_*)、PC管理端(ADMIN_*/PILE_*/SALES_*)
// 内置模拟桩：ChargingServer 在进程内模拟充电桩终端（device_id=builtin），
// 直接复用 PileDeviceService/DeviceRegistry，单跑 Server 即有桩上报功率曲线。
// 用法：ChargingServer [数据库文件路径] [端口] [simPiles]
//       环境变量：SIM_PILES(默认自动=全部桩, 0关闭)
//       如 ChargingServer charging_platform.db 9000

#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>
#include <QStringList>
#include <QTimer>

#include "common/DbManager.h"
#include "common/TcpServer.h"
#include "common/ApiDefs.h"
#include "common/MinioClient.h"
#include "common/SmtpClient.h"
#include "service/OrderService.h"
#include "sim/BuiltinSimEngine.h"

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
            << QStringLiteral("[内置桩] 未指定台数，按全部桩模拟（%1 台）").arg(simPiles);
    }

    // 外部存储：MinIO（文件，如头像上传）。参数经环境变量注入：
    //   MINIO_ENDPOINT / MINIO_BUCKET
    MinioClient::configure(
        qEnvironmentVariable("MINIO_ENDPOINT", QStringLiteral("http://localhost:9010")),
        qEnvironmentVariable("MINIO_BUCKET", QStringLiteral("avatars")));

    // 邮箱（QQ/163）SMTP 发信：忘记密码验证码。
    // 未配置 SMTP_USER/SMTP_AUTH_CODE 时降级为演示模式（验证码打印到服务端日志）。
    int smtpPort = qEnvironmentVariableIntValue("SMTP_PORT");
    if (smtpPort <= 0 || smtpPort > 65535) smtpPort = 465;
    SmtpClient::configure(
        qEnvironmentVariable("SMTP_HOST", QString()),
        static_cast<quint16>(smtpPort),
        qEnvironmentVariable("SMTP_USER", QString()),
        qEnvironmentVariable("SMTP_AUTH_CODE", QString()),
        qEnvironmentVariable("SMTP_FROM", QString()));

    // 预约超时巡检：每 30 秒扫描过期预约，标记超时、释放电桩并施加处罚
    auto* sweepTimer = new QTimer(&app);
    QObject::connect(sweepTimer, &QTimer::timeout, []() {
        OrderService::sweepExpiredReservations();
    });
    sweepTimer->start(30000);

    TcpServer server;
    QObject::connect(&server, &TcpServer::logMessage, [](const QString& line) {
        qInfo().noquote() << line;
    });

    if (!server.start(port)) {
        qWarning() << "[ChargingServer] 启动失败：端口" << port << "被占用或监听失败";
        return 1;
    }

    qInfo().noquote() << QStringLiteral("============================================");

    // 内置模拟桩引擎：进程内模拟 N 台桩（device_id=builtin），复用 PileDeviceService/DeviceRegistry
    if (simPiles > 0) {
        auto* sim = new BuiltinSimEngine(&app);
        sim->configure(QStringLiteral("builtin"), simPiles);
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         sim, &BuiltinSimEngine::stop);
        sim->start();
    } else {
        qInfo().noquote() << QStringLiteral("[内置桩] 已禁用（SIM_PILES=0）");
    }

    qInfo().noquote() << QStringLiteral(" 东软充电桩应用管理平台 - 业务服务器");
    qInfo().noquote() << QStringLiteral(" 监听：0.0.0.0:%1").arg(port);
    qInfo().noquote() << QStringLiteral(" 数据库：%1").arg(dbPath);
    qInfo().noquote() << QStringLiteral(" 接入方：充电用户端 / PC管理端");
    qInfo().noquote() << QStringLiteral(" Ctrl+C 退出");
    qInfo().noquote() << QStringLiteral("============================================");

    return app.exec();
}
