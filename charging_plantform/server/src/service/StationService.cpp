#include "StationService.h"
#include "DbManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <QVector>

namespace {

void recalcStation(QSqlDatabase& db, int stationId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"SQL(
        UPDATE stations SET
            pile_total = (SELECT COUNT(*) FROM piles WHERE piles.station_id = ?),
            pile_free  = (SELECT COUNT(*) FROM piles WHERE piles.station_id = ?
                          AND status = '闲置')
        WHERE station_id = ?;)SQL"));
    q.addBindValue(stationId);
    q.addBindValue(stationId);
    q.addBindValue(stationId);
    q.exec();
}

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kPi = 3.14159265358979323846;

double toRad(double deg) { return deg * kPi / 180.0; }

double distanceKm(double lat1, double lng1, double lat2, double lng2) {
    const double dLat = toRad(lat2 - lat1);
    const double dLng = toRad(lng2 - lng1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(toRad(lat1)) * std::cos(toRad(lat2))
                     * std::sin(dLng / 2) * std::sin(dLng / 2);
    return kEarthRadiusKm * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

} // namespace

Api::Reply StationService::nearby(const QJsonObject& data) {
    const double lat = data.value("lat").toDouble();
    const double lng = data.value("lng").toDouble();
    if (qFuzzyIsNull(lat) && qFuzzyIsNull(lng)) {
        return Api::err(Api::InvalidParam, QStringLiteral("缺少有效坐标"));
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT station_id, name, address, lat, lng, price, pile_total, pile_free "
        "FROM stations;"));
    if (!q.isActive()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        const double sLat = q.value(3).toDouble();
        const double sLng = q.value(4).toDouble();
        QJsonObject s;
        s["station_id"] = q.value(0).toInt();
        s["name"] = q.value(1).toString();
        s["address"] = q.value(2).toString();
        s["price"] = q.value(5).toDouble();
        s["pile_total"] = q.value(6).toInt();
        s["pile_free"] = q.value(7).toInt();
        s["distance"] = distanceKm(lat, lng, sLat, sLng);
        arr.append(s);
    }

    // 排序为服务端业务（按距离由近及远），客户端无需再排
    {
        QVector<QJsonObject> tmp;
        tmp.reserve(arr.size());
        for (const auto& v : arr) tmp.append(v.toObject());
        std::sort(tmp.begin(), tmp.end(), [](const QJsonObject& a, const QJsonObject& b) {
            return a.value("distance").toDouble() < b.value("distance").toDouble();
        });
        arr = QJsonArray();
        for (const auto& s : tmp) arr.append(s);
    }

    QJsonObject out;
    out["stations"] = arr;
    return Api::okData(out);
}

Api::Reply StationService::detail(const QJsonObject& data) {
    const int stationId = data.value("station_id").toInt();
    if (stationId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 station_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT name, address, lat, lng, price, pile_total, pile_free "
        "FROM stations WHERE station_id = ?;"));
    q.addBindValue(stationId);
    if (!q.exec() || !q.next()) return Api::err(Api::NotFound, QStringLiteral("充电站不存在"));

    QJsonObject out;
    out["station_id"] = stationId;
    out["name"] = q.value(0).toString();
    out["address"] = q.value(1).toString();
    out["lat"] = q.value(2).toDouble();
    out["lng"] = q.value(3).toDouble();
    out["price"] = q.value(4).toDouble();
    out["pile_total"] = q.value(5).toInt();
    out["pile_free"] = q.value(6).toInt();
    return Api::okData(out);
}

Api::Reply StationService::pileDetailList(const QJsonObject& data) {
    const int stationId = data.value("station_id").toInt();
    if (stationId <= 0) return Api::err(Api::InvalidParam, QStringLiteral("缺少 station_id"));

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT pile_id, type, status, power_kw FROM piles WHERE station_id = ? "
        "ORDER BY pile_id;"));
    q.addBindValue(stationId);
    if (!q.exec()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject p;
        p["pile_id"] = q.value(0).toInt();
        p["type"] = q.value(1).toString();
        p["status"] = q.value(2).toString();
        p["power"] = q.value(3).toDouble();
        arr.append(p);
    }

    QJsonObject out;
    out["piles"] = arr;
    return Api::okData(out);
}

Api::Reply StationService::mgmtList(const QJsonObject& /*data*/) {
    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT station_id, name, address, lat, lng, price, pile_total, pile_free "
        "FROM stations ORDER BY station_id;"));
    if (!q.isActive()) return Api::err(Api::ServerError, q.lastError().text());

    QJsonArray arr;
    while (q.next()) {
        QJsonObject s;
        s["station_id"] = q.value(0).toInt();
        s["name"] = q.value(1).toString();
        s["address"] = q.value(2).toString();
        s["lat"] = q.value(3).toDouble();
        s["lng"] = q.value(4).toDouble();
        s["price"] = q.value(5).toDouble();
        s["pile_total"] = q.value(6).toInt();
        s["pile_free"] = q.value(7).toInt();
        arr.append(s);
    }

    QJsonObject out;
    out["stations"] = arr;
    return Api::okData(out);
}

Api::Reply StationService::addStation(const QJsonObject& data) {
    const QString name = data.value("name").toString().trimmed();
    const QString address = data.value("address").toString().trimmed();
    const double lat = data.value("lat").toDouble();
    const double lng = data.value("lng").toDouble();
    const double price = data.value("price").toDouble();
    const int pileCount = data.value("pile_count").toInt();
    if (name.isEmpty() || address.isEmpty() || pileCount <= 0 || pileCount > 50
        || lat < -90 || lat > 90 || lng < -180 || lng > 180) {
        return Api::err(Api::InvalidParam, QStringLiteral("参数不合法"));
    }

    QSqlDatabase db = DbManager::threadDb();
    db.transaction();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO stations (name, address, lat, lng, price, pile_total, pile_free) "
        "VALUES (?,?,?,?,?,0,0);"));
    q.addBindValue(name);
    q.addBindValue(address);
    q.addBindValue(lat);
    q.addBindValue(lng);
    q.addBindValue(price);
    if (!q.exec()) {
        db.rollback();
        return Api::err(Api::ServerError, q.lastError().text());
    }
    const int stationId = q.lastInsertId().toInt();

    for (int i = 0; i < pileCount; ++i) {
        QSqlQuery pq(db);
        pq.prepare(QStringLiteral(
            "INSERT INTO piles (station_id, code, type, power_kw, status) "
            "VALUES (?,?,?,?,'闲置');"));
        pq.addBindValue(stationId);
        pq.addBindValue(QStringLiteral("P%1-%2").arg(stationId, 2, 10, QChar('0'))
                                                   .arg(i + 1, 2, 10, QChar('0')));
        pq.addBindValue(i < 2 ? QStringLiteral("快充") : QStringLiteral("慢充"));
        pq.addBindValue(i < 2 ? (i == 0 ? 120.0 : 60.0) : 7.0);
        pq.exec();
    }
    db.commit();
    recalcStation(db, stationId);

    QJsonObject out;
    out["station_id"] = stationId;
    return Api::okData(out);
}
