#include "UserService.h"
#include "DbManager.h"
#include "MinioClient.h"
#include "SmtpClient.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>
#include <cmath>

namespace {

bool validEmail(const QString& e) {
    static const QRegularExpression re(QStringLiteral(
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
    return re.match(e).hasMatch();
}

// 密码至少8位，且同时包含大写字母、小写字母和数字
bool validPassword(const QString& p) {
    if (p.length() < 8) return false;
    bool hasUpper = false, hasLower = false, hasDigit = false;
    for (QChar c : p) {
        if (c.isUpper()) hasUpper = true;
        else if (c.isLower()) hasLower = true;
        else if (c.isDigit()) hasDigit = true;
    }
    return hasUpper && hasLower && hasDigit;
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

// 时间格式化：数据库存的 created_at 是 UTC（SQLite CURRENT_TIMESTAMP），
// 统一转换为北京时间（UTC+8）后再展示。
QString fmtTime(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) return iso;
    dt.setTimeSpec(Qt::UTC);                      // 字段本义是 UTC
    dt = dt.toOffsetFromUtc(8 * 3600);            // 转北京时间
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// 本地时间格式化：用于本身就按服务器本地时间写入的字段，仅重排格式、不换算时区。
QString fmtTimeLocal(const QString& iso) {
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : iso;
}

// 用户累计充电量(kWh)：已完成订单在结算时写入的 energy_kwh 之和
double userEnergy(QSqlDatabase& db, int userId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(o.energy_kwh), 0.0) "
        "FROM orders o WHERE o.user_id = ? AND o.status = ?;"));
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

// 校验邮箱验证码：返回最新一条未使用、未过期、purpose/哈希匹配的记录 id，失败返回 0
int verifyCode(QSqlDatabase& db, const QString& email, const QString& purpose,
               const QString& code) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, code_hash FROM email_verify_code "
        "WHERE email = ? AND purpose = ? AND used = 0 AND expires_at > datetime('now') "
        "ORDER BY id DESC LIMIT 1;"));
    q.addBindValue(email);
    q.addBindValue(purpose);
    if (!q.exec() || !q.next()) return 0;
    if (q.value(1).toString() != DbManager::hashPassword(code)) return 0;
    return q.value(0).toInt();
}

// 标记验证码已使用
void markCodeUsed(QSqlDatabase& db, int verifyId) {
    QSqlQuery m(db);
    m.prepare(QStringLiteral("UPDATE email_verify_code SET used = 1 WHERE id = ?;"));
    m.addBindValue(verifyId);
    m.exec();
}

} // namespace

// 【需求1 - 邮箱+密码登录】处理 USER_LOGIN：
// 1) 校验邮箱格式与密码非空；
// 2) 按邮箱查用户：不存在 -> 提示先注册；冻结 -> 拒绝；密码不匹配 -> 拒绝；
// 3) 校验通过 -> 返回用户信息，客户端据此进入主页。
Api::Reply UserService::login(const QJsonObject& data) {
    const QString email = data.value("email").toString().trimmed();
    const QString password = data.value("password").toString();
    if (!validEmail(email)) return Api::err(Api::InvalidParam, QStringLiteral("邮箱格式不正确"));
    if (password.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("请输入密码"));

    QSqlDatabase db = DbManager::threadDb();
    const QString hash = DbManager::hashPassword(password);

    int userId = -1;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id, status, password FROM users WHERE email = ?;"));
    q.addBindValue(email);
    if (!q.exec() || !q.next())
        return Api::err(Api::NotFound, QStringLiteral("该邮箱尚未注册，请先注册"));

    userId = q.value(0).toInt();
    if (q.value(1).toInt() == 0)   // status=0 表示冻结
        return Api::err(Api::StateConflict, QStringLiteral("账号已被冻结，请联系客服"));
    if (q.value(2).toString() != hash)
        return Api::err(Api::InvalidParam, QStringLiteral("密码错误"));

    // 返回用户信息
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT nickname, avatar_url, balance FROM users WHERE user_id = ?;"));
    sel.addBindValue(userId);
    if (!sel.exec() || !sel.next()) return Api::err(Api::ServerError, QStringLiteral("查询用户失败"));

    QJsonObject out;
    out["user_id"] = userId;
    out["email"] = email;
    out["nickname"] = sel.value(0).toString();
    out["avatar"] = sel.value(1).toString();
    out["balance"] = sel.value(2).toDouble();
    return Api::okData(out);
}

// 【需求1 - 注册新账号】处理 USER_REGISTER：
// 1) 校验邮箱格式、密码强度与邮箱验证码；
// 2) 邮箱已存在 -> 拒绝；
// 3) 通过 -> 创建账号（密码存哈希）并作废验证码。
Api::Reply UserService::registerUser(const QJsonObject& data) {
    const QString email = data.value("email").toString().trimmed();
    const QString password = data.value("password").toString();
    const QString code = data.value("code").toString().trimmed();
    if (!validEmail(email)) return Api::err(Api::InvalidParam, QStringLiteral("邮箱格式不正确"));
    if (!validPassword(password))
        return Api::err(Api::InvalidParam, QStringLiteral("密码至少8位，且需包含大写字母、小写字母和数字"));
    if (code.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("请输入邮箱验证码"));

    QSqlDatabase db = DbManager::threadDb();

    const int verifyId = verifyCode(db, email, QStringLiteral("register"), code);
    if (verifyId == 0) return Api::err(Api::InvalidParam, QStringLiteral("验证码错误或已过期"));

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id FROM users WHERE email = ?;"));
    q.addBindValue(email);
    if (q.exec() && q.next())
        return Api::err(Api::StateConflict, QStringLiteral("该邮箱已注册，请直接登录"));

    const QString hash = DbManager::hashPassword(password);
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO users (email, nickname, avatar_url, balance, password, status) "
        "VALUES (?,?,?,0,?,1);"));
    ins.addBindValue(email);
    ins.addBindValue(QStringLiteral("用户%1").arg(email.left(email.indexOf('@'))));
    ins.addBindValue(QString());
    ins.addBindValue(hash);
    if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());

    markCodeUsed(db, verifyId);

    QJsonObject out;
    out["user_id"] = ins.lastInsertId().toInt();
    out["email"] = email;
    return Api::okData(out);
}

// 【需求1 - 发送邮箱验证码】purpose: register(注册，邮箱须未注册) / reset(找回密码，邮箱须已注册)
// 1) 校验邮箱格式与用途；2) 按用途校验邮箱是否已注册；3) 限流（同一邮箱+用途 1 分钟一次）；
// 4) 生成 6 位随机码 -> 哈希存表（5 分钟有效）-> SMTP 发邮件；未配置 SMTP 时降级演示模式（打印日志）。
Api::Reply UserService::sendCode(const QJsonObject& data) {
    const QString email = data.value("email").toString().trimmed();
    QString purpose = data.value("purpose").toString();
    if (purpose.isEmpty()) purpose = QStringLiteral("reset");
    if (!validEmail(email)) return Api::err(Api::InvalidParam, QStringLiteral("邮箱格式不正确"));
    if (purpose != QStringLiteral("register") && purpose != QStringLiteral("reset"))
        return Api::err(Api::InvalidParam, QStringLiteral("参数不合法"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery chk(db);
    chk.prepare(QStringLiteral("SELECT user_id FROM users WHERE email = ?;"));
    chk.addBindValue(email);
    const bool exists = chk.exec() && chk.next();
    if (purpose == QStringLiteral("register") && exists)
        return Api::err(Api::StateConflict, QStringLiteral("该邮箱已注册，请直接登录"));
    if (purpose == QStringLiteral("reset") && !exists)
        return Api::err(Api::NotFound, QStringLiteral("该邮箱尚未注册"));

    QSqlQuery rate(db);
    rate.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM email_verify_code "
        "WHERE email = ? AND purpose = ? AND created_at > datetime('now','-1 minute');"));
    rate.addBindValue(email);
    rate.addBindValue(purpose);
    if (rate.exec() && rate.next() && rate.value(0).toInt() > 0)
        return Api::err(Api::StateConflict, QStringLiteral("发送过于频繁，请稍后再试"));

    const QString code = QString::number(QRandomGenerator::global()->bounded(100000, 1000000));
    const QString codeHash = DbManager::hashPassword(code);

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO email_verify_code (email, purpose, code_hash, expires_at) "
        "VALUES (?, ?, ?, datetime('now','+5 minutes'));"));
    ins.addBindValue(email);
    ins.addBindValue(purpose);
    ins.addBindValue(codeHash);
    if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());

    // 演示模式兜底：验证码始终打印到服务端日志
    qInfo().noquote() << "[VerifyCode]" << purpose << email << "验证码:" << code;

    const QString subject = QStringLiteral("【东软充电】邮箱验证码");
    const QString body = QStringLiteral("你的验证码是 %1，5 分钟内有效。").arg(code);
    if (SmtpClient::isConfigured() && !SmtpClient::sendPlainText(email, subject, body)) {
        return Api::err(Api::ServerError, QStringLiteral("验证码邮件发送失败，请检查 SMTP 配置"));
    }

    QJsonObject out;
    out["email"] = email;
    out["demo"] = !SmtpClient::isConfigured();
    return Api::okData(out);
}

// 【需求1 - 忘记密码】校验验证码并重置密码
Api::Reply UserService::resetPassword(const QJsonObject& data) {
    const QString email = data.value("email").toString().trimmed();
    const QString code = data.value("code").toString().trimmed();
    const QString newPassword = data.value("new_password").toString();
    if (!validEmail(email)) return Api::err(Api::InvalidParam, QStringLiteral("邮箱格式不正确"));
    if (code.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("请输入验证码"));
    if (!validPassword(newPassword))
        return Api::err(Api::InvalidParam, QStringLiteral("密码至少8位，且需包含大写字母、小写字母和数字"));

    QSqlDatabase db = DbManager::threadDb();

    const int verifyId = verifyCode(db, email, QStringLiteral("reset"), code);
    if (verifyId == 0) return Api::err(Api::InvalidParam, QStringLiteral("验证码错误或已过期"));

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE users SET password = ? WHERE email = ?;"));
    upd.addBindValue(DbManager::hashPassword(newPassword));
    upd.addBindValue(email);
    if (!upd.exec() || upd.numRowsAffected() == 0)
        return Api::err(Api::NotFound, QStringLiteral("用户不存在"));

    markCodeUsed(db, verifyId);
    return Api::ok();
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

// 【需求6 - 头像上传】客户端 base64 图片 → 上传 MinIO 对象存储 → 回传公开 URL 并写入 users.avatar_url。
// 相比存本地路径，URL 跨设备可访问（MinIO 公共桶直连）。
Api::Reply UserService::uploadAvatar(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const QString b64 = data.value("data_b64").toString();
    const QString fileName = data.value("file_name").toString();
    if (userId <= 0 || b64.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("参数不合法"));

    const QByteArray bytes = QByteArray::fromBase64(b64.toLatin1());
    if (bytes.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("图片数据为空"));

    QString ext = QStringLiteral("png");
    QString contentType = QStringLiteral("image/png");
    if (fileName.endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive)
        || fileName.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
        ext = QStringLiteral("jpg"); contentType = QStringLiteral("image/jpeg");
    } else if (fileName.endsWith(QStringLiteral(".bmp"), Qt::CaseInsensitive)) {
        ext = QStringLiteral("bmp"); contentType = QStringLiteral("image/bmp");
    } else if (fileName.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive)) {
        ext = QStringLiteral("gif"); contentType = QStringLiteral("image/gif");
    }

    const QString objectKey = QStringLiteral("avatar/%1_%2.%3")
        .arg(userId).arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);

    QString url;
    if (!MinioClient::upload(objectKey, bytes, contentType, &url)) {
        return Api::err(Api::ServerError, QStringLiteral("头像上传失败"));
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE users SET avatar_url = ? WHERE user_id = ?;"));
    upd.addBindValue(url);
    upd.addBindValue(userId);
    if (!upd.exec() || upd.numRowsAffected() == 0) {
        return Api::err(Api::NotFound, QStringLiteral("用户不存在"));
    }

    QJsonObject out;
    out["avatar"] = url;
    return Api::okData(out);
}

// 【需求7 - 余额充值】校验金额合法性（>0、有限、不超过单笔限额）→ 模拟支付成功 →
// 余额累加 + 充值记录写入同一事务，保证账户数据一致性。
Api::Reply UserService::recharge(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const double amount = data.value("amount").toDouble();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));
    if (!std::isfinite(amount) || amount <= 0) {
        return Api::err(Api::InvalidParam, QStringLiteral("充值金额不合法"));
    }

    QSqlDatabase db = DbManager::threadDb();

    // 单笔限额（可在系统配置表中调整）
    const double limit = configDouble(db, QStringLiteral("recharge_limit"), 5000.0);
    if (amount > limit) {
        return Api::err(Api::InvalidParam,
                        QStringLiteral("超过单笔限额 ¥%1").arg(limit, 0, 'f', 2));
    }

    // 余额更新 + 充值记录写入同一事务
    db.transaction();

    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE users SET balance = balance + ? WHERE user_id = ?;"));
    upd.addBindValue(amount);
    upd.addBindValue(userId);
    if (!upd.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, upd.lastError().text());
    }
    if (upd.numRowsAffected() == 0) {
        db.rollback();
        return Api::err(Api::NotFound, QStringLiteral("用户不存在"));
    }

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO recharge_record (user_id, amount, pay_method) VALUES (?,?,?);"));
    ins.addBindValue(userId);
    ins.addBindValue(amount);
    ins.addBindValue(QStringLiteral("模拟支付"));
    if (!ins.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, ins.lastError().text());
    }
    const int rechargeId = ins.lastInsertId().toInt();

    db.commit();

    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT balance FROM users WHERE user_id = ?;"));
    sel.addBindValue(userId);
    if (!sel.exec() || !sel.next()) return Api::err(Api::ServerError, QStringLiteral("查询余额失败"));

    QJsonObject out;
    out["balance"] = sel.value(0).toDouble();
    out["recharge_id"] = rechargeId;
    return Api::okData(out);
}

// 【充值记录查询】返回该用户最近的充值记录（供用户后续查询）
Api::Reply UserService::rechargeRecords(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    if (userId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 user_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT recharge_id, amount, pay_method, created_at FROM recharge_record "
        "WHERE user_id = ? ORDER BY recharge_id DESC LIMIT 100;"));
    q.addBindValue(userId);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray items;
    while (q.next()) {
        QJsonObject it;
        it["recharge_id"] = q.value(0).toInt();
        it["amount"]      = q.value(1).toDouble();
        it["pay_method"]  = q.value(2).toString();
        it["time"]        = fmtTime(q.value(3).toString());
        items.append(it);
    }

    QJsonObject out;
    out["items"] = items;
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

    // 充电所得（每笔已完成订单一条记录，电量取结算时写入的 energy_kwh）
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        SELECT o.order_id, o.end_time, o.energy_kwh AS energy
        FROM orders o
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
        it["time"]   = fmtTimeLocal(q.value(1).toString());   // end_time 为本地时间
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
