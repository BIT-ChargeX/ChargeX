#include "ChargingFlowWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QFont>
#include <QTimer>

ChargingFlowWidget::ChargingFlowWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(28, 24, 28, 24);

    auto* title = new QLabel(QStringLiteral("电动汽车充电"), this);
    QFont f = title->font();
    f.setPointSize(16);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_statusLabel = new QLabel(QStringLiteral("进入充电页后会自动检测未完成订单"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_pileLabel = new QLabel(QStringLiteral("尚未选择电桩"), this);
    m_pileLabel->setObjectName(QStringLiteral("infoCard"));
    m_pileLabel->setWordWrap(true);
    layout->addWidget(m_pileLabel);

    // 预约到指定时间（可选）
    m_scheduleCheck = new QCheckBox(QStringLiteral("预约到指定时间"), this);
    m_scheduleCheck->setChecked(false);
    layout->addWidget(m_scheduleCheck);

    m_timeEdit = new QDateTimeEdit(this);
    m_timeEdit->setCalendarPopup(true);
    m_timeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_timeEdit->setMinimumDateTime(QDateTime::currentDateTime());
    m_timeEdit->setMaximumDateTime(QDateTime::currentDateTime().addDays(1));
    m_timeEdit->setDateTime(QDateTime::currentDateTime().addSecs(3600));
    m_timeEdit->setEnabled(false);
    layout->addWidget(m_timeEdit);

    m_actionBtn = new QPushButton(QStringLiteral("立即开始充电"), this);
    m_actionBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_actionBtn->setEnabled(false);
    m_actionBtn->setFixedHeight(46);
    layout->addWidget(m_actionBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("取消预约"), this);
    m_cancelBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_cancelBtn->setFixedHeight(40);
    m_cancelBtn->hide();
    layout->addWidget(m_cancelBtn);

    m_settleBtn = new QPushButton(this);
    m_settleBtn->setObjectName(QStringLiteral("warningBtn"));
    m_settleBtn->setFixedHeight(40);
    m_settleBtn->hide();
    layout->addWidget(m_settleBtn);

    m_goPickPileBtn = new QPushButton(QStringLiteral("去「找桩」页选择电桩"), this);
    m_goPickPileBtn->setObjectName(QStringLiteral("secondaryBtn"));
    layout->addWidget(m_goPickPileBtn);

    layout->addStretch(1);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setSingleShot(true);
    connect(m_countdownTimer, &QTimer::timeout, this, &ChargingFlowWidget::render);

    connect(m_actionBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onActionClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onCancelClicked);
    connect(m_settleBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onSettleClicked);
    connect(m_goPickPileBtn, &QPushButton::clicked, this, &ChargingFlowWidget::goPickPile);
    connect(m_scheduleCheck, &QCheckBox::toggled, this, &ChargingFlowWidget::onScheduleToggled);

    // 退出登录后清理选桩与状态，避免历史用户残留
    connect(&AppSession::instance(), &AppSession::loggedOut, this, [this]() {
        m_pendingPile = QJsonObject();
        m_unfinishedOrderId = 0;
        m_unfinishedPileId = 0;
        m_unfinishedStatus.clear();
        m_reserveTime = QDateTime();
        m_countdownTimer->stop();
        m_pileLabel->setText(QStringLiteral("尚未选择电桩"));
        m_statusLabel->clear();
        render();
    });
}

void ChargingFlowWidget::goPickPile() {
    emit goPickPileRequested();
}

void ChargingFlowWidget::setStatus(const QString& text, bool ok) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok
        ? QStringLiteral("color: #2e7d32;")
        : QStringLiteral("color: #c62828;"));
}

void ChargingFlowWidget::onTabEntered() {
    if (!AppSession::instance().isLoggedIn()) return;
    checkUnfinishedOrder();
}

void ChargingFlowWidget::startChargingWithPile(const QJsonObject& pile) {
    m_pendingPile = pile;

    const QString pileDesc = QStringLiteral("已选电桩：编号 %1（%2 · %3kW）\n站点：%4")
        .arg(pile.value("pile_id").toInt())
        .arg(pile.value("type").toString())
        .arg(pile.value("power").toDouble())
        .arg(pile.value("station_name").toString());
    m_pileLabel->setText(pileDesc);
    render();
}

void ChargingFlowWidget::onScheduleToggled(bool checked) {
    m_timeEdit->setEnabled(checked);
    // 切换后刷新主按钮文案（立即开始 / 提交预约）
    render();
}

// 统一刷新界面：根据「是否有未完成订单 + 是否预约占用 + 是否到点」决定按钮与提示
void ChargingFlowWidget::render() {
    const bool loggedIn = AppSession::instance().isLoggedIn();
    if (!loggedIn) {
        m_scheduleCheck->setEnabled(false);
        m_timeEdit->setEnabled(false);
        m_actionBtn->setEnabled(false);
        m_cancelBtn->hide();
        m_settleBtn->hide();
        m_goPickPileBtn->hide();
        return;
    }

    const bool hasUnfinished = m_unfinishedOrderId > 0;
    const bool isReserved = (m_unfinishedStatus == QStringLiteral("预约占用"));
    const bool timeUp = isReserved && m_reserveTime.isValid()
                        && m_reserveTime <= QDateTime::currentDateTime();

    // 时间选择器仅在"无未完成订单"时可用
    m_scheduleCheck->setEnabled(!hasUnfinished);
    m_timeEdit->setEnabled(!hasUnfinished && m_scheduleCheck->isChecked());

    m_settleBtn->hide();
    m_cancelBtn->hide();
    m_goPickPileBtn->hide();
    m_actionBtn->hide();

    // 预约占用
    if (hasUnfinished && isReserved) {
        m_actionBtn->show();
        m_cancelBtn->show();
        if (timeUp) {
            m_actionBtn->setText(QStringLiteral("开始充电"));
            m_actionBtn->setEnabled(true);
            setStatus(QStringLiteral("预约时间已到，点击「开始充电」。"), true);
        } else {
            m_actionBtn->setText(QStringLiteral("开始充电"));
            m_actionBtn->setEnabled(false);
            setStatus(QStringLiteral("已预约，将于 %1 开始充电（单号 #%2）。")
                          .arg(m_reserveTime.toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                          .arg(m_unfinishedOrderId), true);
            const qint64 ms = QDateTime::currentDateTime().msecsTo(m_reserveTime);
            if (ms > 0) m_countdownTimer->start(static_cast<int>(ms));
        }
        return;
    }

    // 充电中 / 待结算
    if (hasUnfinished) {
        m_settleBtn->setText(QStringLiteral("结算"));
        m_settleBtn->show();
        setStatus(QStringLiteral("您有未完成的充电订单（单号 #%1），充电完成后点击「结算」。")
                      .arg(m_unfinishedOrderId), false);
        return;
    }

    // 无未完成订单
    const bool hasPile = !m_pendingPile.isEmpty();
    m_goPickPileBtn->setVisible(!hasPile);
    m_actionBtn->show();
    m_actionBtn->setEnabled(hasPile);
    m_actionBtn->setText(m_scheduleCheck->isChecked()
        ? QStringLiteral("提交预约") : QStringLiteral("立即开始充电"));
    if (hasPile) {
        setStatus(m_scheduleCheck->isChecked()
            ? QStringLiteral("电桩已选好，选择预约时间后点击「提交预约」。")
            : QStringLiteral("电桩已选好，点击「立即开始充电」。"), true);
    } else {
        setStatus(QStringLiteral("请先在「找桩」页选择一个空闲电桩"), false);
    }
}

void ChargingFlowWidget::onActionClicked() {
    if (m_busy) return;
    if (!AppSession::instance().isLoggedIn()) return;

    // 预约占用已到点 → 开始充电
    if (m_unfinishedOrderId > 0 && m_unfinishedStatus == QStringLiteral("预约占用")) {
        if (m_unfinishedPileId > 0) createOrder(m_unfinishedPileId);
        else checkUnfinishedOrder();
        return;
    }

    // 未选桩
    if (m_pendingPile.isEmpty()) {
        setStatus(QStringLiteral("请先在「找桩」页选择一个空闲电桩"), false);
        return;
    }

    doReserve();
}

void ChargingFlowWidget::doReserve() {
    const int pileId = m_pendingPile.value("pile_id").toInt();
    m_busy = true;
    m_actionBtn->setEnabled(false);
    setStatus(QStringLiteral("正在提交预约…"), true);

    QJsonObject d;
    d["user_id"] = AppSession::instance().userId();
    d["pile_id"] = pileId;
    if (m_scheduleCheck->isChecked()) {
        d["reserve_time"] = m_timeEdit->dateTime().toString(Qt::ISODate);
    }

    NetClient::instance().sendRequest(Api::CmdOrderReserve, d,
        [this, pileId](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("预约失败：%1").arg(msg), false);
                render();
                return;
            }
            const int orderId = resp.value("reservation_id").toInt();
            const bool immediate = resp.value("is_immediate").toBool(true);
            if (immediate) {
                createOrder(pileId);
            } else {
                m_unfinishedOrderId = orderId;
                m_unfinishedPileId = pileId;
                m_unfinishedStatus = QStringLiteral("预约占用");
                m_reserveTime = QDateTime::fromString(resp.value("reserve_time").toString(), Qt::ISODate);
                render();
            }
        });
}

void ChargingFlowWidget::createOrder(int pileId) {
    m_busy = true;
    m_actionBtn->setEnabled(false);
    setStatus(QStringLiteral("正在生成充电订单…"), true);

    QJsonObject d;
    d["user_id"] = AppSession::instance().userId();
    d["pile_id"] = pileId;

    NetClient::instance().sendRequest(Api::CmdOrderCreate, d,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("开始充电失败：%1").arg(msg), false);
                checkUnfinishedOrder();
                return;
            }
            m_unfinishedOrderId = resp.value("order_id").toInt();
            m_unfinishedStatus = QStringLiteral("充电中");
            m_reserveTime = QDateTime();
            render();
        });
}

void ChargingFlowWidget::cancelReservation() {
    if (m_unfinishedOrderId <= 0) return;
    if (m_unfinishedStatus != QStringLiteral("预约占用")) return;

    m_busy = true;
    m_cancelBtn->setEnabled(false);
    setStatus(QStringLiteral("正在取消预约…"), true);

    QJsonObject d;
    d["user_id"] = AppSession::instance().userId();
    d["order_id"] = m_unfinishedOrderId;
    // 复用 ORDER_SETTLE：服务端对「预约占用」订单走"取消预约"分支（0 费用、释放电桩）
    NetClient::instance().sendRequest(Api::CmdOrderSettle, d,
        [this](const QJsonObject&, int code, const QString& msg) {
            m_busy = false;
            m_cancelBtn->setEnabled(true);
            if (code != 0) {
                setStatus(QStringLiteral("取消预约失败：%1").arg(msg), false);
                return;
            }
            m_unfinishedOrderId = 0;
            m_unfinishedPileId = 0;
            m_unfinishedStatus.clear();
            m_reserveTime = QDateTime();
            m_pendingPile = QJsonObject();
            m_pileLabel->setText(QStringLiteral("尚未选择电桩"));
            setStatus(QStringLiteral("预约已取消。"), true);
            render();
        });
}

void ChargingFlowWidget::checkUnfinishedOrder() {
    if (!AppSession::instance().isLoggedIn()) {
        setStatus(QStringLiteral("请先登录"), false);
        return;
    }
    if (m_busy) return;
    m_busy = true;
    setStatus(QStringLiteral("正在检查未完成订单…"), true);

    QJsonObject d;
    d["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdOrderCheckUnfinished, d,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("订单检测失败：%1").arg(msg), false);
                return;
            }
            if (resp.value("has_unfinished").toBool()) {
                m_unfinishedOrderId = resp.value("order_id").toInt();
                m_unfinishedPileId = resp.value("pile_id").toInt();
                m_unfinishedStatus = resp.value("status").toString();
                m_reserveTime = QDateTime::fromString(resp.value("reserve_time").toString(), Qt::ISODate);
            } else {
                m_unfinishedOrderId = 0;
                m_unfinishedPileId = 0;
                m_unfinishedStatus.clear();
                m_reserveTime = QDateTime();
            }
            render();
        });
}

void ChargingFlowWidget::onSettleClicked() {
    if (m_unfinishedOrderId <= 0) return;
    emit settleRequested(m_unfinishedOrderId);
}

void ChargingFlowWidget::onCancelClicked() {
    cancelReservation();
}
