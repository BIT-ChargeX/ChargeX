#include "UserService.h"
#include "DbManager.h"
#include "AliyunSms.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QRandomGenerator>
#include <QDebug>

namespace {

bool validPhone(const QString& p) {
    static const QRegularExpression re(QStringLiteral("^1[0-9]{10}$"));
    return re.match(p).hasMatch();
}

// 按手机号查询用户；不存在返回 -1
int findUserIdByPhone(QSqlDatabase& db, const QString& phone, bool* frozen = nullptr) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id, status FROM users WHERE phone = ?;"));
    q.addBindValue(phone);
    if (q.exec() && q.next()) {
        if (frozen) *frozen = (q.value(1).toInt() == 0);
        return q.value(0).toInt();
    }
    return -1;
}

} // namespace

namespace {

// 验证码存储：内存保存，5 分钟有效 + 一次性使用（服务重启即失效）。
struct SmsCode {
    QString code;
    QDateTime expiresAt;
};
QHash<QString, SmsCode> g_smsCodes;   // phone -> 验证码
QMutex g_smsCodesMutex;

QString generateSmsCode(const QString& phone) {
    QString code;
    auto* rnd = QRandomGenerator::global();
    for (int i = 0; i < 6; ++i) code += QString::number(rnd->bounded(10));
    QMutexLocker lock(&g_smsCodesMutex);
    g_smsCodes[phone] = SmsCode{code, QDateTime::currentDateTime().addSecs(5 * 60)};
    return code;
}

// 校验并消耗验证码（一次性）：成功移除；不存在/过期/不匹配返回 false。
bool consumeSmsCode(const QString& phone, const QString& code) {
    QMutexLocker lock(&g_smsCodesMutex);
    auto it = g_smsCodes.constFind(phone);
    if (it == g_smsCodes.constEnd()) return false;
    if (QDateTime::currentDateTime() > it.value().expiresAt) {
        g_smsCodes.remove(phone);
        return false;
    }
    if (it.value().code != code) return false;
    g_smsCodes.remove(phone);
    return true;
}

} // namespace

Api::Reply UserService::sendCode(const QJsonObject& data) {
    const QString phone = data.value("phone").toString();
    if (!validPhone(phone)) return Api::err(Api::InvalidParam, QStringLiteral("手机号格式不正确"));

    const QString code = generateSmsCode(phone);
    QString err;
    if (!AliyunSms::send(phone, code, &err)) {
        // 开发模式兜底：阿里云短信凭证未配置（AliyunSms.h 为空）时，
        // 验证码随响应下发（客户端自动填充）并在服务端日志打印，
        // 保证演示环境无真实短信通道也能走通登录；配置真实凭证后走正常短信分支。
        if (QString::fromLatin1(AliyunSms::kAccessKeyId).trimmed().isEmpty()) {
            QJsonObject out;
            out["dev_code"] = code;
            qInfo().noquote() << QStringLiteral("[演示模式] 未配置阿里云短信凭证，验证码：%1").arg(code);
            return Api::okData(out);
        }
        // 发送失败不落库：用户收不到验证码，也就无法用该验证码登录
        QMutexLocker lock(&g_smsCodesMutex);
        g_smsCodes.remove(phone);
        return Api::err(Api::ServerError, QStringLiteral("短信发送失败：%1").arg(err));
    }

    qInfo().noquote() << QStringLiteral("[短信] 已向 %1 发送登录验证码").arg(phone);
    return Api::ok();
}

Api::Reply UserService::login(const QJsonObject& data) {
    const QString phone = data.value("phone").toString();
    const QString code = data.value("code").toString();
    if (!validPhone(phone)) return Api::err(Api::InvalidParam, QStringLiteral("手机号格式不正确"));
    if (!consumeSmsCode(phone, code))
        return Api::err(Api::InvalidParam, QStringLiteral("验证码错误或已过期"));

    QSqlDatabase db = DbManager::threadDb();
    bool frozen = false;
    int userId = findUserIdByPhone(db, phone, &frozen);
    if (userId >= 0 && frozen) {
        return Api::err(Api::StateConflict, QStringLiteral("账号已被冻结，请联系客服"));
    }

    if (userId < 0) {
        // 首次登录自动注册：默认昵称"用户+手机号后4位"
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT INTO users (phone, nickname, avatar_url, balance, status) VALUES (?,?,?,0,1);"));
        ins.addBindValue(phone);
        ins.addBindValue(QStringLiteral("用户%1").arg(phone.right(4)));
        ins.addBindValue(QString());
        if (!ins.exec()) return Api::err(Api::ServerError, ins.lastError().text());
        userId = ins.lastInsertId().toInt();
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT nickname, avatar_url, balance FROM users WHERE user_id = ?;"));
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) return Api::err(Api::ServerError, QStringLiteral("查询用户失败"));

    QJsonObject out;
    out["user_id"] = userId;
    out["phone"] = phone;
    out["nickname"] = q.value(0).toString();
    out["avatar"] = q.value(1).toString();
    out["balance"] = q.value(2).toDouble();
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
