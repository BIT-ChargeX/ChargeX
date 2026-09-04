#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 充电站 / 电桩服务：需求2(附近查询) / 3(站点信息) / 4(电桩详情)
// 以及 PC 管理端充电站管理（列表 / 新增）
namespace StationService {

Api::Reply nearby(const QJsonObject& data);
Api::Reply detail(const QJsonObject& data);
Api::Reply pileDetailList(const QJsonObject& data);
Api::Reply mgmtList(const QJsonObject& data);   // STATION_MGMT_LIST
Api::Reply addStation(const QJsonObject& data); // STATION_MGMT_ADD

}
