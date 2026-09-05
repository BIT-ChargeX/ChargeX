#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 充电桩模拟端接入服务：
//  PILE_DEV_HELLO  （绑定设备与电桩）
//  PILE_DEV_REPORT（状态/遥测上报，带回待办控制指令）
//  PILE_DEV_RESULT（控制指令执行回执）
// 服务器为状态真源：REPORT 仅允许“闲置→故障”自发故障与遥测更新，避免覆盖业务状态。
namespace PileDeviceService {

Api::Reply hello(const QJsonObject& data);
Api::Reply report(const QJsonObject& data);
Api::Reply result(const QJsonObject& data);
Api::Reply runtimeLogList(const QJsonObject& data);   // PILE_RUNTIME_LOG_LIST

}
