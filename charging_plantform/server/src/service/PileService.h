#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 电桩管理 / 监控服务：需求12(状态监控汇总) / 13(电桩列表、远程重启、操作日志)
namespace PileService {

Api::Reply list(const QJsonObject& data);         // PILE_MGMT_LIST（含编号 code）
Api::Reply reboot(const QJsonObject& data);       // PILE_MGMT_REBOOT
Api::Reply setStatus(const QJsonObject& data);    // PILE_MGMT_SET_STATUS（故障/闲置）
Api::Reply opsLogList(const QJsonObject& data);   // OPS_LOG_LIST
Api::Reply summary(const QJsonObject& data);      // PILE_MON_SUMMARY

}
