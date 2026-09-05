#include "MapApi.h"
#include "common/ApiDefs.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

MapApi& MapApi::instance() {
    static MapApi inst;
    return inst;
}

MapApi::MapApi(QObject* parent) : QObject(parent) {
    m_manager = new QNetworkAccessManager(this);
}

bool MapApi::hasMapKey() const {
    return Api::kTencentMapKey != nullptr && QString(Api::kTencentMapKey).trimmed().size() > 0;
}

void MapApi::geocode(const QString& address, GeocodeCallback cb) {
    if (address.trimmed().isEmpty()) {
        if (cb) cb(false, 0.0, 0.0, QStringLiteral("地址为空"));
        return;
    }
    if (!hasMapKey()) {
        if (cb) cb(false, 0.0, 0.0, QStringLiteral("未配置腾讯地图 key，请手动输入经纬度"));
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address.trimmed());
    query.addQueryItem(QStringLiteral("key"), QString(Api::kTencentMapKey));

    QUrl url{QString(Api::kTencentGeocoderUrl)};
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ChargingClient/1.0");
    QNetworkReply* reply = m_manager->get(req);

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (cb) cb(false, 0.0, 0.0, reply->errorString());
            return;
        }
        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        int status = root.value("status").toInt();
        if (status != 0) {
            if (cb) cb(false, 0.0, 0.0, root.value("message").toString());
            return;
        }
        QJsonObject location = root.value("result").toObject().value("location").toObject();
        double lat = location.value("lat").toDouble();
        double lng = location.value("lng").toDouble();
        if (cb) cb(true, lat, lng, QStringLiteral("ok"));
    });
}
