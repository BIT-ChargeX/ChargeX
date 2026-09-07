#include "SalesService.h"
#include "DbManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDate>
#include <QHash>
#include <cmath>
#include <utility>

namespace {

// 已完成订单，且 end_time 落在 [from, to]（ISO 字符串比较，含时分秒）
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
    return sumBetween(day.toString(Qt::ISODate) + QStringLiteral("T00:00:00"),
                      day.toString(Qt::ISODate) + QStringLiteral("T23:59:59"));
}

// 按日聚合窗口内营收：date(yyyy-MM-dd) -> amount（单条 GROUP BY，避免逐日 N 次查询）
QHash<QString, double> sumByDay(const QDate& from, const QDate& to) {
    QHash<QString, double> map;
    QSqlQuery q(DbManager::threadDb());
    q.prepare(QStringLiteral(
        "SELECT substr(end_time,1,10), COALESCE(SUM(amount),0) FROM orders "
        "WHERE status='已完成' AND substr(end_time,1,10) >= ? AND substr(end_time,1,10) <= ? "
        "GROUP BY substr(end_time,1,10);"));
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    if (q.exec()) {
        while (q.next()) map.insert(q.value(0).toString(), q.value(1).toDouble());
    }
    return map;
}

// 环比百分比：无基线(<=0)返回 -1（前端显示“-”）
double pctChange(double current, double base) {
    if (base <= 0.0) return -1.0;
    return std::round((current - base) / base * 1000.0) / 10.0;
}

} // namespace

Api::Reply SalesService::summary(const QJsonObject& data) {
    const QDate today = QDate::currentDate();

    // ---- 确定统计窗口：优先显式 start/end，否则用 days(7/30) ----
    const QString startText = data.value("start").toString().trimmed();
    const QString endText = data.value("end").toString().trimmed();
    const bool hasRange = !startText.isEmpty() && !endText.isEmpty();

    QDate windowStart;
    QDate windowEnd;
    if (hasRange) {
        windowStart = QDate::fromString(startText, Qt::ISODate);
        windowEnd = QDate::fromString(endText, Qt::ISODate);
        if (!windowStart.isValid() || !windowEnd.isValid())
            return Api::err(Api::InvalidParam, QStringLiteral("日期格式应为 yyyy-MM-dd"));
        if (windowStart > windowEnd) std::swap(windowStart, windowEnd);
        if (windowStart.daysTo(windowEnd) + 1 > 366)
            return Api::err(Api::InvalidParam, QStringLiteral("统计区间过长（最多 366 天）"));
    } else {
        int days = data.value("days").toInt();
        if (days != 7 && days != 30) days = 7;
        windowEnd = today;
        windowStart = today.addDays(-(days - 1));
    }

    // ---- 按日营收序列（补零） ----
    const QHash<QString, double> dayMap = sumByDay(windowStart, windowEnd);
    QJsonArray daily;
    for (QDate day = windowStart; day <= windowEnd; day = day.addDays(1)) {
        const QString iso = day.toString(Qt::ISODate);
        const double amount = dayMap.value(iso, 0.0);
        QJsonObject d;
        d["date"] = day.toString(QStringLiteral("MM-dd"));
        d["date_full"] = iso;
        d["amount"] = amount;
        daily.append(d);
    }

    // ---- 顶部统计卡（口径不变：今日 / 本月 / 累计）+ 环比 ----
    const QDate monthStart(today.year(), today.month(), 1);
    const QString dayEnd = today.toString(Qt::ISODate) + QStringLiteral("T23:59:59");

    const double todayAmt = sumDay(today);
    const double monthAmt = sumBetween(monthStart.toString(Qt::ISODate)
                                           + QStringLiteral("T00:00:00"),
                                       dayEnd);
    const double totalAmt = sumBetween(QStringLiteral("2000-01-01T00:00:00"), dayEnd);

    const double yesterdayAmt = sumDay(today.addDays(-1));
    const QDate prevMonth(today.addMonths(-1).year(), today.addMonths(-1).month(), 1);
    const QDate prevMonthLast = prevMonth.addMonths(1).addDays(-1);
    const QDate prevMonthCmp(today.year(), today.month(),
                             qMin(today.day(), prevMonthLast.day()));
    const double prevMonthAmt = sumBetween(prevMonth.toString(Qt::ISODate)
                                               + QStringLiteral("T00:00:00"),
                                           prevMonthCmp.toString(Qt::ISODate)
                                               + QStringLiteral("T23:59:59"));

    // ---- 站点营收 Top5（窗口内） ----
    QJsonArray topStations;
    {
        QSqlQuery q(DbManager::threadDb());
        q.prepare(QStringLiteral(
            "SELECT COALESCE(s.station_id, 0), COALESCE(s.name, '-'), "
            "       COALESCE(SUM(o.amount), 0) "
            "FROM orders o "
            "LEFT JOIN piles p ON p.pile_id = o.pile_id "
            "LEFT JOIN stations s ON s.station_id = p.station_id "
            "WHERE o.status='已完成' AND substr(o.end_time,1,10) >= ? "
            "  AND substr(o.end_time,1,10) <= ? "
            "GROUP BY s.station_id ORDER BY SUM(o.amount) DESC LIMIT 5;"));
        q.addBindValue(windowStart.toString(Qt::ISODate));
        q.addBindValue(windowEnd.toString(Qt::ISODate));
        if (q.exec()) {
            while (q.next()) {
                QJsonObject st;
                st["station_id"] = q.value(0).toInt();
                st["name"] = q.value(1).toString();
                st["amount"] = q.value(2).toDouble();
                topStations.append(st);
            }
        }
    }

    QJsonObject out;
    out["today"] = todayAmt;
    out["month"] = monthAmt;
    out["total"] = totalAmt;
    out["today_pct"] = pctChange(todayAmt, yesterdayAmt);
    out["month_pct"] = pctChange(monthAmt, prevMonthAmt);
    out["start"] = windowStart.toString(Qt::ISODate);
    out["end"] = windowEnd.toString(Qt::ISODate);
    out["days"] = windowStart.daysTo(windowEnd) + 1;
    out["daily"] = daily;
    out["top_stations"] = topStations;
    return Api::okData(out);
}
