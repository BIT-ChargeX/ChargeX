#include "AdminService.h"
#include "DbManager.h"
#include "SessionManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>

Api::Reply AdminService::login(const QJsonObject& data) {
    const QString account = data.value("account").toString().trimmed();
    const QString password = data.value("password").toString();
    if (account.isEmpty() || password.isEmpty()) {
        return Api::err(Api::InvalidParam, QStringLiteral("账号或密码为空"));
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT admin_id, name FROM admins WHERE account = ? AND password = ?;"));
    q.addBindValue(account);
    q.addBindValue(password);
    if (!q.exec() || !q.next()) {
        return Api::err(Api::NotFound, QStringLiteral("账号或密码错误"));
    }

    const int adminId = q.value(0).toInt();
    const QString name = q.value(1).toString();

    QJsonObject out;
    out["admin_id"] = adminId;
    out["name"] = name;
    out["token"] = SessionManager::instance().createSession(adminId, name);
    return Api::okData(out);
}

Api::Reply AdminService::logout(const QJsonObject& data) {
    const QString token = data.value("token").toString();
    SessionManager::instance().removeSession(token);
    return Api::ok();
}

Api::Reply AdminService::userList(const QJsonObject& data) {
    const int page = qMax(1, data.value("page").toInt());
    const int pageSize = 20;
    const QString keyword = data.value("phone_keyword").toString().trimmed();

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    QString sql = QStringLiteral(
        "SELECT user_id, phone, nickname, balance, reg_time, status "
        "FROM users ");
    if (!keyword.isEmpty()) {
        sql += QStringLiteral("WHERE phone LIKE ? ");
    }
    sql += QStringLiteral("ORDER BY user_id LIMIT %1 OFFSET %2;")
               .arg(pageSize).arg((page - 1) * pageSize);
    q.prepare(sql);
    if (!keyword.isEmpty()) q.addBindValue(QStringLiteral("%%1%").arg(keyword));
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject u;
        u["user_id"] = q.value(0).toInt();
        u["phone"] = q.value(1).toString();
        u["nickname"] = q.value(2).toString();
        u["balance"] = q.value(3).toDouble();
        u["reg_time"] = q.value(4).toString();
        u["status"] = q.value(5).toInt();
        arr.append(u);
    }

    QJsonObject out;
    out["users"] = arr;
    return Api::okData(out);
}

Api::Reply AdminService::freezeUser(const QJsonObject& data) {
    const int userId = data.value("user_id").toInt();
    const QString action = data.value("action").toString();
    if (userId <= 0 || (action != QStringLiteral("freeze") && action != QStringLiteral("unfreeze"))) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不合法"));
    }
    const int newStatus = action == QStringLiteral("freeze") ? 0 : 1;

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE users SET status = ? WHERE user_id = ?;"));
    q.addBindValue(newStatus);
    q.addBindValue(userId);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());
    if (q.numRowsAffected() == 0) return Api::err(Api::NotFound, QStringLiteral("用户不存在"));

    QJsonObject out;
    out["status"] = newStatus;
    return Api::okData(out);
}
