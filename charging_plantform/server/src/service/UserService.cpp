#include "UserService.h"
#include "DbManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace {

bool validPhone(const QString& p) {
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    return re.match(p).hasMatch();
}

// 从系统配置表读取数值型配置，缺失/非法时回退默认值
double configDouble(QSqlDatabase& db, const QString& key, double fallback) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT cfg_value FROM sys_config WHERE cfg_key = ?;"));
    q.addBindValue(key);
    if (q.exec() && q.next()) {
        bool ok = false;
        const double v = q.value(0).toString().toDouble(&ok);
        if (ok) return v;
    }
    return fallback;
}

// 环保等级/称号（按累计碳积分划分）
QString ecoLevel(int points) {
    if (points >= 3000) return QStringLiteral("碳中和卫士");
    if (points >= 1000) return QStringLiteral("环保达人");
    if (points >= 500)  return QStringLiteral("低碳先锋");
    if (points >= 100)  return QStringLiteral("绿色出行者");
    return QStringLiteral("环保新秀");
}

// 时间格式化：ISO -> yyyy-MM-dd HH:mm:ss
QString fmtTime(const QString& iso) {
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : iso;
}

// 用户累计充电量(kWh)：已完成订单 = 桩功率(kW) × 充电时长(小时)
double userEnergy(QSqlDatabase& db, int userId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT COALESCE(SUM(
                 p.power_kw *
                 (julianday(o.end_time) - julianday(o.start_time)) * 24.0), 0.0)
        FROM orders o JOIN piles p ON p.pile_id = o.pile_id
        WHERE o.user_id = ? AND o.status = ?;)SQL"));
    q.addBindValue(userId);
    q.addBindValue(QString(Api::OrderStatus::kDone));
    if (!q.exec()) return 0.0;
    return q.next() ? q.value(0).toDouble() : 0.0;
}

// 用户已兑换积分总数
int spentPoints(QSqlDatabase& db, int userId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(points),0) FROM points_redemption WHERE user_id = ?;"));
    q.addBindValue(userId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

// 可兑换项目（兑换优惠券 / 抵扣充电费用）
struct RedeemItem {
    QString id;
    QString name;
    QString type;   // "coupon" 优惠券 / "deduct" 抵扣充电费用(转余额)
    int cost;       // 所需积分
    double value;   // 面值/抵扣金额（元）
};

const RedeemItem* findRedeemItem(const QString& id) {
    static const RedeemItem items[] = {
        {QStringLiteral("coupon_5"),  QStringLiteral("满10减5元优惠券"),  QStringLiteral("coupon"), 100, 5.0},
        {QStringLiteral("coupon_10"), QStringLiteral("满20减10元优惠券"), QStringLiteral("coupon"), 200, 10.0},
        {QStringLiteral("coupon_30"), QStringLiteral("满50减30元优惠券"), QStringLiteral("coupon"), 500, 30.0},
        {QStringLiteral("deduct_5"),  QStringLiteral("充电费抵扣 ¥5"),   QStringLiteral("deduct"), 100, 5.0},
        {QStringLiteral("deduct_20"), QStringLiteral("充电费抵扣 ¥20"),  QStringLiteral("deduct"), 400, 20.0},
    };
    for (const auto& it : items) {
        if (it.id == id) return &it;
    }
    return nullptr;
}

} // namespace

// 【需求1 - 手机号+密码登录】处理 USER_LOGIN：
// 1) 校验手机号格式与密码非空；
// 2) 按手机号查用户：已存在且被冻结 -> 拒绝；密码不匹配 -> 拒绝；
// 3) 未注册 -> 自动创建账号（首次登录自动注册，密码存哈希）；
// 4) 返回用户信息，客户端据此进入主页。
Api::Reply UserService::login(const QJsonObject& data) {
    const QString phone = data.value("phone").toString();
    const QString password = data.value("password").toString();
    if (!validPhone(phone)) return Api::err(Api::InvalidParam, QStringLiteral("手机号格式不正确"));
    if (password.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("请输入密码"));

    QSqlDatabase db = DbManager::threadDb();
    const QString hash = DbManager::hashPassword(password);

    // 查是否已注册，顺带取回冻结状态与密码哈希
    int userId = -1;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id, status, password FROM users WHERE phone = ?;"));
    q.addBindValue(phone);
    if (q.exec() && q.next()) {
        userId = q.value(0).toInt();
        if (q.value(1).toInt() == 0)   // status=0 表示冻结
            return Api::err(Api::StateConflict, QStringLiteral("账号已被冻结，请联系客服"));
        if (q.value(2).toString() != hash)
            return Api::err(Api::InvalidParam, QStringLiteral("密码错误"));
    }

    // 未注册 -> 自动创建账号（首次登录自动注册）
    if (userId < 0) {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT INTO users (phone, nickname, avatar_url, balance, password, status) "
            "VALUES (?,?,?,0,?,1);"));
        ins.addBindValue(phone);
        ins.addBindValue(QStringLiteral("用户%1").arg(phone.right(4)));
        ins.addBindValue(QString());
        ins.addBindValue(hash);
        if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());
        userId = ins.lastInsertId().toInt();
    }

    // 返回用户信息
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT nickname, avatar_url, balance FROM users WHERE user_id = ?;"));
    sel.addBindValue(userId);
    if (!sel.exec() || !sel.next()) return Api::err(Api::ServerError, QStringLiteral("查询用户失败"));

    QJsonObject out;
    out["user_id"] = userId;
    out["phone"] = phone;
    out["nickname"] = sel.value(0).toString();
    out["avatar"] = sel.value(1).toString();
    out["balance"] = sel.value(2).toDouble();
    return Api::okData(out);
}

Api::Reply UserService::updateProfile(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    const QString newNickname = data.value("nickname").toString();
    const QString newAvatar = data.value("avatar_url").toString();
    const bool hasNickname = data.contains("nickname") && !newNickname.trimmed().isEmpty();
    const bool hasAvatar = data.contains("avatar_url") && !newAvatar.isEmpty();

    if (!hasNickname && !hasAvatar) {
        return Api::err(Api::InvalidParam, QStringLiteral("没有需要修改的内容"));
    }
    if (hasNickname && newNickname.trimmed().length() > 32) {
        return Api::err(Api::InvalidParam, QStringLiteral("昵称长度不能超过 32 个字符"));
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);

    // 先读当前值
    q.prepare(QStringLiteral("SELECT nickname, avatar_url FROM users WHERE user_id = ?;"));
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) return Api::err(Api::NotFound, QStringLiteral("用户不存在"));
    QString nickname = hasNickname ? newNickname.trimmed() : q.value(0).toString();
    QString avatar = hasAvatar ? newAvatar : q.value(1).toString();

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral(
        "UPDATE users SET nickname = ?, avatar_url = ? WHERE user_id = ?;"));
    upd.addBindValue(nickname);
    upd.addBindValue(avatar);
    upd.addBindValue(userId);
    if (!upd.exec()) return Api::err(Api::ServerError, upd.lastError().text());

    QJsonObject out;
    out["nickname"] = nickname;
    out["avatar"] = avatar;
    return Api::okData(out);
}

Api::Reply UserService::recharge(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const double amount = data.value("amount").toDouble();
    if (userId <= 0 || amount <= 0) return Api::err(Api::InvalidParam, QStringLiteral("参数不合法"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE users SET balance = balance + ? WHERE user_id = ?;"));
    q.addBindValue(amount);
    q.addBindValue(userId);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());
    if (q.numRowsAffected() == 0) return Api::err(Api::NotFound, QStringLiteral("用户不存在"));

    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT balance FROM users WHERE user_id = ?;"));
    sel.addBindValue(userId);
    if (!sel.exec() || !sel.next()) return Api::err(Api::ServerError, QStringLiteral("查询余额失败"));

    QJsonObject out;
    out["balance"] = sel.value(0).toDouble();
    return Api::okData(out);
}

Api::Reply UserService::getBalance(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT balance FROM users WHERE user_id = ?;"));
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) return Api::err(Api::NotFound, QStringLiteral("用户不存在"));

    QJsonObject out;
    out["balance"] = q.value(0).toDouble();
    return Api::okData(out);
}

// 【碳积分与环保足迹】根据历史已完成订单实时计算：
//   减碳量 = 累计充电量 × carbon_factor
//   等效植树 = 减碳量 ÷ tree_factor
//   碳积分 = 累计充电量 × points_factor（再减去已兑换积分）
Api::Reply UserService::carbonStats(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    const double energy = userEnergy(db, userId);

    const double carbonFactor = configDouble(db, QStringLiteral("carbon_factor"), 0.785);
    const double treeFactor   = configDouble(db, QStringLiteral("tree_factor"), 18.0);
    const double pointsFactor = configDouble(db, QStringLiteral("points_factor"), 1.0);

    const double carbon = energy * carbonFactor;
    const double trees  = treeFactor > 0 ? carbon / treeFactor : 0.0;
    const int earned    = qRound(energy * pointsFactor);
    const int points    = earned - spentPoints(db, userId);

    QJsonObject out;
    out["energy_kwh"] = energy;
    out["carbon_kg"]  = carbon;
    out["trees"]      = trees;
    out["points"]     = points;
    out["level"]      = ecoLevel(points);
    return Api::okData(out);
}

// 【积分明细】合并"充电所得(+)"与"兑换支出(-)"两类记录
Api::Reply UserService::pointsDetail(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    const double pointsFactor = configDouble(db, QStringLiteral("points_factor"), 1.0);

    QJsonArray items;
    int earned = 0;

    // 充电所得
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT o.order_id, o.end_time,
               p.power_kw * (julianday(o.end_time) - julianday(o.start_time)) * 24.0 AS energy
        FROM orders o JOIN piles p ON p.pile_id = o.pile_id
        WHERE o.user_id = ? AND o.status = ?
        ORDER BY o.order_id DESC;)SQL"));
    q.addBindValue(userId);
    q.addBindValue(QString(Api::OrderStatus::kDone));
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());
    while (q.next()) {
        const int pts = qRound(q.value(2).toDouble() * pointsFactor);
        if (pts <= 0) continue;
        earned += pts;
        QJsonObject it;
        it["type"]   = QStringLiteral("充电");
        it["source"] = QStringLiteral("订单 #%1").arg(q.value(0).toInt());
        it["time"]   = fmtTime(q.value(1).toString());
        it["points"] = pts;
        items.append(it);
    }

    // 兑换支出
    int spent = 0;
    QSqlQuery r(db);
    r.prepare(QStringLiteral("SELECT points, item_name, created_at FROM points_redemption "
                             "WHERE user_id = ? ORDER BY redeem_id DESC;"));
    r.addBindValue(userId);
    if (!r.exec()) return Api::err(Api::ServerError, r.lastError().text());
    while (r.next()) {
        const int pts = r.value(0).toInt();
        spent += pts;
        QJsonObject it;
        it["type"]   = QStringLiteral("兑换");
        it["source"] = r.value(1).toString();
        it["time"]   = fmtTime(r.value(2).toString());
        it["points"] = -pts;
        items.append(it);
    }

    QJsonObject out;
    out["points"]       = earned - spent;
    out["total_earned"] = earned;
    out["total_spent"]  = spent;
    out["items"]        = items;
    return Api::okData(out);
}

// 【积分兑换】校验积分充足后扣减（写入兑换记录）；"抵扣充电费用"类型额外转入余额
Api::Reply UserService::redeemPoints(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const QString itemId = data.value("item_id").toString();
    if (userId <= 0 || itemId.isEmpty())
        return Api::err(Api::InvalidParam, QStringLiteral("参数不完整"));

    const RedeemItem* item = findRedeemItem(itemId);
    if (!item) return Api::err(Api::InvalidParam, QStringLiteral("兑换项目不存在"));

    QSqlDatabase db = DbManager::threadDb();

    QSqlQuery u(db);
    u.prepare(QStringLiteral("SELECT balance FROM users WHERE user_id = ?;"));
    u.addBindValue(userId);
    if (!u.exec() || !u.next()) return Api::err(Api::NotFound, QStringLiteral("用户不存在"));
    const double balance = u.value(0).toDouble();

    const double pointsFactor = configDouble(db, QStringLiteral("points_factor"), 1.0);
    const int earned = qRound(userEnergy(db, userId) * pointsFactor);
    const int current = earned - spentPoints(db, userId);
    if (current < item->cost) {
        return Api::err(Api::StateConflict,
                        QStringLiteral("碳积分不足（当前 %1 分，需要 %2 分）")
                            .arg(current).arg(item->cost));
    }

    const bool isDeduct = item->type == QStringLiteral("deduct");
    db.transaction();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO points_redemption (user_id, points, item_id, item_name, item_type, balance_credit) "
        "VALUES (?,?,?,?,?,?);"));
    ins.addBindValue(userId);
    ins.addBindValue(item->cost);
    ins.addBindValue(item->id);
    ins.addBindValue(item->name);
    ins.addBindValue(item->type);
    ins.addBindValue(isDeduct ? item->value : 0.0);
    if (!ins.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, ins.lastError().text());
    }
    const int redeemId = ins.lastInsertId().toInt();

    double newBalance = balance;
    if (isDeduct) {
        QSqlQuery b(db);
        b.prepare(QStringLiteral("UPDATE users SET balance = balance + ? WHERE user_id = ?;"));
        b.addBindValue(item->value);
        b.addBindValue(userId);
        b.exec();
        newBalance = balance + item->value;
    }

    db.commit();

    QJsonObject out;
    out["points"]    = current - item->cost;
    out["redeem_id"] = redeemId;
    out["item_name"] = item->name;
    out["balance"]   = newBalance;
    return Api::okData(out);
}
