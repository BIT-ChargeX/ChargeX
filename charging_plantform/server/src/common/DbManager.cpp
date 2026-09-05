#include "DbManager.h"
#include "ApiDefs.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QVariant>
#include <QPair>
#include <QList>
#include <QSet>
#include <QDebug>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTime>

QString DbManager::s_path = QString(Api::kDbFile);

QString DbManager::dbPath() { return s_path; }

QString uniqueConnName() {
    return QStringLiteral("conn_%1_%2")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

QSqlDatabase DbManager::threadDb() {
    thread_local QSqlDatabase db;
    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), uniqueConnName());
        db.setDatabaseName(s_path);
        if (!db.open()) {
            qWarning() << "[DbManager] open failed:" << db.lastError().text();
        }
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA busy_timeout = 3000;"));
    }
    return db;
}

void DbManager::init(const QString& dbPath) {
    s_path = dbPath;

    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));

    createSchema(db);
    ensurePileRealtimeColumns(db);
    seedDemo(db);
}

// 为 piles 表补充“实时遥测”列（充电桩终端上报字段）；幂等，老库无需重建
void DbManager::ensurePileRealtimeColumns(QSqlDatabase db) {
    QSet<QString> cols;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA table_info(piles);"));
        while (q.next()) cols.insert(q.value(1).toString());
    }
    struct Add { const char* name; const char* ddl; };
    const Add adds[] = {
        {"last_report", "ALTER TABLE piles ADD COLUMN last_report DATETIME"},
        {"soc", "ALTER TABLE piles ADD COLUMN soc INTEGER"},
        {"cur_power_kw", "ALTER TABLE piles ADD COLUMN cur_power_kw REAL"},
    };
    QSqlQuery q(db);
    for (const auto& a : adds) {
        if (cols.contains(QString::fromLatin1(a.name))) continue;
        q.exec(QString::fromLatin1(a.ddl));
    }
}

void DbManager::createSchema(QSqlDatabase db) {
    QSqlQuery q(db);
    const QStringList ddl = {
        // 用户表
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS users (
                user_id     INTEGER PRIMARY KEY AUTOINCREMENT,
                phone       VARCHAR(11) NOT NULL UNIQUE,
                nickname    VARCHAR(32) NOT NULL,
                avatar_url  VARCHAR(255) DEFAULT '',
                balance     DECIMAL(10,2) NOT NULL DEFAULT 0.00,
                status      INTEGER NOT NULL DEFAULT 1,
                reg_time    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
            );)SQL"),
        // 充电站表
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS stations (
                station_id  INTEGER PRIMARY KEY AUTOINCREMENT,
                name        VARCHAR(64) NOT NULL,
                address     VARCHAR(255) NOT NULL,
                lat         DECIMAL(10,6) NOT NULL,
                lng         DECIMAL(10,6) NOT NULL,
                price       DECIMAL(6,2) NOT NULL,
                pile_total  INTEGER NOT NULL DEFAULT 0,
                pile_free   INTEGER NOT NULL DEFAULT 0
            );)SQL"),
        // 充电桩表
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS piles (
                pile_id       INTEGER PRIMARY KEY AUTOINCREMENT,
                station_id    INTEGER NOT NULL REFERENCES stations(station_id),
                code          VARCHAR(32) NOT NULL,
                type          VARCHAR(8)  NOT NULL,
                power_kw      DECIMAL(6,2) NOT NULL,
                status        VARCHAR(8)  NOT NULL DEFAULT '闲置',
                total_times   INTEGER NOT NULL DEFAULT 0,
                total_hours   DECIMAL(10,2) NOT NULL DEFAULT 0
            );)SQL"),
        // 充电订单表（预约/充电/结算共用一张表，靠 status 区分）
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS orders (
                order_id      INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id       INTEGER NOT NULL REFERENCES users(user_id),
                pile_id       INTEGER NOT NULL REFERENCES piles(pile_id),
                reserve_time  DATETIME,
                start_time    DATETIME,
                end_time      DATETIME,
                amount        DECIMAL(10,2) DEFAULT 0,
                status        VARCHAR(12) NOT NULL DEFAULT '待结算',
                created_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
            );)SQL"),
        // 管理员表
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS admins (
                admin_id    INTEGER PRIMARY KEY AUTOINCREMENT,
                account     VARCHAR(32) NOT NULL UNIQUE,
                password    VARCHAR(64) NOT NULL,
                name        VARCHAR(32) DEFAULT ''
            );)SQL"),
        // 电桩操作日志（远程重启等）
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS ops_log (
                log_id    INTEGER PRIMARY KEY AUTOINCREMENT,
                pile_id   INTEGER NOT NULL,
                operator  VARCHAR(32) DEFAULT '',
                action    VARCHAR(64) DEFAULT '',
                op_time   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
            );)SQL"),
        // 充电桩终端实时运行事件（设备上线/状态变化/指令回执，事件级，避免逐心跳灌水）
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS pile_runtime_log (
                log_id        INTEGER PRIMARY KEY AUTOINCREMENT,
                ts            DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
                device_id     VARCHAR(32) DEFAULT '',
                pile_id       INTEGER NOT NULL DEFAULT 0,
                code          VARCHAR(32) DEFAULT '',
                event         VARCHAR(32) DEFAULT '',
                status        VARCHAR(8)  DEFAULT '',
                soc           INTEGER,
                cur_power_kw  REAL,
                detail        VARCHAR(128) DEFAULT ''
            );)SQL"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_prl_id ON pile_runtime_log(log_id DESC);"),
        // 默认管理员
        QStringLiteral("INSERT OR IGNORE INTO admins (account, password, name) "
                       "VALUES ('admin', '123456', '系统管理员');"),
        // 常用索引
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_piles_station ON piles(station_id);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_user ON orders(user_id);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_orders_pile_status ON orders(pile_id, status);"),
    };
    for (const QString& sql : ddl) {
        if (!q.exec(sql)) {
            qWarning() << "[DbManager] DDL error:" << q.lastError().text() << "\n" << sql;
        }
    }
}

// 演示数据：默认用户 + 北京 8 座站点 + 电桩 + 近30天订单（供销售业绩图）
void DbManager::seedDemo(QSqlDatabase db) {
    QSqlQuery q(db);

    q.exec(QStringLiteral("SELECT COUNT(*) FROM users;"));
    if (q.next() && q.value(0).toInt() == 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO users (phone, nickname, avatar_url, balance, status) "
            "VALUES (?, ?, '', ?, 1);"));
        const QList<QPair<QString, double>> demoUsers = {
            {QStringLiteral("13800001111"), 66.00},
            {QStringLiteral("13900002222"), 20.50},
            {QStringLiteral("13700003333"), 0.00},
        };
        for (const auto& u : demoUsers) {
            q.addBindValue(u.first);
            q.addBindValue(QStringLiteral("用户%1").arg(u.first.right(4)));
            q.addBindValue(u.second);
            q.exec();
        }
    }

    struct DemoStation { QString name; QString addr; double lat, lng, price; };
    const QList<DemoStation> demos = {
        {QStringLiteral("中关村充电站"), QStringLiteral("北京市海淀区中关村大街1号"), 39.9836, 116.3139, 1.60},
        {QStringLiteral("五道口充电站"), QStringLiteral("北京市海淀区成府路35号"),     39.9927, 116.3375, 1.50},
        {QStringLiteral("三里屯充电站"), QStringLiteral("北京市朝阳区工人体育场北路"), 39.9370, 116.4551, 2.00},
        {QStringLiteral("国贸充电站"),   QStringLiteral("北京市朝阳区建国门外大街"),   39.9087, 116.4551, 2.20},
        {QStringLiteral("金融街充电站"), QStringLiteral("北京市西城区金融大街"),       39.9153, 116.3571, 1.80},
        {QStringLiteral("王府井充电站"), QStringLiteral("北京市东城区王府井大街"),     39.9096, 116.4109, 1.90},
        {QStringLiteral("回龙观充电站"), QStringLiteral("北京市昌平区龙泽街道"),       40.0774, 116.3116, 1.40},
        {QStringLiteral("石景山万达充电站"), QStringLiteral("北京市石景山区万达广场"), 39.9066, 116.2249, 1.30},
    };

    q.exec(QStringLiteral("SELECT COUNT(*) FROM stations;"));
    bool stationsEmpty = q.next() && q.value(0).toInt() == 0;
    if (stationsEmpty) {
        for (const auto& s : demos) {
            q.prepare(QStringLiteral(
                "INSERT INTO stations (name, address, lat, lng, price) VALUES (?,?,?,?,?);"));
            q.addBindValue(s.name);
            q.addBindValue(s.addr);
            q.addBindValue(s.lat);
            q.addBindValue(s.lng);
            q.addBindValue(s.price);
            q.exec();
        }

        // 每站 4 桩：2 快充(120/60kW) + 2 慢充(7kW)，制造少量故障/在用状态便于演示监控
        for (int si = 0; si < demos.size(); ++si) {
            int stationId = si + 1;
            for (int pi = 0; pi < 4; ++pi) {
                bool fast = (pi < 2);
                double power = fast ? (pi == 0 ? 120.0 : 60.0) : 7.0;
                QString type = fast ? QStringLiteral("快充") : QStringLiteral("慢充");
                QString status = QString(Api::PileStatus::kIdle);
                if (si == 0 && pi == 3)      status = QString(Api::PileStatus::kFault);
                else if (si == 1 && pi == 1) status = QString(Api::PileStatus::kInUse);
                else if (si == 3 && pi == 0) status = QString(Api::PileStatus::kInUse);
                else if (si == 4 && pi == 2) status = QString(Api::PileStatus::kFault);

                q.prepare(QStringLiteral(
                    "INSERT INTO piles (station_id, code, type, power_kw, status, total_times, total_hours) "
                    "VALUES (?,?,?,?,?,?,?);"));
                q.addBindValue(stationId);
                q.addBindValue(QStringLiteral("P%1-%2").arg(stationId, 2, 10, QChar('0'))
                                                        .arg(pi + 1, 2, 10, QChar('0')));
                q.addBindValue(type);
                q.addBindValue(power);
                q.addBindValue(status);
                q.addBindValue((si + 1) * 3 + pi * 5);
                q.addBindValue((si + 1) * 1.5 + pi * 0.8);
                q.exec();
            }
        }

        // 同步站点空闲/总桩数
        q.exec(QStringLiteral(R"SQL(
            UPDATE stations SET
                pile_total = (SELECT COUNT(*) FROM piles WHERE piles.station_id = stations.station_id),
                pile_free  = (SELECT COUNT(*) FROM piles WHERE piles.station_id = stations.station_id
                              AND piles.status = '闲置');)SQL"));
    }

    // 近30天销售演示订单（只在一开始播种一次，避免每次重启叠加）
    q.exec(QStringLiteral("SELECT COUNT(*) FROM orders;"));
    bool ordersEmpty = q.next() && q.value(0).toInt() == 0;
    if (ordersEmpty) {
        const int pilesCount = [&]() {
            QSqlQuery c(db);
            c.exec(QStringLiteral("SELECT COUNT(*) FROM piles;"));
            return c.next() ? c.value(0).toInt() : 0;
        }();
        const int usersCount = [&]() {
            QSqlQuery c(db);
            c.exec(QStringLiteral("SELECT COUNT(*) FROM users;"));
            return c.next() ? c.value(0).toInt() : 0;
        }();
        if (pilesCount > 0 && usersCount > 0) {
            auto* rnd = QRandomGenerator::global();
            const QDateTime now = QDateTime::currentDateTime();
            for (int day = 30; day >= 0; --day) {
                const int n = 2 + rnd->bounded(4); // 每天 2~5 单
                for (int i = 0; i < n; ++i) {
                    const int pileId = 1 + rnd->bounded(pilesCount);
                    const int userId = 1 + rnd->bounded(usersCount);
                    const double amount = 30.0 + rnd->bounded(1000) / 10.0;
                    QDateTime dt = now.addDays(-day);
                    dt.setTime(QTime(rnd->bounded(8) + 7, rnd->bounded(60)));
                    q.prepare(QStringLiteral(R"SQL(
                        INSERT INTO orders (user_id, pile_id, reserve_time, start_time, end_time,
                                            amount, status, created_at)
                        VALUES (?,?,?,?,?,?,'已完成',?);)SQL"));
                    q.addBindValue(userId);
                    q.addBindValue(pileId);
                    q.addBindValue(dt.addSecs(-3600).toString(Qt::ISODate));
                    q.addBindValue(dt.toString(Qt::ISODate));
                    q.addBindValue(dt.addSecs(3600).toString(Qt::ISODate));
                    q.addBindValue(amount);
                    q.addBindValue(dt.toString(Qt::ISODate));
                    q.exec();
                }
            }
        }
    }
}
