#pragma once
#include <QJsonObject>
#include "ApiDefs.h"

// 销售业绩汇总：今日/本月/总营收 + 近N日每日营收序列
namespace SalesService {

Api::Reply summary(const QJsonObject& data);   // SALES_SUMMARY {days}

}
