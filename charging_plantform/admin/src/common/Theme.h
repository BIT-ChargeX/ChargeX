#pragma once
#include <QString>
#include <QColor>
#include <QPalette>

class QWidget;

// 全局视觉主题：Minimal Executive（深色 / 强调=青）。
// 设计令牌：所有颜色只在此定义一次；业务代码通过 objectName 或 Token 引用，
// QSS 由 Theme::globalQss() 从令牌生成，杜绝手写十六进制漂移。
namespace Theme {

// ---- 令牌（深色 Minimal Executive） ----
inline QColor primary()        { return QColor("#35D0BA"); }   // 强调色 青（CTA/选中/曲线）
inline QColor onPrimary()      { return QColor("#062A24"); }
inline QColor accentHover()    { return QColor("#57E0CC"); }
inline QColor accentPressed()  { return QColor("#1FA88F"); }
inline QColor primaryContainer(){ return QColor("#0E332B"); }  // accent tint（选中底/表行）
inline QColor onPrimaryContainer(){ return QColor("#B7F6E9"); }
inline QColor secondary()      { return QColor("#9AA7B4"); }   // 次级前景/图标
inline QColor onSecondary()    { return QColor("#0F1115"); }
inline QColor secondaryContainer(){ return QColor("#0C352E"); }// 导航选中底
inline QColor onSecondaryContainer(){ return QColor("#B7F6E9"); }

inline QColor background()     { return QColor("#0F1115"); }   // 窗口底
inline QColor surface()        { return QColor("#0F1115"); }
inline QColor card()           { return QColor("#1B2129"); }   // 卡片/表格
inline QColor surfaceContainer(){ return QColor("#161A20"); }
inline QColor surfaceHigh()    { return QColor("#232B35"); }   // hover/表头强调
inline QColor onSurface()      { return QColor("#E6EAF0"); }
inline QColor border()         { return QColor("#2A333D"); }
inline QColor borderStrong()   { return QColor("#3B4651"); }

inline QColor textPrimary()   { return QColor("#E6EAF0"); }
inline QColor textSecondary() { return QColor("#9AA7B4"); }
inline QColor textMuted()     { return QColor("#6B7887"); }

inline QColor danger()        { return QColor("#FF6B6B"); }
inline QColor onDanger()      { return QColor("#240809"); }
inline QColor dangerContainer(){ return QColor("#3A1B1E"); }
inline QColor onDangerContainer(){ return QColor("#FFD4D4"); }
inline QColor success()       { return QColor("#5FD48C"); }
inline QColor onSuccess()     { return QColor("#082012"); }
inline QColor successContainer(){ return QColor("#14321B"); }
inline QColor info()          { return QColor("#57A9FF"); }
inline QColor infoContainer() { return QColor("#0E2A49"); }
inline QColor warning()       { return QColor("#F5B860"); }
inline QColor warningContainer(){ return QColor("#3A2C0E"); }
inline QColor accent()        { return primary(); }            // 兼容旧引用（CTA）

// 电桩状态 -> 语义色（文字/前景）
inline QColor statusText(const QString& status) {
    if (status == QStringLiteral("在用"))     return info();
    if (status == QStringLiteral("预约占用")) return warning();
    if (status == QStringLiteral("故障"))     return danger();
    if (status == QStringLiteral("闲置"))     return success();
    return textSecondary();
}

// 电桩状态 -> 语义深底（chip 容器）
inline QColor statusBackground(const QString& status) {
    if (status == QStringLiteral("在用"))     return infoContainer();
    if (status == QStringLiteral("预约占用")) return warningContainer();
    if (status == QStringLiteral("故障"))     return dangerContainer();
    if (status == QStringLiteral("闲置"))     return successContainer();
    return surfaceContainer();
}

// 订单状态 -> 语义色（文字/前景）
inline QColor orderStatusText(const QString& status) {
    if (status == QStringLiteral("预约占用")) return warning();
    if (status == QStringLiteral("充电中"))   return info();
    if (status == QStringLiteral("待结算"))   return QColor("#C9BFFF");
    if (status == QStringLiteral("已完成"))   return success();
    if (status == QStringLiteral("已取消"))   return textMuted();
    return textSecondary();
}

// 订单状态 -> 语义深底（chip 容器）
inline QColor orderStatusBackground(const QString& status) {
    if (status == QStringLiteral("预约占用")) return warningContainer();
    if (status == QStringLiteral("充电中"))   return infoContainer();
    if (status == QStringLiteral("待结算"))   return QColor("#2A2450");
    if (status == QStringLiteral("已完成"))   return successContainer();
    if (status == QStringLiteral("已取消"))   return surfaceHigh();
    return surfaceContainer();
}

// ---- 态化 QSS 辅助（把内联色收编到令牌） ----
// 告警条（MonitorWidget 底部）/ 连接 pill（MainWindow 顶部）样式字符串
QString alarmQss(bool ok);
QString connPillQss(bool ok);

// 深色整体调色板（消除 Fusion 默认浅色在弹出层/边框/下拉露白）
QPalette darkPalette();

// ---- 全局样式表（由令牌生成） ----
QString globalQss();

// 把鼠标指针统一为手型（QSS 不支持 cursor 属性，需在代码里设置）
void applyPointingCursor(QWidget* root);

}
