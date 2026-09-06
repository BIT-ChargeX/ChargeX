#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 用户账户服务：需求1(登录/自动注册) / 6(资料维护) / 7(余额充值) / 碳积分与兑换
namespace UserService {

Api::Reply login(const QJsonObject& data);
Api::Reply updateProfile(const QJsonObject& data);
Api::Reply uploadAvatar(const QJsonObject& data);   // 头像上传：base64 -> MinIO -> 存 URL
Api::Reply recharge(const QJsonObject& data);
Api::Reply rechargeRecords(const QJsonObject& data); // 充值记录查询
Api::Reply getBalance(const QJsonObject& data);
Api::Reply carbonStats(const QJsonObject& data);   // 碳积分与环保足迹
Api::Reply pointsDetail(const QJsonObject& data);  // 积分明细列表
Api::Reply redeemPoints(const QJsonObject& data);  // 积分兑换

}
