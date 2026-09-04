#pragma once
#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

// 腾讯地图 WebService 封装：地址 -> 经纬度（需求2 的定位环节）。
// 未配置地图 key 或网络不可用时返回失败，由调用方降级为手动输入经纬度。
class MapApi : public QObject {
    Q_OBJECT
public:
    using GeocodeCallback =
        std::function<void(bool ok, double lat, double lng, const QString& msg)>;

    static MapApi& instance();

    bool hasMapKey() const;
    void geocode(const QString& address, GeocodeCallback cb);

private:
    explicit MapApi(QObject* parent = nullptr);

    QNetworkAccessManager* m_manager;
};
