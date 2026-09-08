#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 用户账户服务：需求1(邮箱登录/注册/忘记密码) / 6(资料维护) / 7(余额充值)/ 碳积分与兑换
namespace UserService {

Api::Reply login(const QJsonObject& data);
Api::Reply registerUser(const QJsonObject& data);
Api::Reply sendCode(const QJsonObject& data);      // 发送邮箱验证码（注册/忘记密码）
Api::Reply resetPassword(const QJsonObject& data); // 验证码重置密码
Api::Reply updateProfile(const QJsonObject& data);
Api::Reply uploadAvatar(const QJsonObject& data);   // 头像上传：base64 -> MinIO -> 存 URL
Api::Reply recharge(const QJsonObject& data);
Api::Reply rechargeRecords(const QJsonObject& data); // 充值记录查询
Api::Reply getBalance(const QJsonObject& data);
Api::Reply carbonStats(const QJsonObject& data);   // 碳积分与环保足迹
Api::Reply pointsDetail(const QJsonObject& data);  // 积分明细列表
Api::Reply redeemPoints(const QJsonObject& data);  // 积分兑换

}
