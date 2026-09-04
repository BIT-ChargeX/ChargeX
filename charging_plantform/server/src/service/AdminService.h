#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 管理端服务：需求11(管理员登录) / 14(用户账号管理)
namespace AdminService {

Api::Reply login(const QJsonObject& data);   // 成功返回 token
Api::Reply logout(const QJsonObject& data);
Api::Reply userList(const QJsonObject& data);
Api::Reply freezeUser(const QJsonObject& data);

}
