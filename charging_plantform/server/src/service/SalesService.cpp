#include "SalesService.h"
#include "DbManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDate>

namespace {

// 已完成订单，且 end_time 落在 [from, to]（ISO 字符串比较）
double sumBetween(const QString& from, const QString& to) {
    QSqlQuery q(DbManager::threadDb());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(amount),0) FROM orders "
        "WHERE status='已完成' AND substr(end_time,1,19) >= ? AND substr(end_time,1,19) <= ?;"));
    q.addBindValue(from);
    q.addBindValue(to);
    if (q.exec() && q.next()) return q.value(0).toDouble();
    return 0.0;
}

double sumDay(const QDate& day) {
    QSqlQuery q(DbManager::threadDb());
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(amount),0) FROM orders "
        "WHERE status='已完成' AND substr(end_time,1,10)=?;"));
    q.addBindValue(day.toString(Qt::ISODate));
    if (q.exec() && q.next()) return q.value(0).toDouble();
    return 0.0;
}

}

Api::Reply SalesService::summary(const QJsonObject& data) {
    const QDate today = QDate::currentDate();
    const QString dayEnd = today.toString(Qt::ISODate) + QStringLiteral("T23:59:59");

    double monthTotal = 0.0;
    {
        const QDate monthStart(today.year(), today.month(), 1);
        monthTotal = sumBetween(monthStart.toString(Qt::ISODate)
                                    + QStringLiteral("T00:00:00"), dayEnd);
    }

    QJsonObject out;
    out["today"] = sumBetween(today.toString(Qt::ISODate) + QStringLiteral("T00:00:00"), dayEnd);
    out["month"] = monthTotal;
    out["total"] = sumBetween(QStringLiteral("2000-01-01T00:00:00"), dayEnd);

    int days = data.value("days").toInt();
    if (days != 7 && days != 30) days = 7;

    QJsonArray daily;
    for (int i = days - 1; i >= 0; --i) {
        const QDate day = today.addDays(-i);
        QJsonObject d;
        d["date"] = day.toString(QStringLiteral("MM-dd"));
        d["amount"] = sumDay(day);
        daily.append(d);
    }
    out["days"] = days;
    out["daily"] = daily;
    return Api::okData(out);
}
