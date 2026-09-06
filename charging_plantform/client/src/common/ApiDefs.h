#pragma once
#include <QtGlobal>

// 协议 / 常量集中定义，全模块统一引用。
// 命令码与字段对齐 protocol/interface_protocol.md v1.0，任何改动需先在组内同步版本。

namespace Api {

inline constexpr const char* kHost = "127.0.0.1";
inline constexpr int kPort = 9000;

// 腾讯地图 WebService Key：演示前填入真实 key；
// 留空时定位自动降级为"手动输入经纬度"，不影响其余功能。
inline constexpr const char* kTencentMapKey = "U66BZ-DBO6U-H5AVC-GWFDF-BY7EO-LQFOX";
inline constexpr const char* kTencentMapReferer = "ChargingClient";
inline constexpr const char* kTencentGeocoderUrl = "https://apis.map.qq.com/ws/geocoder/v1/";
inline constexpr const char* kTencentRouteUrl = "https://apis.map.qq.com/uri/v1/routeplan";
// 地点联想（地址输入框关键字下拉提示）
inline constexpr const char* kTencentSuggestionUrl = "https://apis.map.qq.com/ws/place/v1/suggestion/";

// ---- 命令码 ----
// 账户
inline constexpr const char* CmdUserLogin          = "USER_LOGIN";   // 手机号+密码登录（首次登录自动注册）
inline constexpr const char* CmdUserUpdateProfile  = "USER_UPDATE_PROFILE";
inline constexpr const char* CmdUserRecharge       = "USER_RECHARGE";
inline constexpr const char* CmdUserGetBalance     = "USER_GET_BALANCE";
// 充电站 / 电桩（刘恩东）
inline constexpr const char* CmdStationNearby      = "STATION_NEARBY";
inline constexpr const char* CmdStationDetail      = "STATION_DETAIL";
inline constexpr const char* CmdPileDetailList     = "PILE_DETAIL_LIST";
// 综合推荐（需求20）：服务端按 驾车距离/时长/价格/空闲率 加权评分排序
inline constexpr const char* CmdStationRecommend   = "STATION_RECOMMEND";
// 充电业务（孙晟云）
inline constexpr const char* CmdOrderCheckUnfinished = "ORDER_CHECK_UNFINISHED";
inline constexpr const char* CmdOrderReserve          = "ORDER_RESERVE";
inline constexpr const char* CmdOrderCreate           = "ORDER_CREATE";
inline constexpr const char* CmdOrderSettle           = "ORDER_SETTLE";
// 注：ORDER_SETTLE 结算/扣费/释放电桩的业务逻辑在服务端（OrderService::settle）
// 结算命令 ORDER_SETTLE 协议 v1.1 尚未冻结，冻结前服务端不会响应，客户端不主动发送。

// ---- 错误码（interface_protocol.md 第4节）----
enum ErrCode {
    LocalNetError = -1,      // 本地错误：服务器未连接，请求未发出
    Ok = 0,
    InvalidParam  = 1001,  // 参数校验失败
    NotFound      = 1002,  // 资源不存在
    StateConflict = 1003,  // 状态冲突（已被预约/已冻结）
    Forbidden     = 1004,  // 权限不足
    ServerError   = 5000   // 服务端内部错误
};

}
