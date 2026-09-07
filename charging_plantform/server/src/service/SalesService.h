#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 销售业绩汇总（SALES_SUMMARY）：
//   支持 days(7/30) 或自定义 start/end(yyyy-MM-dd，≤366天) 窗口；
//   返回 今日/本月/累计营收 + 日环比(今日 vs 昨日)/月环比(本月 vs 上月同期)，
//   以及窗口内每日营收序列 daily[] 与 站点营收 Top5 top_stations[]。
namespace SalesService {

Api::Reply summary(const QJsonObject& data);   // SALES_SUMMARY {days | start,end}

}
