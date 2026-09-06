#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 用户账户服务：需求1(登录/自动注册) / 6(资料维护) / 7(余额充值)
namespace UserService {

Api::Reply login(const QJsonObject& data);
Api::Reply updateProfile(const QJsonObject& data);
Api::Reply recharge(const QJsonObject& data);
Api::Reply getBalance(const QJsonObject& data);

}
