#include "PileDeviceService.h"
#include "DbManager.h"
#include "DeviceRegistry.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSet>

namespace {

QList<int> pileIdsOfDevice(const QString& deviceId) {
    return DeviceRegistry::instance().devicePiles(deviceId);
}

QSet<int> toSet(const QList<int>& ids) {
    QSet<int> s;
    for (int id : ids) s.insert(id);
    return s;
}

constexpr int kRuntimeKeep = 5000;

// 记录充电桩终端“事件级”运行日志（上线/状态变化/指令回执），并清理超出上限的旧记录
void writeRuntimeEvent(QSqlDatabase db, const QString& deviceId, int pileId,
                       const QString& code, const QString& event, const QString& status,
                       int soc, double powerKw, const QString& detail) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO pile_runtime_log (ts, device_id, pile_id, code, event, status, soc, "
        "cur_power_kw, detail) VALUES (?,?,?,?,?,?,?,?,?);"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(deviceId);
    q.addBindValue(pileId);
    q.addBindValue(code);
    q.addBindValue(event);
    q.addBindValue(status);
    q.addBindValue(soc);
    q.addBindValue(powerKw);
    q.addBindValue(detail);
    q.exec();

    QSqlQuery prune(db);
    prune.exec(QStringLiteral(
        "DELETE FROM pile_runtime_log WHERE log_id NOT IN "
        "(SELECT log_id FROM pile_runtime_log ORDER BY log_id DESC LIMIT %1);")
                   .arg(kRuntimeKeep));
}

} // namespace

Api::Reply PileDeviceService::hello(const QJsonObject& data) {
    const QString deviceId = data.value("device_id").toString().trimmed();
    int want = data.value("pile_count").toInt();
    if (deviceId.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("缺少 device_id"));
    if (want < 0) want = 0;
    if (want > 200) want = 200;

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral("SELECT pile_id, code, type, power_kw, status "
                          "FROM piles ORDER BY pile_id;"));

    QList<int> allIds;
    QHash<int, QJsonObject> info;
    while (q.next()) {
        const int pid = q.value(0).toInt();
        allIds.append(pid);
        QJsonObject o;
        o["pile_id"] = pid;
        o["code"] = q.value(1).toString();
        o["type"] = q.value(2).toString();
        o["power"] = q.value(3).toDouble();
        o["status"] = q.value(4).toString();
        info.insert(pid, o);
    }

    // 设备重复接入复用原绑定；否则从未绑定桩中按序分配
    const QList<int> chosen = DeviceRegistry::instance().allocate(deviceId, want, allIds);

    QJsonArray bound;
    for (int pid : chosen) {
        const QJsonObject it = info.value(pid);
        if (!it.isEmpty()) bound.append(it);
    }

    if (!bound.isEmpty()) {
        writeRuntimeEvent(db, deviceId, 0, QString(), QStringLiteral("上线绑定"), QString(),
                          0, 0.0, QStringLiteral("绑定%1台电桩").arg(bound.size()));
    }

    QJsonObject out;
    out["ok"] = true;
    out["device_id"] = deviceId;
    out["bound"] = bound;
    return Api::okData(out);
}

Api::Reply PileDeviceService::report(const QJsonObject& data) {
    const QString deviceId = data.value("device_id").toString();
    const QList<int> devicePiles = pileIdsOfDevice(deviceId);
    if (devicePiles.isEmpty()) {
        return Api::err(Api::NotFound, QStringLiteral("设备未注册或未绑定电桩"));
    }
    const QSet<int> owned = toSet(devicePiles);

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT status, code FROM piles WHERE pile_id = ?;"));

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QJsonArray reports = data.value("reports").toArray();

    QList<int> reportedIds;
    for (const auto& v : reports) {
        const QJsonObject r = v.toObject();
        const int pileId = r.value("pile_id").toInt();
        if (pileId <= 0 || !owned.contains(pileId)) continue;

        sel.bindValue(0, pileId);
        QString currentStatus;
        QString code;
        if (!sel.exec() || !sel.next()) continue;
        currentStatus = sel.value(0).toString();
        code = sel.value(1).toString();
        sel.finish();

        const QString inStatus = r.value("status").toString();
        // 自发故障：仅允许 闲置 -> 故障；其余状态一律以服务器为准，避免覆盖预约/在用业务态
        QString newStatus = currentStatus;
        if (inStatus == QStringLiteral("故障") && currentStatus == QStringLiteral("闲置")) {
            newStatus = QStringLiteral("故障");
            writeRuntimeEvent(db, deviceId, pileId, code, QStringLiteral("状态变化"),
                              newStatus, r.value("soc").toInt(),
                              r.value("cur_power").toDouble(),
                              QStringLiteral("设备上报：闲置→故障"));
        }

        QSqlQuery upd(db);
        upd.prepare(QStringLiteral(
            "UPDATE piles SET status = ?, soc = ?, cur_power_kw = ?, last_report = ? "
            "WHERE pile_id = ?;"));
        upd.addBindValue(newStatus);
        upd.addBindValue(r.value("soc").toInt());
        upd.addBindValue(r.value("cur_power").toDouble());
        upd.addBindValue(now);
        upd.addBindValue(pileId);
        upd.exec();

        reportedIds.append(pileId);
    }

    QJsonObject out;
    out["pending"] = DeviceRegistry::instance().takePending(reportedIds);
    return Api::okData(out);
}

Api::Reply PileDeviceService::result(const QJsonObject& data) {
    const QString deviceId = data.value("device_id").toString();
    const int cmdId = data.value("cmd_id").toInt();
    const int pileId = data.value("pile_id").toInt();
    const QString detail = data.value("detail").toString();
    if (cmdId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 cmd_id"));

    DeviceRegistry::instance().ackPending(cmdId);

    // 记录设备执行回执到操作日志（需求13 看得到执行结果）
    if (pileId > 0 && !detail.isEmpty()) {
        QSqlQuery q(DbManager::threadDb());
        q.prepare(QStringLiteral(
            "INSERT INTO ops_log (pile_id, operator, action) VALUES (?,?,?);"));
        q.addBindValue(pileId);
        q.addBindValue(QStringLiteral("设备:%1").arg(deviceId));
        q.addBindValue(detail);
        q.exec();
        writeRuntimeEvent(DbManager::threadDb(), deviceId, pileId, QString(),
                          QStringLiteral("指令回执"), QString(), 0, 0.0, detail);
    }

    QJsonObject out;
    out["ok"] = true;
    return Api::okData(out);
}

Api::Reply PileDeviceService::runtimeLogList(const QJsonObject& data) {
    int limit = data.value("limit").toInt();
    if (limit <= 0) limit = 100;
    if (limit > 500) limit = 500;

    QSqlDatabase db = DbManager::threadDb();

    QJsonArray logs;
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT ts, device_id, pile_id, code, event, status, soc, cur_power_kw, detail "
            "FROM pile_runtime_log ORDER BY log_id DESC LIMIT ?;"));
        q.addBindValue(limit);
        if (q.exec()) {
            while (q.next()) {
                QJsonObject o;
                o["ts"] = q.value(0).toString();
                o["device_id"] = q.value(1).toString();
                o["pile_id"] = q.value(2).toInt();
                o["code"] = q.value(3).toString();
                o["event"] = q.value(4).toString();
                o["status"] = q.value(5).toString();
                o["soc"] = q.value(6).toInt();
                o["cur_power"] = q.value(7).toDouble();
                o["detail"] = q.value(8).toString();
                logs.append(o);
            }
        }
    }

    // 在线桩：最近 15 秒内上报过
    const QString cutoff = QDateTime::currentDateTime()
                               .addSecs(-15)
                               .toString(Qt::ISODate);
    int online = 0;
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM piles WHERE last_report IS NOT NULL AND last_report >= ?;"));
        q.addBindValue(cutoff);
        if (q.exec() && q.next()) online = q.value(0).toInt();
    }

    QJsonObject out;
    out["logs"] = logs;
    out["online_piles"] = online;
    out["total_piles"] = DeviceRegistry::instance().boundCount();
    return Api::okData(out);
}
