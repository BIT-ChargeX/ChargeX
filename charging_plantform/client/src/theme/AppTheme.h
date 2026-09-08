#pragma once
#include <QString>

// 客户端全局主题样式（QSS），在 main() 中通过 app.setStyleSheet 一次性应用。
// 配色：新能源绿 + 浅灰白，保持登录 / 弹窗 / 主页视觉统一。
namespace AppTheme {

inline QString styleSheet() {
    return QStringLiteral(R"QSS(
/* ===================== 基础 ===================== */
QWidget {
    color: #22303c;
    font-family: "Microsoft YaHei UI", "PingFang SC", "Noto Sans CJK SC", "Segoe UI", sans-serif;
}

QMainWindow, QDialog {
    background: #f5f7fa;
}

/* 根容器：登录页与主页共用的 QStackedWidget */
QWidget#appRoot {
    background: #f5f7fa;
}

/* ===================== 按钮 ===================== */
QPushButton {
    background: #00b578;
    color: #ffffff;
    border: none;
    border-radius: 8px;
    padding: 8px 16px;
    font-weight: 500;
}
QPushButton:hover { background: #00c88a; }
QPushButton:pressed { background: #009a66; }
QPushButton:disabled { background: #c8d5cf; color: #ffffff; }
QPushButton:focus { outline: none; }

/* 主 CTA（登录/注册/确认等） */
QPushButton#primaryBtn {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00c88a, stop:1 #00b578);
    color: #ffffff;
    border: none;
    border-radius: 10px;
    padding: 10px 18px;
    font-size: 15px;
    font-weight: 600;
}
QPushButton#primaryBtn:hover { background: #00c88a; }
QPushButton#primaryBtn:pressed { background: #009a66; }
QPushButton#primaryBtn:disabled { background: #c8d5cf; color: #ffffff; }

/* 次要按钮（取消 / 关闭 / 刷新 / 发送验证码等） */
QPushButton#secondaryBtn {
    background: #ffffff;
    color: #00b578;
    border: 1px solid #7fd6b3;
    border-radius: 8px;
    padding: 8px 16px;
    font-weight: 500;
}
QPushButton#secondaryBtn:hover { background: #eafff5; }
QPushButton#secondaryBtn:pressed { background: #d9f5e8; }
QPushButton#secondaryBtn:disabled { color: #b9c4c0; border-color: #d5ddd9; background: #f5f7fa; }

/* 警示按钮（退出登录等危险操作） */
QPushButton#dangerBtn {
    background: #ffffff;
    color: #e5484d;
    border: 1px solid #f3b7b9;
    border-radius: 8px;
    padding: 8px 16px;
    font-weight: 500;
}
QPushButton#dangerBtn:hover { background: #fff0f0; }
QPushButton#dangerBtn:pressed { background: #ffe3e4; }

/* 警示色 CTA（去结算） */
QPushButton#warningBtn {
    background: #fff4e5;
    color: #e67e22;
    border: 1px solid #ffd9a0;
    border-radius: 8px;
    padding: 8px 16px;
    font-weight: 600;
}
QPushButton#warningBtn:hover { background: #ffe9cf; }
QPushButton#warningBtn:pressed { background: #ffdbb0; }

/* 链接式按钮 */
QPushButton#linkBtn {
    background: transparent;
    border: none;
    color: #00b578;
    padding: 0;
    font-weight: 500;
}
QPushButton#linkBtn:hover { color: #009a66; text-decoration: underline; }

QPushButton#linkBtnGray {
    background: transparent;
    border: none;
    color: #8a94a6;
    padding: 0;
    font-weight: 400;
}
QPushButton#linkBtnGray:hover { color: #5b6472; text-decoration: underline; }

/* 列表行式按钮（我的页“碳积分 / 我的订单”） */
QPushButton#rowBtn {
    background: #ffffff;
    color: #22303c;
    border: 1px solid #e5e9f0;
    border-radius: 10px;
    padding: 12px 14px;
    text-align: left;
    font-weight: 500;
}
QPushButton#rowBtn:hover { border-color: #7fd6b3; background: #f6fdf9; }

/* ===================== 输入框 / 下拉 / 数字框 ===================== */
QLineEdit {
    background: #ffffff;
    border: 1px solid #dde3ea;
    border-radius: 8px;
    padding: 0 12px;
    min-height: 20px;
    selection-background-color: #00b578;
    selection-color: #ffffff;
}
QLineEdit:hover { border-color: #c2ccd6; }
QLineEdit:focus { border-color: #00b578; }
QLineEdit:disabled { background: #f2f4f7; color: #9aa3af; }

QComboBox {
    background: #ffffff;
    border: 1px solid #dde3ea;
    border-radius: 8px;
    padding: 0 12px;
    min-height: 20px;
}
QComboBox:hover { border-color: #c2ccd6; }
QComboBox:focus { border-color: #00b578; }
QComboBox::drop-down {
    border: none;
    border-left: 1px solid #eef1f5;
    width: 26px;
}
QComboBox QAbstractItemView {
    background: #ffffff;
    border: 1px solid #dde3ea;
    border-radius: 8px;
    selection-background-color: #e6f9f0;
    selection-color: #0f3d2e;
    outline: none;
}

QAbstractSpinBox {
    background: #ffffff;
    border: 1px solid #dde3ea;
    border-radius: 8px;
    padding: 0 10px;
    min-height: 20px;
}
QAbstractSpinBox:hover { border-color: #c2ccd6; }
QAbstractSpinBox:focus { border-color: #00b578; }
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    border: none;
    background: transparent;
    width: 18px;
}

/* ===================== 表格 ===================== */
QTableView, QTableWidget {
    background: #ffffff;
    alternate-background-color: #f8fafc;
    border: 1px solid #e5e9f0;
    border-radius: 10px;
    gridline-color: #eef1f5;
    selection-background-color: #d9f5e8;
    selection-color: #0f3d2e;
    outline: none;
}
QTableView::item, QTableWidget::item {
    padding: 6px 8px;
    border: none;
}
QTableView::item:selected, QTableWidget::item:selected {
    background: #d9f5e8;
    color: #0f3d2e;
}
QHeaderView::section {
    background: #f4f7fa;
    color: #4a5560;
    padding: 8px;
    border: none;
    border-bottom: 1px solid #e5e9f0;
    font-weight: 600;
}
QTableCornerButton::section {
    background: #f4f7fa;
    border: none;
}

/* ===================== 列表 ===================== */
QListWidget {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget::item {
    border: none;
}

/* ===================== 标签页 ===================== */
QTabWidget::pane {
    border: none;
    background: #f5f7fa;
}
QTabBar {
    background: #ffffff;
}
QTabBar::tab {
    background: transparent;
    color: #6b7280;
    padding: 12px 22px;
    margin: 4px 4px 0 4px;
    border: none;
    font-size: 14px;
}
QTabBar::tab:hover { color: #00b578; }
QTabBar::tab:selected {
    color: #00b578;
    font-weight: 600;
    border-bottom: 3px solid #00b578;
}

/* ===================== 滚动条 ===================== */
QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #c7d0d8;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #a9b6c0; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: none; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

QScrollBar:horizontal {
    background: transparent;
    height: 8px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: #c7d0d8;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #a9b6c0; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; background: none; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

/* ===================== 提示气泡 / 消息框 ===================== */
QToolTip {
    background: #22303c;
    color: #ffffff;
    border: none;
    padding: 6px 10px;
    border-radius: 4px;
}
QMessageBox { background: #ffffff; }

/* ===================== 业务组件 ===================== */
/* 登录页 */
QWidget#loginPage {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #eafff5, stop:1 #f5f7fa);
}
QLabel#loginHero {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #00b578, stop:0.55 #00c88a, stop:1 #00d09c);
    color: #ffffff;
    font-size: 24px;
    font-weight: 700;
    border-bottom-left-radius: 24px;
    border-bottom-right-radius: 24px;
}
QFrame#loginCard {
    background: #ffffff;
    border: 1px solid #e8edf2;
    border-radius: 16px;
}
QLabel#loginSubtitle {
    color: #22303c;
    font-size: 15px;
    font-weight: 600;
}

/* 选桩信息卡 */
QLabel#infoCard {
    background: #f0f9f4;
    color: #1c3a2d;
    border: 1px solid #cdeedd;
    border-radius: 10px;
    padding: 14px;
}
)QSS");
}

} // namespace AppTheme
