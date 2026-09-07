#pragma once
#include <QVector>
#include <QPair>
#include <QString>

// 腾讯地图 WebService 封装（服务端）：
// 驾车距离矩阵：一次请求计算 起点 -> 多个终点 的驾车距离与时长，
// 供 STATION_RECOMMEND 对 Top5 候选充电站做真实路网估算。
// 失败时返回空向量并填充 err，调用方降级为直线距离估算。
namespace TencentApi {

struct RouteInfo {
    double distMeters = 0.0;   // 驾车距离（米）
    double durSeconds = 0.0;   // 驾车时长（秒）
};

// tos: 终点坐标列表（lat, lng）；结果与 tos 一一对应
QVector<RouteInfo> drivingMatrix(double fromLat, double fromLng,
                                 const QVector<QPair<double, double>>& tos,
                                 QString* err = nullptr);

}
