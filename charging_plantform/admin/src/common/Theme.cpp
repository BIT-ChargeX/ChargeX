#include "Theme.h"

#include <QWidget>
#include <QAbstractButton>
#include <QComboBox>
#include <QTabBar>
#include <QScrollBar>
#include <QList>
#include <QPair>
#include <algorithm>

namespace {
QString c(const QColor& col) { return col.name(); }
}

QString Theme::alarmQss(bool ok) {
    // 告警条：阈值预警（MonitorWidget）
    const QColor fg = ok ? success() : danger();
    const QColor bg = ok ? successContainer() : dangerContainer();
    return QStringLiteral(
               "color:%1; background:%2; padding:9px 12px; border-radius:10px;"
               "font-weight:600; font-size:13px;")
        .arg(c(fg), c(bg));
}

QString Theme::connPillQss(bool ok) {
    // 连接状态 pill（MainWindow 顶栏）
    const QColor fg = ok ? success() : danger();
    const QColor bg = ok ? successContainer() : dangerContainer();
    return QStringLiteral(
               "color:%1; background:%2; padding:4px 12px; border-radius:12px;"
               "font-weight:600; font-size:12px;")
        .arg(c(fg), c(bg));
}

QString Theme::globalQss() {
    // 原始模板用 @token 占位，最后统一替换成令牌色，保证单源。
    QString s = QStringLiteral(R"QSS(
/* ================= M3 · 全局基础 ================= */
QWidget {
    font-family: "Noto Sans CJK SC", "PingFang SC", "Microsoft YaHei",
                 "Segoe UI", "WenQuanYi Micro Hei", sans-serif;
    font-size: 13px;
    color: @ink;
}
QMainWindow, QDialog {
    background: @bg;
}

/* ================= 文本层级（M3 type scale 映射） ================= */
QLabel#pageTitle    { font-size: 22px; font-weight: 700; color: @ink; }
QLabel#sectionTitle { font-size: 15px; font-weight: 600; color: @ink; }
QLabel#fieldLabel   { font-size: 13px; font-weight: 500; color: @sub; }
QLabel#cardCaption  { font-size: 12px; color: @muted; }
QLabel#hintMuted    { color: @muted; font-size: 12px; }
QLabel#brandTitle   { font-size: 17px; font-weight: 700; color: @ink; padding: 0 6px; }
QLabel#userChip {
    color: @ink; font-size: 12px; font-weight: 600;
    background: @surf; border: 1px solid @out;
    border-radius: 999px; padding: 5px 14px;
}

/* ================= 登录品牌深色面板 ================= */
QFrame#brandPanel {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #0E1B33, stop:1 #123A63);
    border: none;
}
QLabel#brandPanelTitle { color: #FFFFFF; font-size: 26px; font-weight: 800; }
QLabel#brandPanelSub   { color: #A8C7EA; font-size: 13px; }
QLabel#brandPanelDesc  { color: #CBD8E6; font-size: 12px; }

/* ================= 输入控件（M3 outlined） ================= */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: @card;
    border: 1px solid @outS;
    border-radius: 8px;
    padding: 8px 12px;
    selection-background-color: @primC;
    selection-color: @onPrimC;
}
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
    border-color: @ink;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border: 2px solid @prim;
    padding: 7px 11px;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
    background: @surf; color: @muted;
}
QComboBox::drop-down { border: none; width: 26px; }
QComboBox QAbstractItemView {
    background: @card; border: 1px solid @out; border-radius: 8px;
    padding: 4px;
    selection-background-color: @primC; selection-color: @onPrimC;
    outline: none;
}
QComboBox QAbstractItemView::item { padding: 7px 10px; border-radius: 6px; }

/* ================= 按钮（M3 filled / outlined / text） ================= */
QPushButton {
    background: transparent;
    color: @prim;
    border: 1px solid @outS;
    border-radius: 999px;
    padding: 9px 20px;
    font-size: 14px;
    font-weight: 600;
}
QPushButton:hover { background: @surfH; }
QPushButton:pressed { background: @out; }
QPushButton:focus { border: 2px solid @prim; padding: 8px 19px; }
QPushButton:disabled { background: @surf; color: @muted; border-color: transparent; }

QPushButton#btnPrimary {
    background: @prim; color: @onPrim; border: none; font-weight: 600;
}
QPushButton#btnPrimary:hover   { background: #1B77B0; }
QPushButton#btnPrimary:pressed { background: #004C7A; }
QPushButton#btnPrimary:disabled { background: #B9D2E4; color: #FFFFFF; }

QPushButton#btnDanger {
    background: @danger; color: @onDanger; border: none; font-weight: 600;
}
QPushButton#btnDanger:hover   { background: #C8322E; }
QPushButton#btnDanger:pressed { background: #900B0B; }
QPushButton#btnDanger:disabled { background: #EFC1C1; color: #FFFFFF; }

QPushButton#btnSuccess {
    background: @succ; color: @onSucc; border: none; font-weight: 600;
}
QPushButton#btnSuccess:hover   { background: #1D7F40; }
QPushButton#btnSuccess:pressed { background: #0F4D25; }
QPushButton#btnSuccess:disabled { background: #B7D8C1; color: #FFFFFF; }

QPushButton#btnGhost {
    background: transparent; border: none; color: @sub;
}
QPushButton#btnGhost:hover   { color: @prim; background: @surfH; }
QPushButton#btnGhost:pressed { background: @out; }

/* ================= 卡片 / 统计卡 ================= */
QFrame#card {
    background: @card;
    border: 1px solid @out;
    border-radius: 12px;
}
QFrame#statCardSky, QFrame#statCardGreen, QFrame#statCardRed, QFrame#statCardAmber {
    background: @card;
    border: 1px solid @out;
    border-radius: 12px;
}
QFrame#statCardSky   { border-top: 4px solid #075985; }
QFrame#statCardGreen { border-top: 4px solid @succ; }
QFrame#statCardRed   { border-top: 4px solid @danger; }
QFrame#statCardAmber { border-top: 4px solid #92400E; }
QLabel#statValue {
    font-size: 28px; font-weight: 700; color: @ink;
}
QLabel#statCaption { color: @muted; font-size: 12px; font-weight: 500; }

/* ================= 表格（容器化表面） ================= */
QTableWidget, QTableView {
    background: @card;
    border: 1px solid @out;
    border-radius: 10px;
    gridline-color: transparent;
    alternate-background-color: @bg;
    selection-background-color: transparent;
    selection-color: @ink;
    outline: none;
}
QTableWidget::item, QTableView::item {
    padding: 6px 8px;
    border: none;
    border-bottom: 1px solid #EDF0F4;
}
QTableWidget::item:hover, QTableView::item:hover { background: @surfH; }
QTableWidget::item:selected, QTableView::item:selected {
    background: @primC; color: @onPrimC;
}
QHeaderView::section {
    background: @surfH;
    color: @sub;
    font-weight: 600;
    font-size: 12px;
    border: none;
    border-right: 1px solid @out;
    border-bottom: 1px solid @out;
    padding: 9px 8px;
}
QTableCornerButton::section { background: @surfH; border: none; }

/* ================= 页签（保留，兼容） ================= */
QTabWidget::pane { border: none; background: @bg; top: -1px; }
QTabBar::tab {
    background: transparent; color: @sub; padding: 11px 22px;
    font-weight: 600; border-bottom: 2px solid transparent;
}
QTabBar::tab:hover { color: @ink; }
QTabBar::tab:selected { color: @prim; border-bottom: 2px solid @prim; }

/* ================= 顶栏 / 状态栏 ================= */
QToolBar {
    background: @card;
    border: none;
    border-bottom: 1px solid @out;
    padding: 10px 18px;
    spacing: 10px;
}
QStatusBar {
    background: @card;
    border-top: 1px solid @out;
    color: @muted;
}

/* ================= 导航栏（NavRail） ================= */
QWidget#navRail {
    background: @card;
    border-right: 1px solid @out;
}
QPushButton[navItem="true"] {
    background: transparent; border: none; color: @sub;
    border-radius: 999px;
    padding: 10px 14px;
    text-align: left;
    font-weight: 600;
    font-size: 13px;
}
QPushButton[navItem="true"]:hover { background: @surfH; }
QPushButton[navItem="true"]:pressed { background: @out; }
QPushButton[navItem="true"]:checked {
    background: @secC; color: @onSecC;
}

/* ================= 滚动条（细） ================= */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @out; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: @outS; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: @out; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: @outS; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }

/* ================= 其它 ================= */
QLabel#logView {
    background: @card; border: none; font-family: monospace; font-size: 12px; color: @sub;
}
QDockWidget { color: @ink; font-weight: 600; }
QFrame#brandSep { background: @out; }
)QSS");

    const QList<QPair<QString, QString>> tokens = {
        {"@prim",    c(Theme::primary())},
        {"@onPrim",  c(Theme::onPrimary())},
        {"@primC",   c(Theme::primaryContainer())},
        {"@onPrimC", c(Theme::onPrimaryContainer())},
        {"@secC",    c(Theme::secondaryContainer())},
        {"@onSecC",  c(Theme::onSecondaryContainer())},
        {"@bg",      c(Theme::background())},
        {"@card",    c(Theme::card())},
        {"@surf",    c(Theme::surfaceContainer())},
        {"@surfH",   c(Theme::surfaceHigh())},
        {"@ink",     c(Theme::textPrimary())},
        {"@sub",     c(Theme::textSecondary())},
        {"@muted",   c(Theme::textMuted())},
        {"@out",     c(Theme::border())},
        {"@outS",    c(Theme::borderStrong())},
        {"@danger",  c(Theme::danger())},
        {"@onDanger", c(Theme::onDanger())},
        {"@dangerC", c(Theme::dangerContainer())},
        {"@succ",    c(Theme::success())},
        {"@onSucc",  c(Theme::onSuccess())},
        {"@succC",   c(Theme::successContainer())},
    };
    // 按 token 名长度降序替换：避免短前缀（如 @prim）先替换而截断长 token（如 @primC），
    // 否则会出现 '#00639bC' 这类非法颜色被 QCssParser 丢弃。
    QList<QPair<QString, QString>> sorted = tokens;
    std::sort(sorted.begin(), sorted.end(),
              [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                  return a.first.size() > b.first.size();
              });
    for (const auto& t : sorted) s.replace(t.first, t.second);
    return s;
}

void Theme::applyPointingCursor(QWidget* root) {
    if (!root) return;
    const auto buttons = root->findChildren<QAbstractButton*>();
    for (QAbstractButton* b : buttons) b->setCursor(Qt::PointingHandCursor);
    const auto combos = root->findChildren<QComboBox*>();
    for (QComboBox* c : combos) c->setCursor(Qt::PointingHandCursor);
    const auto tabs = root->findChildren<QTabBar*>();
    for (QTabBar* t : tabs) t->setCursor(Qt::PointingHandCursor);
}
