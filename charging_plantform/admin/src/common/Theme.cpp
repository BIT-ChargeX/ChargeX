#include "Theme.h"

#include <QWidget>
#include <QAbstractButton>
#include <QComboBox>
#include <QTabBar>
#include <QScrollBar>

QString Theme::globalQss() {
    return QStringLiteral(R"QSS(
/* ============ 全局基础 ============ */
QWidget {
    font-family: "Noto Sans CJK SC", "PingFang SC", "Microsoft YaHei", "WenQuanYi Micro Hei", sans-serif;
    font-size: 13px;
    color: #0F172A;
}
QMainWindow, QDialog {
    background: #F8FAFC;
}

/* ============ 通用标签 ============ */
QLabel#pageTitle {
    font-size: 20px; font-weight: 700; color: #0F172A;
}
QLabel#sectionTitle {
    font-size: 14px; font-weight: 600; color: #0F172A;
}
QLabel#fieldLabel {
    font-size: 12px; font-weight: 600; color: #334155;
}
QLabel#cardCaption {
    color: #64748B;
}
QLabel#brandName {
    font-size: 22px; font-weight: 800; color: #FFFFFF;
}
QLabel#brandSub {
    color: #BAE6FD; font-size: 12px;
}
QLabel#userChip {
    color: #334155; font-size: 12px; font-weight: 600;
    background: #F1F5F9; border: 1px solid #E2E8F0;
    border-radius: 14px; padding: 4px 12px;
}
QLabel#brandTitle {
    font-size: 17px; font-weight: 700; color: #0F172A; padding: 0 6px;
}
QLabel#hintMuted {
    color: #94A3B8; font-size: 12px;
}

/* ============ 品牌深色面板（登录页左栏） ============ */
QFrame#brandPanel {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #0F172A, stop:1 #1E3A5F);
    border: none;
}
QLabel#brandPanelTitle {
    color: #FFFFFF; font-size: 26px; font-weight: 800;
}
QLabel#brandPanelSub {
    color: #93C5FD; font-size: 14px;
}
QLabel#brandPanelDesc {
    color: #CBD5E1; font-size: 12px;
}

/* ============ 输入控件 ============ */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: #FFFFFF;
    border: 1px solid #CBD5E1;
    border-radius: 6px;
    padding: 7px 10px;
    selection-background-color: #BAE6FD;
    selection-color: #0F172A;
}
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
    border-color: #94A3B8;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border: 1px solid #0369A1;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
    background: #F1F5F9; color: #94A3B8;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 4px;
    selection-background-color: #E0F2FE; selection-color: #075985;
    outline: none;
}

/* ============ 按钮 ============ */
QPushButton {
    background: #FFFFFF;
    color: #1E293B;
    border: 1px solid #CBD5E1;
    border-radius: 6px;
    padding: 7px 16px;
    font-weight: 500;
}
QPushButton:hover {
    background: #F8FAFC; border-color: #94A3B8;
}
QPushButton:pressed {
    background: #E2E8F0;
}
QPushButton:focus {
    border: 1px solid #0369A1;
}
QPushButton:disabled {
    background: #F1F5F9; color: #9CA3AF; border-color: #E2E8F0;
}
QPushButton#btnPrimary {
    background: #0369A1; color: #FFFFFF; border: none; font-weight: 600;
}
QPushButton#btnPrimary:hover  { background: #0284C7; }
QPushButton#btnPrimary:pressed{ background: #075985; }
QPushButton#btnPrimary:disabled { background: #7DD3FC; color: #FFFFFF; }
QPushButton#btnDanger {
    background: #FFF7F7; color: #DC2626; border: 1px solid #FECACA;
}
QPushButton#btnDanger:hover  { background: #FEE2E2; border-color: #EF4444; }
QPushButton#btnDanger:pressed{ background: #FECACA; }
QPushButton#btnGhost {
    background: transparent; border: none; color: #475569;
}
QPushButton#btnGhost:hover  { color: #0369A1; background: #F1F5F9; }
QPushButton#btnGhost:pressed{ background: #E2E8F0; }
QPushButton#btnSuccess {
    background: #16A34A; color: #FFFFFF; border: none; font-weight: 600;
}
QPushButton#btnSuccess:hover  { background: #15803D; }
QPushButton#btnSuccess:pressed{ background: #166534; }

/* ============ 卡片 ============ */
QFrame#card {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 8px;
}
QFrame#statCard {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-left: 4px solid #CBD5E1;
    border-radius: 8px;
}
QFrame#statCardSky   { border-left: 4px solid #1D4ED8; }
QFrame#statCardGreen { border-left: 4px solid #16A34A; }
QFrame#statCardRed   { border-left: 4px solid #DC2626; }
QFrame#statCardAmber { border-left: 4px solid #D97706; }
QLabel#statValue {
    font-size: 26px; font-weight: 700; color: #0F172A;
}
QLabel#statCaption {
    color: #64748B; font-size: 12px;
}

/* ============ 表格 ============ */
QTableWidget, QTableView {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    border-radius: 6px;
    gridline-color: #EEF2F7;
    alternate-background-color: #F8FAFC;
    selection-background-color: #E0F2FE;
    selection-color: #075985;
    outline: none;
}
QTableWidget::item, QTableView::item {
    padding: 4px 6px;
}
QTableWidget::item:selected, QTableView::item:selected {
    background: #E0F2FE; color: #075985;
}
QHeaderView::section {
    background: #F1F5F9;
    color: #334155;
    font-weight: 600;
    border: none;
    border-right: 1px solid #E2E8F0;
    border-bottom: 1px solid #E2E8F0;
    padding: 7px 8px;
}
QTableCornerButton::section {
    background: #F1F5F9; border: none; border-right: 1px solid #E2E8F0;
}

/* ============ 页签 ============ */
QTabWidget::pane {
    border: none;
    background: #F8FAFC;
    top: -1px;
}
QTabBar::tab {
    background: transparent;
    color: #64748B;
    padding: 11px 22px;
    font-weight: 600;
    border-bottom: 3px solid transparent;
}
QTabBar::tab:hover { color: #0F172A; }
QTabBar::tab:selected {
    color: #0369A1;
    border-bottom: 3px solid #0369A1;
}

/* ============ 顶部导航与状态栏 ============ */
QToolBar {
    background: #FFFFFF;
    border: none;
    border-bottom: 1px solid #E2E8F0;
    padding: 8px 14px;
    spacing: 8px;
}
QStatusBar {
    background: #FFFFFF;
    border-top: 1px solid #E2E8F0;
    color: #475569;
}
QLabel#statusPill {
    font-size: 12px; font-weight: 600;
    padding: 3px 10px; border-radius: 10px;
}

/* ============ Dock / 日志 ============ */
QDockWidget {
    color: #0F172A;
    font-weight: 600;
}
QDockWidget::title {
    background: #F1F5F9;
    border-bottom: 1px solid #E2E8F0;
    padding: 7px 10px;
    text-align: left;
}
QPlainTextEdit#logView {
    background: #FFFFFF;
    border: none;
    font-family: "JetBrains Mono", "DejaVu Sans Mono", "Noto Sans Mono CJK SC", monospace;
    font-size: 12px;
    color: #334155;
}
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #CBD5E1; border-radius: 5px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #94A3B8; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #CBD5E1; border-radius: 5px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }

/* ============ 警示条 ============ */
QLabel#alarmBox {
    padding: 7px 10px;
    border-radius: 6px;
    font-weight: 600;
}
)QSS");
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
