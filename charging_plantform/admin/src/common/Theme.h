#pragma once
#include <QString>
#include <QColor>

class QWidget;

// 全局视觉主题：Material 3（浅色 / 天空蓝 seed，继承原藏青+天空蓝品牌）。
// 设计令牌：所有颜色只在此定义一次；业务代码通过 objectName 或 Token 引用，
// QSS 由 Theme::globalQss() 从令牌生成，杜绝手写十六进制漂移。
namespace Theme {

// ---- 令牌（M3 light scheme，近似由 seed #0369A1 生成） ----
inline QColor primary()        { return QColor("#00639B"); }   // primary(40)，CTA/选中/链接
inline QColor onPrimary()      { return QColor("#FFFFFF"); }
inline QColor primaryContainer(){ return QColor("#CAE9FF"); }  // 强调底（悬停选中/表行）
inline QColor onPrimaryContainer(){ return QColor("#001E30"); }
inline QColor secondary()      { return QColor("#4F5E70"); }
inline QColor onSecondary()    { return QColor("#FFFFFF"); }
inline QColor secondaryContainer(){ return QColor("#D5E3F6"); }// 导航选中胶囊
inline QColor onSecondaryContainer(){ return QColor("#0B1F2E"); }

inline QColor background()     { return QColor("#F7F9FD"); }   // surface(98)
inline QColor surface()        { return QColor("#F7F9FD"); }
inline QColor card()           { return QColor("#FFFFFF"); }   // surface-container-low
inline QColor surfaceContainer(){ return QColor("#F0F4F8"); }
inline QColor surfaceHigh()    { return QColor("#E4EAF0"); }   // 表头 / 悬停叠层
inline QColor onSurface()      { return QColor("#171C20"); }
inline QColor border()         { return QColor("#C4CAD4"); }   // outline(80)
inline QColor borderStrong()   { return QColor("#757780"); }   // outline

inline QColor textPrimary()   { return QColor("#171C20"); }
inline QColor textSecondary() { return QColor("#475569"); }
inline QColor textMuted()     { return QColor("#64748B"); }

inline QColor danger()        { return QColor("#BA1A1A"); }    // error(40)
inline QColor onDanger()      { return QColor("#FFFFFF"); }
inline QColor dangerContainer(){ return QColor("#FDE0E0"); }
inline QColor onDangerContainer(){ return QColor("#410002"); }
inline QColor success()       { return QColor("#166534"); }    // 语义绿（自定义）
inline QColor onSuccess()     { return QColor("#FFFFFF"); }
inline QColor successContainer(){ return QColor("#DCF3E1"); }
inline QColor accent()        { return primary(); }            // 兼容旧引用（CTA）

// 电桩状态 -> 语义色（文字/前景，M3 chip 深字）
inline QColor statusText(const QString& status) {
    if (status == QStringLiteral("在用"))     return QColor("#075985");
    if (status == QStringLiteral("预约占用")) return QColor("#92400E");
    if (status == QStringLiteral("故障"))     return danger();
    if (status == QStringLiteral("闲置"))     return success();
    return textSecondary();
}

// 电桩状态 -> 语义浅底（chip 容器）
inline QColor statusBackground(const QString& status) {
    if (status == QStringLiteral("在用"))     return QColor("#E0F2FE");
    if (status == QStringLiteral("预约占用")) return QColor("#FEF3C7");
    if (status == QStringLiteral("故障"))     return dangerContainer();
    if (status == QStringLiteral("闲置"))     return successContainer();
    return surfaceContainer();
}

// ---- 态化 QSS 辅助（把内联色收编到令牌） ----
// 告警条（MonitorWidget 底部）/ 连接 pill（MainWindow 顶部）样式字符串
QString alarmQss(bool ok);
QString connPillQss(bool ok);

// ---- 全局样式表（由令牌生成） ----
QString globalQss();

// 把鼠标指针统一为手型（QSS 不支持 cursor 属性，需在代码里设置）
void applyPointingCursor(QWidget* root);

}
