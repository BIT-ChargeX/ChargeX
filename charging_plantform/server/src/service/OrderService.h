#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 充电业务服务：需求8(未完成订单检测) / 9(预约) / 10(订单生成) / 结算(余额支付)
namespace OrderService {

Api::Reply checkUnfinished(const QJsonObject& data);
// 扫描所有过期的“预约占用”订单：超时则标记“已超时”、释放电桩并施加处罚。幂等，可被后台定时器与各业务入口调用。
void sweepExpiredReservations();
Api::Reply reserve(const QJsonObject& data);
Api::Reply create(const QJsonObject& data);
Api::Reply settle(const QJsonObject& data);   // ORDER_SETTLE
Api::Reply settlePreview(const QJsonObject& data); // ORDER_SETTLE_PREVIEW：只读预估本次应扣金额
Api::Reply listOrders(const QJsonObject& data);   // ORDER_LIST：查询用户全部订单
Api::Reply mgmtList(const QJsonObject& data);     // ORDER_MGMT_LIST：管理端分页订单查询
Api::Reply cancelReserved(const QJsonObject& data); // ORDER_MGMT_CANCEL：取消预约占用订单

}
