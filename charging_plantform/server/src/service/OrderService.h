#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 充电业务服务：需求8(未完成订单检测) / 9(预约) / 10(订单生成) / 结算(余额支付)
namespace OrderService {

Api::Reply checkUnfinished(const QJsonObject& data);
Api::Reply reserve(const QJsonObject& data);
Api::Reply create(const QJsonObject& data);
Api::Reply settle(const QJsonObject& data);   // ORDER_SETTLE

}
