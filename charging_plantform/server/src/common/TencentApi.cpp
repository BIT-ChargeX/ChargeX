#include "TencentApi.h"
#include "ApiDefs.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace TencentApi {

QVector<RouteInfo> drivingMatrix(double fromLat, double fromLng,
                                 const QVector<QPair<double, double>>& tos,
                                 QString* err) {
    QVector<RouteInfo> out;
    if (err) err->clear();

    const QString key = QString::fromLatin1(Api::kTencentMapKey).trimmed();
    if (key.isEmpty()) {
        if (err) *err = QStringLiteral("未配置腾讯地图 key");
        return out;
    }
    if (tos.isEmpty()) return out;

    // to 参数：多组 "lat,lng" 用分号拼接
    QStringList toParts;
    for (const auto& t : tos) {
        toParts << QStringLiteral("%1,%2").arg(t.first, 0, 'f', 6).arg(t.second, 0, 'f', 6);
    }

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("mode"), QStringLiteral("driving"));
    q.addQueryItem(QStringLiteral("from"),
                   QStringLiteral("%1,%2").arg(fromLat, 0, 'f', 6).arg(fromLng, 0, 'f', 6));
    q.addQueryItem(QStringLiteral("to"), toParts.join(QLatin1Char(';')));
    q.addQueryItem(QStringLiteral("key"), key);

    QUrl url{QString::fromLatin1(Api::kTencentMatrixUrl)};
    url.setQuery(q);

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ChargingServer/1.0");
    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);   // 与 AliyunSms 一致：10s 超时

    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (err) *err = QStringLiteral("腾讯地图请求超时");
        return out;
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = reply->errorString();
        return out;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const int status = root.value("status").toInt();
    if (status != 0) {
        if (err) *err = root.value("message").toString();
        return out;
    }

    // rows[0].elements[] 与 tos 一一对应
    const QJsonArray rows = root.value("result").toObject().value("rows").toArray();
    if (rows.isEmpty()) {
        if (err) *err = QStringLiteral("腾讯地图返回空结果");
        return out;
    }
    const QJsonArray elements = rows.first().toObject().value("elements").toArray();
    for (const auto& v : elements) {
        const QJsonObject e = v.toObject();
        RouteInfo r;
        r.distMeters = e.value("distance").toDouble();
        r.durSeconds = e.value("duration").toDouble();
        out.append(r);
    }
    return out;
}

}
