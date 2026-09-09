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
               "color:%1; background:%2; padding:8px 12px; border-radius:4px;"
               "font-weight:600; font-size:13px;")
        .arg(c(fg), c(bg));
}

QString Theme::connPillQss(bool ok) {
    // 连接状态 pill（MainWindow 顶栏）
    const QColor fg = ok ? success() : danger();
    const QColor bg = ok ? successContainer() : dangerContainer();
    return QStringLiteral(
               "color:%1; background:%2; padding:4px 12px; border-radius:4px;"
               "font-weight:600; font-size:12px;")
        .arg(c(fg), c(bg));
}

QPalette Theme::darkPalette() {
    QPalette pal;
    pal.setColor(QPalette::Window, background());
    pal.setColor(QPalette::WindowText, textPrimary());
    pal.setColor(QPalette::Base, card());
    pal.setColor(QPalette::AlternateBase, surfaceContainer());
    pal.setColor(QPalette::Text, textPrimary());
    pal.setColor(QPalette::Button, card());
    pal.setColor(QPalette::ButtonText, textPrimary());
    pal.setColor(QPalette::BrightText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Highlight, primaryContainer());
    pal.setColor(QPalette::HighlightedText, onPrimaryContainer());
    pal.setColor(QPalette::Link, accent());
    pal.setColor(QPalette::ToolTipBase, card());
    pal.setColor(QPalette::ToolTipText, textPrimary());
    pal.setColor(QPalette::PlaceholderText, textMuted());
    pal.setColor(QPalette::Disabled, QPalette::Text, textMuted());
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, textMuted());
    return pal;
}

QString Theme::globalQss() {
    // 原始模板用 @token 占位，最后统一替换成令牌色，保证单源。
    QString s = QStringLiteral(R"QSS(
/* ================= Minimal Executive · 深色 · 全局基础 ================= */
QWidget {
    font-family: "Segoe UI Variable Text", "Segoe UI", "Microsoft YaHei UI",
                 "Microsoft YaHei", "PingFang SC", sans-serif;
    font-size: 13px;
    color: @ink;
}
QMainWindow, QDialog {
    background: @bg;
}

/* ================= 文本层级 ================= */
QLabel#pageTitle    { font-size: 20px; font-weight: 600; color: @ink; }
QLabel#sectionTitle { font-size: 15px; font-weight: 600; color: @ink; }
QLabel#fieldLabel   { font-size: 13px; font-weight: 500; color: @sub; }
QLabel#cardCaption  { font-size: 12px; color: @muted; }
QLabel#hintMuted    { color: @muted; font-size: 12px; }
QLabel#brandTitle   { font-size: 16px; font-weight: 600; color: @ink; padding: 0 6px; }
QLabel#userChip {
    color: @ink; font-size: 12px; font-weight: 600;
    background: @surf; border: 1px solid @out;
    border-radius: 4px; padding: 5px 12px;
}

/* ================= 登录品牌深色面板（深藏青→青 渐变） ================= */
QFrame#brandPanel {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #070A0F, stop:0.6 #0B1B22, stop:1 #0E2E2A);
    border: none;
}
QLabel#brandPanelTitle { color: #E6EAF0; font-size: 24px; font-weight: 600; }
QLabel#brandPanelSub   { color: #9FE8DA; font-size: 13px; }
QLabel#brandPanelDesc  { color: #C7D0DA; font-size: 12px; }

/* ================= 输入控件 ================= */
QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QTimeEdit, QComboBox {
    background: @card;
    border: 1px solid @outS;
    border-radius: 4px;
    padding: 7px 10px;
    selection-background-color: @primC;
    selection-color: @onPrimC;
}
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QDateEdit:hover, QDateTimeEdit:hover, QTimeEdit:hover, QComboBox:hover {
    border-color: @ink;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus, QTimeEdit:focus, QComboBox:focus {
    border: 2px solid @prim;
    padding: 6px 9px;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QDateEdit:disabled, QDateTimeEdit:disabled, QTimeEdit:disabled, QComboBox:disabled {
    background: @surf; color: @muted;
}
/* 复选框（深色） */
QCheckBox { color: @ink; background: transparent; spacing: 6px; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid @outS; border-radius: 3px; background: @card; }
QCheckBox::indicator:hover { border-color: @ink; }
QCheckBox::indicator:checked { background: @prim; border-color: @prim; image: none; }
QCheckBox:disabled { color: @muted; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: @card; border: 1px solid @out; border-radius: 4px;
    padding: 4px;
    selection-background-color: @primC; selection-color: @onPrimC;
    outline: none;
}
QComboBox QAbstractItemView::item { padding: 7px 10px; border-radius: 4px; }

/* ================= 按钮 ================= */
QPushButton {
    background: @card;
    color: @ink;
    border: 1px solid @outS;
    border-radius: 4px;
    padding: 6px 16px;
    font-size: 14px;
    font-weight: 600;
}
QPushButton:hover   { background: @surfH; }
QPushButton:pressed { background: @out; }
QPushButton:focus { border: 2px solid @prim; padding: 5px 15px; }
QPushButton:disabled { background: @surf; color: @muted; border-color: transparent; }

QPushButton#btnPrimary {
    background: @prim; color: @onPrim; border: none; font-weight: 600;
}
QPushButton#btnPrimary:hover   { background: @primH; }
QPushButton#btnPrimary:pressed { background: @primP; }
QPushButton#btnPrimary:disabled { background: @surfH; color: @muted; }

QPushButton#btnDanger {
    background: @danger; color: @onDanger; border: none; font-weight: 600;
}
QPushButton#btnDanger:hover   { background: #FF8A8A; }
QPushButton#btnDanger:pressed { background: #C24A4A; }
QPushButton#btnDanger:disabled { background: @surfH; color: @muted; }

QPushButton#btnSuccess {
    background: @succ; color: @onSucc; border: none; font-weight: 600;
}
QPushButton#btnSuccess:hover   { background: #7FE6A6; }
QPushButton#btnSuccess:pressed { background: #3FA76B; }
QPushButton#btnSuccess:disabled { background: @surfH; color: @muted; }

QPushButton#btnGhost {
    background: transparent; border: none; color: @sub;
}
QPushButton#btnGhost:hover   { background: @surfH; color: @prim; }
QPushButton#btnGhost:pressed { background: @out; }

/* ================= 卡片 / 统计卡 ================= */
QFrame#card {
    background: @card;
    border: 1px solid @out;
    border-radius: 8px;
}
QFrame#statCardSky, QFrame#statCardGreen, QFrame#statCardRed, QFrame#statCardAmber {
    background: @card;
    border: 1px solid @out;
    border-radius: 8px;
}
QFrame#statCardSky   { border-top: 3px solid @info; }
QFrame#statCardGreen { border-top: 3px solid @succ; }
QFrame#statCardRed   { border-top: 3px solid @danger; }
QFrame#statCardAmber { border-top: 3px solid @warn; }
QLabel#statValue {
    font-size: 24px; font-weight: 600; color: @ink;
}
QLabel#statCaption { color: @sub; font-size: 12px; font-weight: 500; }

/* ================= 表格 ================= */
QTableWidget, QTableView {
    background: @card;
    border: 1px solid @out;
    border-radius: 6px;
    gridline-color: transparent;
    alternate-background-color: @bg;
    selection-background-color: transparent;
    selection-color: @ink;
    outline: none;
}
QTableWidget::item, QTableView::item {
    padding: 6px 10px;
    border: none;
    border-bottom: 1px solid @out;
}
QTableWidget::item:hover, QTableView::item:hover { background: @surfH; }
QTableWidget::item:selected, QTableView::item:selected {
    background: @primC; color: @onPrimC;
}
QHeaderView::section {
    background: @surf;
    color: @sub;
    font-weight: 600;
    font-size: 12px;
    border: none;
    border-right: 1px solid @out;
    border-bottom: 1px solid @out;
    padding: 8px 10px;
}
QTableCornerButton::section { background: @surf; border: none; }

/* ================= 页签（保留兼容） ================= */
QTabWidget::pane { border: none; background: @bg; top: -1px; }
QTabBar::tab {
    background: transparent; color: @sub; padding: 10px 20px;
    font-weight: 600; border-bottom: 2px solid transparent;
}
QTabBar::tab:hover { color: @ink; }
QTabBar::tab:selected { color: @prim; border-bottom: 2px solid @prim; }

/* ================= 顶栏 / 状态栏 ================= */
QToolBar {
    background: @card;
    border: none;
    border-bottom: 1px solid @out;
    padding: 8px 16px;
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
    border-radius: 4px;
    padding: 8px 12px;
    text-align: left;
    font-weight: 500;
    font-size: 13px;
}
QPushButton[navItem="true"]:hover { background: @surfH; }
QPushButton[navItem="true"]:pressed { background: @out; }
QPushButton[navItem="true"]:checked {
    background: @secC; color: @onSecC; font-weight: 600;
}

/* ================= 滚动条（细） ================= */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #3B4651; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #55626E; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #3B4651; border-radius: 5px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #55626E; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }

/* ================= 其它 ================= */
QLabel#logView {
    background: @card; border: none; font-family: monospace; font-size: 12px; color: @sub;
}
QDockWidget { color: @ink; font-weight: 600; }
QFrame#brandSep { background: @out; }
)QSS");

    const QList<QPair<QString, QString>> tokens = {
        {"@prim",     c(Theme::primary())},
        {"@primH",    c(Theme::accentHover())},
        {"@primP",    c(Theme::accentPressed())},
        {"@onPrim",   c(Theme::onPrimary())},
        {"@primC",    c(Theme::primaryContainer())},
        {"@onPrimC",  c(Theme::onPrimaryContainer())},
        {"@secC",     c(Theme::secondaryContainer())},
        {"@onSecC",   c(Theme::onSecondaryContainer())},
        {"@bg",       c(Theme::background())},
        {"@card",     c(Theme::card())},
        {"@surf",     c(Theme::surfaceContainer())},
        {"@surfH",    c(Theme::surfaceHigh())},
        {"@ink",      c(Theme::textPrimary())},
        {"@sub",      c(Theme::textSecondary())},
        {"@muted",    c(Theme::textMuted())},
        {"@out",      c(Theme::border())},
        {"@outS",     c(Theme::borderStrong())},
        {"@danger",   c(Theme::danger())},
        {"@onDanger", c(Theme::onDanger())},
        {"@dangerC",  c(Theme::dangerContainer())},
        {"@succ",     c(Theme::success())},
        {"@onSucc",   c(Theme::onSuccess())},
        {"@succC",    c(Theme::successContainer())},
        {"@info",     c(Theme::info())},
        {"@infoC",    c(Theme::infoContainer())},
        {"@warn",     c(Theme::warning())},
        {"@warnC",    c(Theme::warningContainer())},
    };
    // 按 token 名长度降序替换，避免短前缀（如 @prim）截断长 token（如 @primC）
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
