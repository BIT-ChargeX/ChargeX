#pragma once
#include <QtGlobal>
#include <QJsonObject>
#include <QString>

// 协议 / 常量集中定义（服务端），命令码与字段对齐 protocol/interface_protocol.md v1.0。
namespace Api {

inline constexpr int kPort = 9000;
inline constexpr const char* kDbFile = "charging_platform.db";

// ---- 命令码 ----
// 用户账户（对应客户端 account）
inline constexpr const char* CmdUserLogin          = "USER_LOGIN";   // 手机号+密码登录（首次登录自动注册）
inline constexpr const char* CmdUserUpdateProfile  = "USER_UPDATE_PROFILE";
inline constexpr const char* CmdUserRecharge       = "USER_RECHARGE";
inline constexpr const char* CmdUserGetBalance     = "USER_GET_BALANCE";
inline constexpr const char* CmdUserRechargeRecords= "USER_RECHARGE_RECORDS"; // 充值记录查询
inline constexpr const char* CmdUserCarbonStats    = "USER_CARBON_STATS";    // 碳积分与环保足迹
inline constexpr const char* CmdUserPointsDetail   = "USER_POINTS_DETAIL";   // 积分明细列表
inline constexpr const char* CmdUserPointsRedeem   = "USER_POINTS_REDEEM";   // 积分兑换
// 充电站 / 电桩查询
inline constexpr const char* CmdStationNearby      = "STATION_NEARBY";
inline constexpr const char* CmdStationDetail      = "STATION_DETAIL";
inline constexpr const char* CmdPileDetailList     = "PILE_DETAIL_LIST";
// 充电业务
inline constexpr const char* CmdOrderCheckUnfinished = "ORDER_CHECK_UNFINISHED";
inline constexpr const char* CmdOrderReserve          = "ORDER_RESERVE";
inline constexpr const char* CmdOrderCreate           = "ORDER_CREATE";
inline constexpr const char* CmdOrderSettle           = "ORDER_SETTLE";   // 结算：服务端计算费用并扣减余额
// 管理端（除 ADMIN_LOGIN/ADMIN_LOGOUT 外，请求 data 均需携带 token：
// ADMIN_LOGIN 成功后返回 token，分发器对管理命令做会话校验）
inline constexpr const char* CmdAdminLogin       = "ADMIN_LOGIN";
inline constexpr const char* CmdAdminLogout      = "ADMIN_LOGOUT";
inline constexpr const char* CmdUserList         = "USER_LIST";
inline constexpr const char* CmdUserFreeze       = "USER_FREEZE";
inline constexpr const char* CmdPileMgmtList     = "PILE_MGMT_LIST";
inline constexpr const char* CmdPileMgmtReboot   = "PILE_MGMT_REBOOT";
inline constexpr const char* CmdPileMgmtSetStatus= "PILE_MGMT_SET_STATUS";
inline constexpr const char* CmdPileMonSummary   = "PILE_MON_SUMMARY";
inline constexpr const char* CmdOpsLogList       = "OPS_LOG_LIST";
inline constexpr const char* CmdStationMgmtList  = "STATION_MGMT_LIST";
inline constexpr const char* CmdStationMgmtAdd   = "STATION_MGMT_ADD";
// SALES_SUMMARY：入参 days(7/30) 或 start/end(yyyy-MM-dd,≤366天)；
// 返回 today/month/total(+today_pct/month_pct 环比,-1 无基线)、daily[]、top_stations[]
inline constexpr const char* CmdSalesSummary     = "SALES_SUMMARY";
// 管理端查询充电桩终端实时运行日志（需 token）
inline constexpr const char* CmdPileRuntimeLogList = "PILE_RUNTIME_LOG_LIST";

// 充电桩终端(模拟设备)接入：设备长连接，设备→服务器；控制指令经 REPORT 的 pending 回带
inline constexpr const char* CmdPileDevHello     = "PILE_DEV_HELLO";
inline constexpr const char* CmdPileDevReport    = "PILE_DEV_REPORT";
inline constexpr const char* CmdPileDevResult    = "PILE_DEV_RESULT";

// ---- 错误码（interface_protocol.md 第4节）----
enum ErrCode {
    LocalNetError = -1,
    Ok = 0,
    InvalidParam  = 1001,  // 参数校验失败
    NotFound      = 1002,  // 资源不存在
    StateConflict = 1003,  // 状态冲突（已被预约/账号冻结）
    Forbidden     = 1004,  // 权限不足
    ServerError   = 5000   // 服务端内部错误
};

// 各业务服务的统一返回
struct Reply {
    int code = Ok;
    QJsonObject data;
};

inline Reply ok() { return Reply(); }
inline Reply err(int code, const QString& msg) {
    Reply r;
    r.code = code;
    r.data["msg"] = msg;
    return r;
}
inline Reply okData(const QJsonObject& data) { Reply r; r.data = data; return r; }

inline const char* errorText(int code) {
    switch (code) {
    case InvalidParam:  return "参数校验失败";
    case NotFound:      return "资源不存在";
    case StateConflict: return "状态冲突";
    case Forbidden:     return "权限不足";
    case ServerError:   return "服务端内部错误";
    default:            return "操作失败";
    }
}

// 电桩状态常量（与 schema 一致）
namespace PileStatus {
inline constexpr const char* kIdle    = "闲置";
inline constexpr const char* kInUse   = "在用";
inline constexpr const char* kFault   = "故障";
inline constexpr const char* kReserved = "预约占用";
}
// 订单状态常量（与 schema 一致）
namespace OrderStatus {
inline constexpr const char* kReserved = "预约占用";
inline constexpr const char* kCharging = "充电中";
inline constexpr const char* kPending  = "待结算";
inline constexpr const char* kDone     = "已完成";
inline constexpr const char* kCanceled = "已取消";
}

}
