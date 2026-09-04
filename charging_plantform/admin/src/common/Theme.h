#pragma once
#include <QString>
#include <QColor>

class QWidget;

// 全局视觉主题（浅色商务风）
// 设计令牌：藏青主色 #0F172A / 天空蓝强调 #0369A1 / 浅灰底 #F8FAFC
// 所有颜色只在 Theme 中定义一次；业务代码通过 objectName 或 Token 引用。
namespace Theme {

// ---- 令牌 ----
inline QColor primary()      { return QColor("#0F172A"); }   // 藏青
inline QColor onPrimary()    { return QColor("#FFFFFF"); }
inline QColor accent()       { return QColor("#0369A1"); }   // 天空蓝 CTA
inline QColor onAccent()     { return QColor("#FFFFFF"); }
inline QColor background()   { return QColor("#F8FAFC"); }
inline QColor card()         { return QColor("#FFFFFF"); }
inline QColor border()       { return QColor("#E2E8F0"); }
inline QColor textPrimary()  { return QColor("#0F172A"); }
inline QColor textSecondary(){ return QColor("#475569"); }
inline QColor textMuted()    { return QColor("#94A3B8"); }
inline QColor danger()       { return QColor("#DC2626"); }
inline QColor success()      { return QColor("#16A34A"); }

// 电桩状态 -> 语义色（文字）
inline QColor statusText(const QString& status) {
    if (status == QStringLiteral("在用") || status == QStringLiteral("预约占用"))
        return status == QStringLiteral("预约占用") ? QColor("#B45309") : QColor("#1D4ED8");
    if (status == QStringLiteral("故障")) return danger();
    if (status == QStringLiteral("闲置")) return success();
    return textSecondary();
}

// 电桩状态 -> 语义底色（浅色块）
inline QColor statusBackground(const QString& status) {
    if (status == QStringLiteral("在用"))     return QColor("#EFF6FF");
    if (status == QStringLiteral("预约占用")) return QColor("#FFFBEB");
    if (status == QStringLiteral("故障"))     return QColor("#FEF2F2");
    if (status == QStringLiteral("闲置"))     return QColor("#F0FDF4");
    return QColor("#F8FAFC");
}

// ---- 全局样式表 ----
QString globalQss();

// 把鼠标指针统一为手型（QSS 不支持 cursor 属性，需在代码里设置）
void applyPointingCursor(QWidget* root);

}
