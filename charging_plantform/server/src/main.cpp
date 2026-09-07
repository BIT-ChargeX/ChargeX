// 东软充电桩平台 —— 服务器（headless，无 GUI）
// 职责：SQLite 初始化 -> 启动 TCP 业务服务(默认9000) -> 处理三类客户端：
//        充电用户端(USER_*/STATION_*/ORDER_*)、PC管理端(ADMIN_*/PILE_*/SALES_*)
// 用法：ChargingServer [数据库文件路径] [端口]
//       如 ChargingServer charging_platform.db 9000

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QVector>
#include <QSqlQuery>
#include <QRandomGenerator>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <QStringList>

#include "common/DbManager.h"
#include "common/TcpServer.h"
#include "common/ApiDefs.h"
#include "common/InfluxClient.h"
#include "common/MinioClient.h"

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

    // 外部存储：InfluxDB 2.x（时序）与 MinIO（文件）。
    // 连接参数通过环境变量注入（便于容器化部署），未设置时使用以下默认值：
    //   INFLUX_URL / INFLUX_ORG / INFLUX_BUCKET / INFLUX_TOKEN
    //   MINIO_ENDPOINT / MINIO_BUCKET
    InfluxClient::configure(
        qEnvironmentVariable("INFLUX_URL", QStringLiteral("http://localhost:8086")),
        qEnvironmentVariable("INFLUX_ORG", QStringLiteral("myorg")),
        qEnvironmentVariable("INFLUX_BUCKET", QStringLiteral("charging")),
        qEnvironmentVariable("INFLUX_TOKEN", QStringLiteral("my-super-secret-token")));
    MinioClient::configure(
        qEnvironmentVariable("MINIO_ENDPOINT", QStringLiteral("http://localhost:9010")),
        qEnvironmentVariable("MINIO_BUCKET", QStringLiteral("avatars")));

    // 演示遥测：每 5 秒给每台桩生成一条采样并批量写入 InfluxDB
    QVector<int> pileIds;
    {
        QSqlDatabase db = DbManager::threadDb();
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT pile_id FROM piles ORDER BY pile_id;"));
        while (q.next()) pileIds.append(q.value(0).toInt());
    }

    auto* telemetryTimer = new QTimer(&app);
    QObject::connect(telemetryTimer, &QTimer::timeout, [pileIds]() {
        if (pileIds.isEmpty()) return;
        const qint64 ts = QDateTime::currentMSecsSinceEpoch();
        auto* rnd = QRandomGenerator::global();
        QStringList lines;
        for (int pileId : pileIds) {
            QMap<QString, QString> tags;
            tags[QStringLiteral("pile_id")] = QString::number(pileId);
            QMap<QString, QVariant> fields;
            const double voltage = 218.0 + rnd->bounded(50) / 10.0;   // 218~223V
            const double current = rnd->bounded(400) / 10.0;          // 0~40A
            const double powerKw = voltage * current / 1000.0;        // kW
            fields[QStringLiteral("voltage")] = voltage;
            fields[QStringLiteral("current")] = current;
            fields[QStringLiteral("power_kw")] = powerKw;
            lines << InfluxClient::line(QStringLiteral("pile_metrics"), tags, fields, ts);
        }
        InfluxClient::write(lines.join('\n'));
    });
    telemetryTimer->start(5000);

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
