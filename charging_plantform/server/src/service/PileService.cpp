#include "PileService.h"
#include "DbManager.h"
#include "DeviceRegistry.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>

Api::Reply PileService::list(const QJsonObject& data) {
    const int stationId = data.value("station_id").toInt();
    const int page = qMax(1, data.value("page").toInt());
    const int pageSize = 100;

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    QString sql = QStringLiteral(
        "SELECT p.pile_id, s.name, p.code, p.type, p.power_kw, p.status, "
        "p.total_times, p.total_hours "
        "FROM piles p LEFT JOIN stations s ON p.station_id = s.station_id ");
    if (stationId > 0) sql += QStringLiteral("WHERE p.station_id = %1 ").arg(stationId);
    sql += QStringLiteral("ORDER BY p.station_id, p.pile_id LIMIT %1 OFFSET %2;")
               .arg(pageSize).arg((page - 1) * pageSize);
    q.prepare(sql);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject p;
        p["pile_id"] = q.value(0).toInt();
        p["station"] = q.value(1).toString();
        p["code"] = q.value(2).toString();
        p["type"] = q.value(3).toString();
        p["power"] = q.value(4).toDouble();
        p["status"] = q.value(5).toString();
        p["total_times"] = q.value(6).toInt();
        p["total_hours"] = q.value(7).toDouble();
        arr.append(p);
    }

    QJsonObject out;
    out["piles"] = arr;
    return Api::okData(out);
}

Api::Reply PileService::reboot(const QJsonObject& data) {
    const int pileId = data.value("pile_id").toInt();
    const QString operatorName = data.value("operator").toString();
    if (pileId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 pile_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM piles WHERE pile_id = ?;"));
    q.addBindValue(pileId);
    if (!q.exec() || !q.next()) return Api::err(Api::NotFound, QStringLiteral("电桩不存在"));

    const QString status = q.value(0).toString();
    // 规则：故障等待报修（禁止重启）；预约占用禁止；闲置/在用可重启
    if (status == QString(Api::PileStatus::kFault)) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("电桩故障等待报修，禁止远程重启，请先线下检修"));
    }
    if (status == QString(Api::PileStatus::kReserved)) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("电桩被预约占用，禁止远程重启"));
    }

    db.transaction();
    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE piles SET status = ? WHERE pile_id = ?;"));
    upd.addBindValue(QString(Api::PileStatus::kIdle));
    upd.addBindValue(pileId);
    if (!upd.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, upd.lastError().text());
    }

    QSqlQuery log(db);
    log.prepare(QStringLiteral(
        "INSERT INTO ops_log (pile_id, operator, action) VALUES (?,?,?);"));
    log.addBindValue(pileId);
    log.addBindValue(operatorName.isEmpty() ? QStringLiteral("PC管理端")
                                            : operatorName);
    log.addBindValue(QStringLiteral("远程重启"));
    log.exec();
    db.commit();

    // 通知已绑定的充电桩终端执行重启（未接入则忽略=回退）
    DeviceRegistry::instance().enqueue(pileId, DeviceCmd::kReboot);

    QJsonObject out;
    out["result"] = QStringLiteral("ok");
    out["status"] = QString(Api::PileStatus::kIdle);
    return Api::okData(out);
}

Api::Reply PileService::setStatus(const QJsonObject& data) {
    const int pileId = data.value("pile_id").toInt();
    const QString status = data.value("status").toString();
    const QString operatorName = data.value("operator").toString();
    if (pileId <= 0
        || (status != QString(Api::PileStatus::kIdle)
            && status != QString(Api::PileStatus::kFault))) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不合法（status 仅支持 故障/闲置）"));
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM piles WHERE pile_id = ?;"));
    q.addBindValue(pileId);
    if (!q.exec() || !q.next()) return Api::err(Api::NotFound, QStringLiteral("电桩不存在"));

    const QString current = q.value(0).toString();
    const QString kIdle = QString(Api::PileStatus::kIdle);
    const QString kInUse = QString(Api::PileStatus::kInUse);
    const QString kFault = QString(Api::PileStatus::kFault);
    const QString kReserved = QString(Api::PileStatus::kReserved);

    // 状态机规则：
    //  - 设故障：仅适用于 闲置 / 在用
    //  - 恢复空闲：仅适用于 在用（故障=等待报修，禁止恢复）
    //  - 预约占用：所有操作禁止
    if (current == kReserved) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("电桩被预约占用，禁止变更状态"));
    }
    if (status == kFault) {
        if (current == kFault) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("电桩已处于故障，等待报修"));
        }
        if (current != kIdle && current != kInUse) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("仅闲置/在用充电桩可设为故障"));
        }
    } else {   // 恢复空闲（target = 闲置）
        if (current == kFault) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("电桩故障等待报修，禁止恢复空闲，请先线下检修"));
        }
        if (current == kIdle) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("电桩已是闲置状态，无需恢复"));
        }
        if (current != kInUse) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("仅在用充电桩可恢复为空闲"));
        }
    }

    const QString action = status == kFault
                               ? QStringLiteral("设置故障")
                               : QStringLiteral("恢复空闲");

    db.transaction();
    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE piles SET status = ? WHERE pile_id = ?;"));
    upd.addBindValue(status);
    upd.addBindValue(pileId);
    if (!upd.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, upd.lastError().text());
    }

    QSqlQuery log(db);
    log.prepare(QStringLiteral(
        "INSERT INTO ops_log (pile_id, operator, action) VALUES (?,?,?);"));
    log.addBindValue(pileId);
    log.addBindValue(operatorName.isEmpty() ? QStringLiteral("PC管理端") : operatorName);
    log.addBindValue(action);
    log.exec();
    db.commit();

    // 通知已绑定的充电桩终端同步该状态设置（未接入则忽略=回退）
    DeviceRegistry::instance().enqueue(pileId, DeviceCmd::kSetStatus,
                                       QJsonObject{{"status", status}});

    QJsonObject out;
    out["result"] = QStringLiteral("ok");
    out["status"] = status;
    return Api::okData(out);
}

Api::Reply PileService::opsLogList(const QJsonObject& data) {
    int limit = data.value("limit").toInt();
    if (limit <= 0 || limit > 500) limit = 100;

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT op_time, pile_id, operator, action FROM ops_log "
        "ORDER BY log_id DESC LIMIT ?;"));
    q.addBindValue(limit);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject o;
        o["time"] = q.value(0).toString();
        o["pile_id"] = q.value(1).toInt();
        o["operator"] = q.value(2).toString();
        o["action"] = q.value(3).toString();
        arr.append(o);
    }

    QJsonObject out;
    out["logs"] = arr;
    return Api::okData(out);
}

Api::Reply PileService::summary(const QJsonObject& /*data*/) {
    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT status, COUNT(*) FROM piles GROUP BY status;"));
    if (!q.isActive()) return Api::err(Api::ServerError, q.lastError().text());

    int idle = 0, inUse = 0, fault = 0, reserved = 0, total = 0;
    while (q.next()) {
        const QString s = q.value(0).toString();
        const int c = q.value(1).toInt();
        total += c;
        if (s == QString(Api::PileStatus::kIdle)) idle += c;
        else if (s == QString(Api::PileStatus::kInUse)) inUse += c;
        else if (s == QString(Api::PileStatus::kFault)) fault += c;
        else if (s == QString(Api::PileStatus::kReserved)) reserved += c;
    }
    const int busy = inUse + reserved;
    const double faultRatio = total > 0 ? (fault * 100.0) / total : 0.0;

    QJsonObject out;
    out["total"] = total;
    out["in_use"] = busy;
    out["idle"] = idle;
    out["fault"] = fault;
    out["fault_ratio"] = faultRatio;
    return Api::okData(out);
}
