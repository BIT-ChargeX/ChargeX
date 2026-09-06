#include "StationService.h"
#include "DbManager.h"
#include "TencentApi.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <QVector>
#include <QPair>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QStringList>

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

// 需求20 综合推荐权重：驾车距离 / 驾车时长 / 价格 / 空闲率
constexpr double kWDist = 0.30;
constexpr double kWTime = 0.25;
constexpr double kWPrice = 0.20;
constexpr double kWFree = 0.25;

// 推荐结果缓存：同坐标 60s 内直接复用，避免反复调用腾讯矩阵（个人key有日配额）
QMutex g_recoCacheMutex;
QHash<QString, QPair<QDateTime, QJsonArray>> g_recoCache;

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

// STATION_RECOMMEND：需求20 智能充电站推荐
// 直线距离 Top5 初筛 -> 腾讯驾车距离矩阵（真实路网距离/时长）-> 加权综合评分
// 评分维度：驾车距离 0.30 / 驾车时长 0.25 / 价格 0.20 / 空闲率 0.25（权重见文件顶部）
// 腾讯 API 失败时降级为直线距离估算（*1.4 路网系数、30km/h），保证流程不断。
Api::Reply StationService::recommend(const QJsonObject& data) {
    const double lat = data.value("lat").toDouble();
    const double lng = data.value("lng").toDouble();
    if (qFuzzyIsNull(lat) && qFuzzyIsNull(lng)) {
        return Api::err(Api::InvalidParam, QStringLiteral("缺少有效坐标"));
    }

    // 60s 缓存：同坐标（3位小数内）不重复调用腾讯地图
    const QString cacheKey =
        QStringLiteral("%1,%2").arg(lat, 0, 'f', 3).arg(lng, 0, 'f', 3);
    {
        QMutexLocker lock(&g_recoCacheMutex);
        const auto it = g_recoCache.constFind(cacheKey);
        if (it != g_recoCache.constEnd()
            && it->first.secsTo(QDateTime::currentDateTime()) < 60) {
            QJsonObject out;
            out["stations"] = it->second;
            return Api::okData(out);
        }
    }

    QSqlDatabase db = DbManager::threadDb();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT station_id, name, address, lat, lng, price, pile_total, pile_free "
        "FROM stations;"));
    if (!q.isActive()) return Api::err(Api::ServerError, q.lastError().text());

    // 1) 直线距离初筛 Top5（控制腾讯矩阵调用量）
    struct Cand { QJsonObject obj; double straight; };
    QVector<Cand> cands;
    while (q.next()) {
        const double sLat = q.value(3).toDouble();
        const double sLng = q.value(4).toDouble();
        QJsonObject s;
        s["station_id"] = q.value(0).toInt();
        s["name"] = q.value(1).toString();
        s["address"] = q.value(2).toString();
        s["lat"] = sLat;
        s["lng"] = sLng;
        s["price"] = q.value(5).toDouble();
        s["pile_total"] = q.value(6).toInt();
        s["pile_free"] = q.value(7).toInt();
        const double d = distanceKm(lat, lng, sLat, sLng);
        s["straight_km"] = d;
        cands.append({s, d});
    }
    if (cands.isEmpty()) {
        QJsonObject out;
        out["stations"] = QJsonArray();
        return Api::okData(out);
    }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.straight < b.straight; });
    if (cands.size() > 5) cands.resize(5);

    // 2) 腾讯驾车距离矩阵：一次请求算完 Top5 各组的真实驾车距离/时长
    QVector<QPair<double, double>> tos;
    for (const auto& c : cands)
        tos.append({c.obj.value("lat").toDouble(), c.obj.value("lng").toDouble()});
    QString routeErr;
    const QVector<TencentApi::RouteInfo> routes =
        TencentApi::drivingMatrix(lat, lng, tos, &routeErr);
    const bool routeOk = routes.size() == cands.size();

    // 3) 评分维度数据 + 归一化 + 加权
    const int n = static_cast<int>(cands.size());
    QVector<double> d(n), t(n), p(n), f(n);
    double minD = 1e18, maxD = 0.0, minT = 1e18, maxT = 0.0;
    double minP = 1e18, maxP = 0.0, minF = 1e18, maxF = 0.0;
    for (int i = 0; i < n; ++i) {
        const double straight = cands[i].obj.value("straight_km").toDouble();
        if (routeOk) {
            d[i] = routes[i].distMeters / 1000.0;
            t[i] = routes[i].durSeconds / 60.0;
        } else {
            d[i] = straight * 1.4;        // 直线->路网 经验系数
            t[i] = d[i] / 30.0 * 60.0;     // 按 30km/h 估算时长
        }
        p[i] = cands[i].obj.value("price").toDouble();
        const int total = cands[i].obj.value("pile_total").toInt();
        const int freeCnt = cands[i].obj.value("pile_free").toInt();
        f[i] = total > 0 ? static_cast<double>(freeCnt) / total : 0.0;

        minD = qMin(minD, d[i]); maxD = qMax(maxD, d[i]);
        minT = qMin(minT, t[i]); maxT = qMax(maxT, t[i]);
        minP = qMin(minP, p[i]); maxP = qMax(maxP, p[i]);
        minF = qMin(minF, f[i]); maxF = qMax(maxF, f[i]);
    }

    // min-max 归一化；全相等时取 0.5 避免除零
    const auto norm = [](double v, double mn, double mx) {
        return mx > mn ? (v - mn) / (mx - mn) : 0.5;
    };
    QVector<double> score(n);
    int recoIdx = 0, fastIdx = 0;
    for (int i = 0; i < n; ++i) {
        // 距离/时长/价格越小越好 -> 反向；空闲率越大越好 -> 正向
        score[i] = kWDist * (1.0 - norm(d[i], minD, maxD))
                 + kWTime * (1.0 - norm(t[i], minT, maxT))
                 + kWPrice * (1.0 - norm(p[i], minP, maxP))
                 + kWFree * norm(f[i], minF, maxF);
        if (score[i] > score[recoIdx]) recoIdx = i;
        if (t[i] < t[fastIdx]) fastIdx = i;
    }

    // 4) 按综合分降序输出；recommend/fastest 徽章标记两张特殊卡片
    QVector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&score](int a, int b) { return score[a] > score[b]; });

    QJsonArray arr;
    for (const int i : order) {
        QJsonObject s = cands[i].obj;
        s["drive_km"] = d[i];
        s["drive_min"] = t[i];
        s["score"] = score[i];
        s["recommend"] = (i == recoIdx);
        s["fastest"] = (i == fastIdx);
        arr.append(s);
    }

    {
        QMutexLocker lock(&g_recoCacheMutex);
        g_recoCache.insert(cacheKey, {QDateTime::currentDateTime(), arr});
    }

    QJsonObject out;
    out["stations"] = arr;
    out["route_ok"] = routeOk;
    if (!routeOk) out["route_error"] = routeErr;
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
