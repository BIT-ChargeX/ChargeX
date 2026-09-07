#include "UserService.h"
#include "DbManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace {

bool validPhone(const QString& p) {
    static const QRegularExpression re(QStringLiteral("^1[3-9][0-9]{9}$"));
    return re.match(p).hasMatch();
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

} // namespace

// 【需求1 - 手机号+密码登录】处理 USER_LOGIN：
// 1) 校验手机号格式与密码非空；
// 2) 按手机号查用户：不存在 -> 提示先注册；冻结 -> 拒绝；密码不匹配 -> 拒绝；
// 3) 校验通过 -> 返回用户信息，客户端据此进入主页。
Api::Reply UserService::login(const QJsonObject& data) {
    const QString phone = data.value("phone").toString();
    const QString password = data.value("password").toString();
    if (!validPhone(phone)) return Api::err(Api::InvalidParam, QStringLiteral("手机号格式不正确"));
    if (password.isEmpty()) return Api::err(Api::InvalidParam, QStringLiteral("请输入密码"));

    QSqlDatabase db = DbManager::threadDb();
    const QString hash = DbManager::hashPassword(password);

    int userId = -1;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id, status, password FROM users WHERE phone = ?;"));
    q.addBindValue(phone);
    if (!q.exec() || !q.next())
        return Api::err(Api::NotFound, QStringLiteral("该手机号尚未注册，请先注册"));

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
    out["phone"] = phone;
    out["nickname"] = sel.value(0).toString();
    out["avatar"] = sel.value(1).toString();
    out["balance"] = sel.value(2).toDouble();
    return Api::okData(out);
}

// 【需求1 - 注册新账号】处理 USER_REGISTER：
// 1) 校验手机号格式与密码强度；
// 2) 手机号已存在 -> 拒绝；
// 3) 通过 -> 创建账号（密码存哈希）。
Api::Reply UserService::registerUser(const QJsonObject& data) {
    const QString phone = data.value("phone").toString();
    const QString password = data.value("password").toString();
    if (!validPhone(phone)) return Api::err(Api::InvalidParam, QStringLiteral("手机号格式不正确"));
    if (!validPassword(password))
        return Api::err(Api::InvalidParam, QStringLiteral("密码至少8位，且需包含大写字母、小写字母和数字"));

    QSqlDatabase db = DbManager::threadDb();

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id FROM users WHERE phone = ?;"));
    q.addBindValue(phone);
    if (q.exec() && q.next())
        return Api::err(Api::StateConflict, QStringLiteral("该手机号已注册，请直接登录"));

    const QString hash = DbManager::hashPassword(password);
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO users (phone, nickname, avatar_url, balance, password, status) "
        "VALUES (?,?,?,0,?,1);"));
    ins.addBindValue(phone);
    ins.addBindValue(QStringLiteral("用户%1").arg(phone.right(4)));
    ins.addBindValue(QString());
    ins.addBindValue(hash);
    if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());

    QJsonObject out;
    out["user_id"] = ins.lastInsertId().toInt();
    out["phone"] = phone;
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
