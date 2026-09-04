#include "OrderService.h"
#include "DbManager.h"
#include "ApiDefs.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QDateTime>
#include <cmath>

namespace {

bool userOk(QSqlDatabase& db, int userId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM users WHERE user_id = ?;"));
    q.addBindValue(userId);
    return q.exec() && q.next() && q.value(0).toInt() == 1;
}

} // namespace

Api::Reply OrderService::checkUnfinished(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT order_id FROM orders WHERE user_id = ? AND status IN (?,?,?) "
        "ORDER BY order_id DESC LIMIT 1;"));
    q.addBindValue(userId);
    q.addBindValue(QString(Api::OrderStatus::kReserved));
    q.addBindValue(QString(Api::OrderStatus::kCharging));
    q.addBindValue(QString(Api::OrderStatus::kPending));
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonObject out;
    if (q.next()) {
        out["has_unfinished"] = true;
        out["order_id"] = q.value(0).toInt();
    } else {
        out["has_unfinished"] = false;
    }
    return Api::okData(out);
}

Api::Reply OrderService::reserve(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const int pileId = data.value("pile_id").toInt();
    if (userId <= 0 || pileId <= 0) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不完整"));
    }
    const QString timeSlot = data.value("time_slot").toString();

    QSqlDatabase db = DbManager::threadDb();
    if (!userOk(db, userId)) {
        return Api::err(Api::StateConflict, QStringLiteral("用户不存在或已被冻结"));
    }

    // 需求8硬规则：用户存在未完成订单（预约/充电中/待结算）时禁止再次预约
    QSqlQuery busy(db);
    busy.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM orders WHERE user_id = ? AND status IN (?,?,?);"));
    busy.addBindValue(userId);
    busy.addBindValue(QString(Api::OrderStatus::kReserved));
    busy.addBindValue(QString(Api::OrderStatus::kCharging));
    busy.addBindValue(QString(Api::OrderStatus::kPending));
    if (busy.exec() && busy.next() && busy.value(0).toInt() > 0) {
        return Api::err(Api::StateConflict, QStringLiteral("您有未完成的充电订单，请先结算"));
    }

    // 校验电桩当前是否可预约
    QSqlQuery pile(db);
    pile.prepare(QStringLiteral("SELECT status FROM piles WHERE pile_id = ?;"));
    pile.addBindValue(pileId);
    if (!pile.exec() || !pile.next()) {
        return Api::err(Api::NotFound, QStringLiteral("电桩不存在"));
    }
    if (pile.value(0).toString() != QString(Api::PileStatus::kIdle)) {
        return Api::err(Api::StateConflict, QStringLiteral("该电桩当前不可用，请选择空闲电桩"));
    }

    // 防止同一电桩重复预约
    QSqlQuery dup(db);
    dup.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM orders WHERE pile_id = ? AND status IN (?,?,?);"));
    dup.addBindValue(pileId);
    dup.addBindValue(QString(Api::OrderStatus::kReserved));
    dup.addBindValue(QString(Api::OrderStatus::kCharging));
    dup.addBindValue(QString(Api::OrderStatus::kPending));
    if (dup.exec() && dup.next() && dup.value(0).toInt() > 0) {
        return Api::err(Api::StateConflict, QStringLiteral("该电桩已有进行中的预约或订单"));
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    db.transaction();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO orders (user_id, pile_id, reserve_time, status) VALUES (?,?,?,?);"));
    ins.addBindValue(userId);
    ins.addBindValue(pileId);
    ins.addBindValue(now);
    ins.addBindValue(QString(Api::OrderStatus::kReserved));
    if (!ins.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, ins.lastError().text());
    }
    const int orderId = ins.lastInsertId().toInt();

    QSqlQuery lockPile(db);
    lockPile.prepare(QStringLiteral("UPDATE piles SET status = ? WHERE pile_id = ?;"));
    lockPile.addBindValue(QString(Api::PileStatus::kReserved));
    lockPile.addBindValue(pileId);
    lockPile.exec();

    db.commit();

    QJsonObject out;
    out["reservation_id"] = orderId;
    return Api::okData(out);
}

Api::Reply OrderService::create(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const int pileId = data.value("pile_id").toInt();
    if (userId <= 0 || pileId <= 0) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不完整"));
    }

    QSqlDatabase db = DbManager::threadDb();
    if (!userOk(db, userId)) {
        return Api::err(Api::StateConflict, QStringLiteral("用户不存在或已被冻结"));
    }

    // 查找该用户对该桩的待开始预约（预约占用）
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral(
        "SELECT order_id FROM orders WHERE user_id = ? AND pile_id = ? AND status = ? "
        "ORDER BY order_id DESC LIMIT 1;"));
    sel.addBindValue(userId);
    sel.addBindValue(pileId);
    sel.addBindValue(QString(Api::OrderStatus::kReserved));
    sel.exec();

    int orderId = sel.next() ? sel.value(0).toInt() : 0;

    // 没有预约记录则兜底：桩空闲时直接生成订单（模拟“立即充电”）
    if (orderId == 0) {
        QSqlQuery pile(db);
        pile.prepare(QStringLiteral("SELECT status FROM piles WHERE pile_id = ?;"));
        pile.addBindValue(pileId);
        if (!pile.exec() || !pile.next()) {
            return Api::err(Api::NotFound, QStringLiteral("电桩不存在"));
        }
        if (pile.value(0).toString() != QString(Api::PileStatus::kIdle)) {
            return Api::err(Api::StateConflict, QStringLiteral("电桩当前不可用"));
        }
        const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT INTO orders (user_id, pile_id, start_time, status) VALUES (?,?,?,?);"));
        ins.addBindValue(userId);
        ins.addBindValue(pileId);
        ins.addBindValue(now);
        ins.addBindValue(QString(Api::OrderStatus::kCharging));
        if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());
        orderId = ins.lastInsertId().toInt();
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    db.transaction();

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral(
        "UPDATE orders SET start_time = ?, status = ? WHERE order_id = ?;"));
    upd.addBindValue(now);
    upd.addBindValue(QString(Api::OrderStatus::kCharging));
    upd.addBindValue(orderId);
    if (!upd.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, upd.lastError().text());
    }

    QSqlQuery p2(db);
    p2.prepare(QStringLiteral(
        "UPDATE piles SET status = ?, total_times = total_times + 1 WHERE pile_id = ?;"));
    p2.addBindValue(QString(Api::PileStatus::kInUse));
    p2.addBindValue(pileId);
    p2.exec();

    db.commit();

    QJsonObject out;
    out["order_id"] = orderId;
    out["status"] = QString(Api::OrderStatus::kCharging);
    return Api::okData(out);
}

Api::Reply OrderService::settle(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    int orderId = data.value("order_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();

    // 未指定订单则取用户最近一笔未完成订单
    if (orderId <= 0) {
        QSqlQuery latest(db);
        latest.prepare(QStringLiteral(
            "SELECT order_id FROM orders WHERE user_id = ? AND status IN (?,?,?) "
            "ORDER BY order_id DESC LIMIT 1;"));
        latest.addBindValue(userId);
        latest.addBindValue(QString(Api::OrderStatus::kReserved));
        latest.addBindValue(QString(Api::OrderStatus::kCharging));
        latest.addBindValue(QString(Api::OrderStatus::kPending));
        if (!latest.exec() || !latest.next())
            return Api::err(Api::NotFound, QStringLiteral("没有可结算的订单"));
        orderId = latest.value(0).toInt();
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT o.pile_id, o.status, o.start_time, u.balance,
               p.power_kw, s.price
        FROM orders o
        JOIN users u ON u.user_id = o.user_id
        LEFT JOIN piles p ON p.pile_id = o.pile_id
        LEFT JOIN stations s ON s.station_id = p.station_id
        WHERE o.order_id = ? AND o.user_id = ?;)SQL"));
    q.addBindValue(orderId);
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) {
        return Api::err(Api::NotFound, QStringLiteral("订单不存在"));
    }

    const int pileId = q.value(0).toInt();
    const QString status = q.value(1).toString();
    const QString startText = q.value(2).toString();
    const double balance = q.value(3).toDouble();
    const double powerKw = q.value(4).toDouble();
    const double price = q.value(5).toDouble();

    if (status != QString(Api::OrderStatus::kCharging)
        && status != QString(Api::OrderStatus::kPending)
        && status != QString(Api::OrderStatus::kReserved)) {
        return Api::err(Api::StateConflict, QStringLiteral("该订单已结算或已取消"));
    }

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 预约占用但未开充 → 视为取消，不产生费用
    if (status == QString(Api::OrderStatus::kReserved)) {
        db.transaction();
        QSqlQuery upd(db);
        upd.prepare(QStringLiteral("UPDATE orders SET status = ?, end_time = ? WHERE order_id = ?;"));
        upd.addBindValue(QString(Api::OrderStatus::kCanceled));
        upd.addBindValue(now);
        upd.addBindValue(orderId);
        upd.exec();
        QSqlQuery free(db);
        free.prepare(QStringLiteral("UPDATE piles SET status = ? WHERE pile_id = ?;"));
        free.addBindValue(QString(Api::PileStatus::kIdle));
        free.addBindValue(pileId);
        free.exec();
        db.commit();

        QJsonObject out;
        out["order_id"] = orderId;
        out["status"] = QString(Api::OrderStatus::kCanceled);
        out["amount"] = 0.0;
        out["balance"] = balance;
        return Api::okData(out);
    }

    // 模拟计费：充电时长 = 距开始时间（下限0.2h，上限12h）
    double hours = 1.0;
    QDateTime startDt = QDateTime::fromString(startText, Qt::ISODate);
    if (startDt.isValid()) {
        const double elapsed = startDt.secsTo(QDateTime::currentDateTime()) / 3600.0;
        hours = qBound(0.2, elapsed, 12.0);
    }
    const double amountRaw = powerKw * price * hours;
    const double amount = std::round(amountRaw * 100.0) / 100.0;

    if (balance + 1e-9 < amount) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("余额不足（本次需 ¥%1），请先充值").arg(amount, 0, 'f', 2));
    }

    db.transaction();

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral(R"SQL(
        UPDATE orders SET status = '已完成', amount = ?, end_time = ?
        WHERE order_id = ?;)SQL"));
    upd.addBindValue(amount);
    upd.addBindValue(now);
    upd.addBindValue(orderId);
    if (!upd.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, upd.lastError().text());
    }

    QSqlQuery bal(db);
    bal.prepare(QStringLiteral("UPDATE users SET balance = balance - ? WHERE user_id = ?;"));
    bal.addBindValue(amount);
    bal.addBindValue(userId);
    bal.exec();

    QSqlQuery pile(db);
    pile.prepare(QStringLiteral(
        "UPDATE piles SET status = '闲置', total_hours = total_hours + ? WHERE pile_id = ?;"));
    pile.addBindValue(hours);
    pile.addBindValue(pileId);
    pile.exec();

    db.commit();

    double newBalance = balance - amount;
    {
        QSqlQuery sel(db);
        sel.prepare(QStringLiteral("SELECT balance FROM users WHERE user_id = ?;"));
        sel.addBindValue(userId);
        if (sel.exec() && sel.next()) newBalance = sel.value(0).toDouble();
    }

    QJsonObject out;
    out["order_id"] = orderId;
    out["status"] = QString(Api::OrderStatus::kDone);
    out["amount"] = amount;
    out["balance"] = newBalance;
    return Api::okData(out);
}
