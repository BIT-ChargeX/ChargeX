#include "OrderService.h"
#include "DbManager.h"
#include "ApiDefs.h"
#include "DeviceRegistry.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QList>
#include <cmath>

namespace {

constexpr int kSlotMinutes = 30;            // 预约时段长度（分钟）
constexpr int kReservePenaltyOrders = 2;     // 预约超时后需完成的充电订单数

bool userOk(QSqlDatabase& db, int userId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT status FROM users WHERE user_id = ?;"));
    q.addBindValue(userId);
    return q.exec() && q.next() && q.value(0).toInt() == 1;
}

// 管理端展示手机号脱敏（138****5678）
QString maskPhone(const QString& phone) {
    if (phone.size() == 11) {
        return phone.left(3) + QStringLiteral("****") + phone.right(4);
    }
    return phone;
}

} // namespace

// 扫描所有“预约占用”订单：时段结束仍未开充 → 标记“已超时”、释放电桩、给用户施加处罚。
// 幂等（只处理 status=预约占用 且已过时段末的订单），可被后台定时器与各业务入口重复调用。
void OrderService::sweepExpiredReservations() {
    QSqlDatabase db = DbManager::threadDb();
    const QDateTime now = QDateTime::currentDateTime();

    struct Expired { int orderId; int userId; int pileId; };
    QList<Expired> expired;
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT order_id, user_id, pile_id, reserve_time FROM orders WHERE status = ?;"));
        q.addBindValue(QString(Api::OrderStatus::kReserved));
        if (!q.exec()) return;
        while (q.next()) {
            const QDateTime rt = QDateTime::fromString(q.value(3).toString(), Qt::ISODate);
            if (!rt.isValid()) continue;
            if (now >= rt.addSecs(kSlotMinutes * 60)) {
                expired.append({q.value(0).toInt(), q.value(1).toInt(), q.value(2).toInt()});
            }
        }
    }

    for (const auto& e : expired) {
        db.transaction();

        QSqlQuery upd(db);
        upd.prepare(QStringLiteral(
            "UPDATE orders SET status = ?, end_time = ? WHERE order_id = ? AND status = ?;"));
        upd.addBindValue(QString(Api::OrderStatus::kTimeout));
        upd.addBindValue(now.toString(Qt::ISODate));
        upd.addBindValue(e.orderId);
        upd.addBindValue(QString(Api::OrderStatus::kReserved));
        if (!upd.exec() || upd.numRowsAffected() == 0) {
            db.rollback();
            continue;
        }

        QSqlQuery freePile(db);
        freePile.prepare(QStringLiteral(
            "UPDATE piles SET status = ? WHERE pile_id = ? AND status = ?;"));
        freePile.addBindValue(QString(Api::PileStatus::kIdle));
        freePile.addBindValue(e.pileId);
        freePile.addBindValue(QString(Api::PileStatus::kReserved));
        freePile.exec();

        QSqlQuery pen(db);
        pen.prepare(QStringLiteral("UPDATE users SET reserve_penalty = ? WHERE user_id = ?;"));
        pen.addBindValue(kReservePenaltyOrders);
        pen.addBindValue(e.userId);
        pen.exec();

        db.commit();
    }
}

Api::Reply OrderService::checkUnfinished(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    sweepExpiredReservations();

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT o.order_id, o.pile_id, o.status, o.reserve_time, "
        "p.cur_power_kw, p.charge_done "
        "FROM orders o LEFT JOIN piles p ON p.pile_id = o.pile_id "
        "WHERE o.user_id = ? AND o.status IN (?,?,?) "
        "ORDER BY o.order_id DESC LIMIT 1;"));
    q.addBindValue(userId);
    q.addBindValue(QString(Api::OrderStatus::kReserved));
    q.addBindValue(QString(Api::OrderStatus::kCharging));
    q.addBindValue(QString(Api::OrderStatus::kPending));
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonObject out;
    if (q.next()) {
        out["has_unfinished"] = true;
        out["order_id"] = q.value(0).toInt();
        out["pile_id"] = q.value(1).toInt();
        out["status"] = q.value(2).toString();
        out["reserve_time"] = q.value(3).toString();
        out["cur_power"] = q.value(4).toDouble();
        out["charge_done"] = q.value(5).toInt() == 1;
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
    const QString reserveTimeStr = data.value("reserve_time").toString();

    sweepExpiredReservations();

    QSqlDatabase db = DbManager::threadDb();
    if (!userOk(db, userId)) {
        return Api::err(Api::StateConflict, QStringLiteral("用户不存在或已被冻结"));
    }

    // 超时处罚：预约超时后需完成若干充电订单才能再次预约
    QSqlQuery pen(db);
    pen.prepare(QStringLiteral("SELECT reserve_penalty FROM users WHERE user_id = ?;"));
    pen.addBindValue(userId);
    if (pen.exec() && pen.next() && pen.value(0).toInt() > 0) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("您上次预约超时，需完成 %1 个充电订单后才能再次预约")
                            .arg(pen.value(0).toInt()));
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

    // 预约时段校验：必须是当天、未来、半小时整点（如 14:00 / 14:30）
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime reserveTime = QDateTime::fromString(reserveTimeStr, Qt::ISODate);
    if (!reserveTime.isValid() || reserveTime <= now) {
        return Api::err(Api::InvalidParam, QStringLiteral("请选择当天未来的预约时段"));
    }
    if (reserveTime.date() != now.date()) {
        return Api::err(Api::InvalidParam, QStringLiteral("预约时段仅限当天"));
    }
    if (reserveTime.time().minute() % kSlotMinutes != 0 || reserveTime.time().second() != 0) {
        return Api::err(Api::InvalidParam, QStringLiteral("预约时段必须为半小时整点"));
    }
    const QString reserveTimeIso = reserveTime.toString(Qt::ISODate);

    db.transaction();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO orders (user_id, pile_id, reserve_time, status) VALUES (?,?,?,?);"));
    ins.addBindValue(userId);
    ins.addBindValue(pileId);
    ins.addBindValue(reserveTimeIso);
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
    out["reserve_time"] = reserveTimeIso;
    return Api::okData(out);
}

Api::Reply OrderService::create(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const int pileId = data.value("pile_id").toInt();
    if (userId <= 0 || pileId <= 0) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不完整"));
    }

    sweepExpiredReservations();

    QSqlDatabase db = DbManager::threadDb();
    if (!userOk(db, userId)) {
        return Api::err(Api::StateConflict, QStringLiteral("用户不存在或已被冻结"));
    }

    // 查找该用户对该桩的待开始预约（预约占用）
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral(
        "SELECT order_id, reserve_time FROM orders WHERE user_id = ? AND pile_id = ? AND status = ? "
        "ORDER BY order_id DESC LIMIT 1;"));
    sel.addBindValue(userId);
    sel.addBindValue(pileId);
    sel.addBindValue(QString(Api::OrderStatus::kReserved));
    sel.exec();

    int orderId = 0;
    if (sel.next()) {
        orderId = sel.value(0).toInt();
        const QDateTime reserveTime = QDateTime::fromString(sel.value(1).toString(), Qt::ISODate);
        // 预约时间未到则不允许开始
        if (reserveTime.isValid() && reserveTime > QDateTime::currentDateTime()) {
            return Api::err(Api::StateConflict,
                            QStringLiteral("预约时间未到，请 %1 后再开始充电")
                                .arg(reserveTime.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        }
    }

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

    // 通知已绑定的充电桩终端执行“上电”（未接入设备则忽略，服务器已直改库=回退）
    DeviceRegistry::instance().enqueue(pileId, DeviceCmd::kStart,
                                       QJsonObject{{"order_id", orderId}});

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

    // 每完成一笔订单，抵扣一次超时处罚（若处于处罚期内）
    QSqlQuery pen(db);
    pen.prepare(QStringLiteral(
        "UPDATE users SET reserve_penalty = "
        "CASE WHEN reserve_penalty > 0 THEN reserve_penalty - 1 ELSE 0 END "
        "WHERE user_id = ?;"));
    pen.addBindValue(userId);
    pen.exec();

    db.commit();

    // 结算完成 → 通知已绑定充电桩终端“断电释放”（未接入则忽略=回退）
    DeviceRegistry::instance().enqueue(pileId, DeviceCmd::kStop,
                                       QJsonObject{{"order_id", orderId}});

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

Api::Reply OrderService::listOrders(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT o.order_id, o.pile_id, o.reserve_time, o.start_time, o.end_time,
               o.amount, o.status, o.created_at,
               p.code, p.type, p.power_kw,
               s.station_id, s.name AS station_name
        FROM orders o
        JOIN piles p ON p.pile_id = o.pile_id
        LEFT JOIN stations s ON s.station_id = p.station_id
        WHERE o.user_id = ?
        ORDER BY o.order_id DESC;)SQL"));
    q.addBindValue(userId);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray orders;
    while (q.next()) {
        QJsonObject o;
        o["order_id"]     = q.value(0).toInt();
        o["pile_id"]      = q.value(1).toInt();
        o["reserve_time"] = q.value(2).toString();
        o["start_time"]   = q.value(3).toString();
        o["end_time"]     = q.value(4).toString();
        o["amount"]       = q.value(5).toDouble();
        o["status"]       = q.value(6).toString();
        o["created_at"]   = q.value(7).toString();
        o["pile_code"]    = q.value(8).toString();
        o["type"]         = q.value(9).toString();
        o["power_kw"]     = q.value(10).toDouble();
        o["station_id"]   = q.value(11).toInt();
        o["station_name"] = q.value(12).toString();
        orders.append(o);
    }

    QJsonObject out;
    out["orders"] = orders;
    return Api::okData(out);
}

// ORDER_MGMT_LIST：管理端分页订单查询（只读），支持状态/关键字/日期过滤
Api::Reply OrderService::mgmtList(const QJsonObject& data) {
    const int page = qMax(1, data.value("page").toInt());
    int pageSize = data.value("page_size").toInt();
    if (pageSize <= 0) pageSize = 20;
    if (pageSize > 100) pageSize = 100;

    const QString status = data.value("status").toString().trimmed();
    const QString keyword = data.value("keyword").toString().trimmed();
    const QString startDate = data.value("start_date").toString().trimmed();
    const QString endDate = data.value("end_date").toString().trimmed();

    QList<QVariant> binds;
    QStringList cond;
    if (!status.isEmpty()) {
        cond << QStringLiteral("o.status = ?");
        binds << status;
    }
    if (!keyword.isEmpty()) {
        cond << QStringLiteral("(u.phone LIKE ? OR u.nickname LIKE ? "
                               "OR p.code LIKE ? OR s.name LIKE ?)");
        const QString kw = QStringLiteral("%%1%").arg(keyword);
        for (int i = 0; i < 4; ++i) binds << kw;
    }
    if (!startDate.isEmpty()) {
        cond << QStringLiteral("substr(o.created_at, 1, 10) >= ?");
        binds << startDate;
    }
    if (!endDate.isEmpty()) {
        cond << QStringLiteral("substr(o.created_at, 1, 10) <= ?");
        binds << endDate;
    }
    const QString where = cond.isEmpty()
                              ? QString()
                              : QStringLiteral("WHERE ") + cond.join(QStringLiteral(" AND "));

    QSqlDatabase db = DbManager::threadDb();

    int total = 0;
    {
        QSqlQuery c(db);
        c.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM orders o "
            "LEFT JOIN users u ON u.user_id = o.user_id "
            "LEFT JOIN piles p ON p.pile_id = o.pile_id "
            "LEFT JOIN stations s ON s.station_id = p.station_id %1;")
                      .arg(where));
        for (const QVariant& v : binds) c.addBindValue(v);
        if (c.exec() && c.next()) total = c.value(0).toInt();
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT o.order_id, u.user_id, u.phone, u.nickname,
               COALESCE(s.station_id, 0), COALESCE(s.name, '-'),
               p.pile_id, p.code, p.type, p.power_kw,
               o.reserve_time, o.start_time, o.end_time,
               o.amount, o.status, o.created_at
        FROM orders o
        LEFT JOIN users u ON u.user_id = o.user_id
        LEFT JOIN piles p ON p.pile_id = o.pile_id
        LEFT JOIN stations s ON s.station_id = p.station_id
        %1 ORDER BY o.order_id DESC LIMIT %2 OFFSET %3;)SQL")
                  .arg(where)
                  .arg(pageSize)
                  .arg((page - 1) * pageSize));
    for (const QVariant& v : binds) q.addBindValue(v);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray orders;
    while (q.next()) {
        QJsonObject o;
        o["order_id"]     = q.value(0).toInt();
        o["user_id"]      = q.value(1).toInt();
        o["phone"]        = maskPhone(q.value(2).toString());
        o["nickname"]     = q.value(3).toString();
        o["station_id"]   = q.value(4).toInt();
        o["station"]      = q.value(5).toString();
        o["pile_id"]      = q.value(6).toInt();
        o["code"]         = q.value(7).toString();
        o["type"]         = q.value(8).toString();
        o["power_kw"]     = q.value(9).toDouble();
        o["reserve_time"] = q.value(10).toString();
        o["start_time"]   = q.value(11).toString();
        o["end_time"]     = q.value(12).toString();
        o["amount"]       = q.value(13).toDouble();
        o["status"]       = q.value(14).toString();
        o["created_at"]   = q.value(15).toString();
        orders.append(o);
    }

    QJsonObject out;
    out["orders"] = orders;
    out["total"] = total;
    out["page"] = page;
    out["page_size"] = pageSize;
    return Api::okData(out);
}

// ORDER_MGMT_CANCEL：管理端取消“预约占用”订单（释放电桩，写 ops_log）
Api::Reply OrderService::cancelReserved(const QJsonObject& data) {
    const int orderId = data.value("order_id").toInt();
    const QString operatorName = data.value("operator").toString();
    if (orderId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 order_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral(
        "SELECT pile_id, status FROM orders WHERE order_id = ?;"));
    sel.addBindValue(orderId);
    if (!sel.exec() || !sel.next()) {
        return Api::err(Api::NotFound, QStringLiteral("订单不存在"));
    }
    const int pileId = sel.value(0).toInt();
    const QString status = sel.value(1).toString();
    if (status != QString(Api::OrderStatus::kReserved)) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("仅预约占用状态的订单可被管理端取消"));
    }

    db.transaction();

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral(
        "UPDATE orders SET status = ?, end_time = ? "
        "WHERE order_id = ? AND status = ?;"));
    upd.addBindValue(QString(Api::OrderStatus::kCanceled));
    upd.addBindValue(now);
    upd.addBindValue(orderId);
    upd.addBindValue(QString(Api::OrderStatus::kReserved));
    if (!upd.exec() || upd.numRowsAffected() == 0) {
        db.rollback();
        return Api::err(Api::StateConflict,
                        QStringLiteral("订单状态已变化，取消失败，请刷新后重试"));
    }

    QSqlQuery freePile(db);
    freePile.prepare(QStringLiteral(
        "UPDATE piles SET status = ? WHERE pile_id = ?;"));
    freePile.addBindValue(QString(Api::PileStatus::kIdle));
    freePile.addBindValue(pileId);
    freePile.exec();

    QSqlQuery log(db);
    log.prepare(QStringLiteral(
        "INSERT INTO ops_log (pile_id, operator, action) VALUES (?,?,?);"));
    log.addBindValue(pileId);
    log.addBindValue(operatorName.isEmpty() ? QStringLiteral("PC管理端")
                                            : operatorName);
    log.addBindValue(QStringLiteral("取消预约订单(%1)").arg(orderId));
    log.exec();

    db.commit();

    QJsonObject out;
    out["order_id"] = orderId;
    out["status"] = QString(Api::OrderStatus::kCanceled);
    return Api::okData(out);
}
