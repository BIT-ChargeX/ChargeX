#pragma once
#include <QtGlobal>
#include <QJsonObject>
#include <QString>

// 协议 / 常量集中定义（PC 管理端），与 server 侧 ApiDefs 同步。
// 管理命令需携带 ADMIN_LOGIN 返回的 token。
namespace Api {

inline constexpr const char* kHost = "127.0.0.1";
inline constexpr int kPort = 9000;

// ---- 管理端命令 ----
inline constexpr const char* CmdAdminLogin        = "ADMIN_LOGIN";
inline constexpr const char* CmdAdminLogout       = "ADMIN_LOGOUT";
inline constexpr const char* CmdUserList          = "USER_LIST";
inline constexpr const char* CmdUserFreeze        = "USER_FREEZE";
inline constexpr const char* CmdPileMgmtList      = "PILE_MGMT_LIST";
inline constexpr const char* CmdPileMgmtReboot    = "PILE_MGMT_REBOOT";
inline constexpr const char* CmdPileMgmtSetStatus = "PILE_MGMT_SET_STATUS";
inline constexpr const char* CmdPileMonSummary    = "PILE_MON_SUMMARY";
inline constexpr const char* CmdPileRuntimeLogList= "PILE_RUNTIME_LOG_LIST";
inline constexpr const char* CmdOpsLogList        = "OPS_LOG_LIST";
inline constexpr const char* CmdStationMgmtList   = "STATION_MGMT_LIST";
inline constexpr const char* CmdStationMgmtAdd    = "STATION_MGMT_ADD";
inline constexpr const char* CmdSalesSummary      = "SALES_SUMMARY";

// ---- 错误码（与服务端一致）----
enum ErrCode {
    LocalNetError = -1,
    Ok = 0,
    InvalidParam  = 1001,
    NotFound      = 1002,
    StateConflict = 1003,
    Forbidden     = 1004,
    ServerError   = 5000
};

}
