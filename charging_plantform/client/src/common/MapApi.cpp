#include "MapApi.h"
#include "common/ApiDefs.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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

void MapApi::suggest(const QString& keyword, const QString& region, SuggestCallback cb) {
    if (keyword.trimmed().isEmpty()) {
        if (cb) cb(false, QJsonArray(), QStringLiteral("关键字为空"));
        return;
    }
    if (!hasMapKey()) {
        if (cb) cb(false, QJsonArray(), QStringLiteral("未配置腾讯地图 key"));
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("keyword"), keyword.trimmed());
    query.addQueryItem(QStringLiteral("region"), region.trimmed().isEmpty()
                                                  ? QStringLiteral("北京") : region.trimmed());
    query.addQueryItem(QStringLiteral("key"), QString(Api::kTencentMapKey));

    QUrl url{QString(Api::kTencentSuggestionUrl)};
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ChargingClient/1.0");
    QNetworkReply* reply = m_manager->get(req);

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (cb) cb(false, QJsonArray(), reply->errorString());
            return;
        }
        QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        int status = root.value("status").toInt();
        if (status != 0) {
            if (cb) cb(false, QJsonArray(), root.value("message").toString());
            return;
        }
        // data[] -> [{title, address, lat, lng}]
        QJsonArray items;
        const QJsonArray data = root.value("data").toArray();
        for (const auto& v : data) {
            QJsonObject src = v.toObject();
            QJsonObject it;
            it["title"] = src.value("title").toString();
            it["address"] = src.value("address").toString();
            QJsonObject loc = src.value("location").toObject();
            it["lat"] = loc.value("lat").toDouble();
            it["lng"] = loc.value("lng").toDouble();
            items.append(it);
        }
        if (cb) cb(true, items, QStringLiteral("ok"));
    });
}
