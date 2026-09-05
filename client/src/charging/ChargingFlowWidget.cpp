#include "ChargingFlowWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QFont>

ChargingFlowWidget::ChargingFlowWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("电动汽车充电"), this);
    QFont f = title->font();
    f.setPointSize(15);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    m_statusLabel = new QLabel(QStringLiteral("进入充电页后请先检查未完成订单"), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_pileLabel = new QLabel(QStringLiteral("未选择电桩"), this);
    m_pileLabel->setStyleSheet(QStringLiteral("background: #f5f5f5; padding: 8px;"
                                              "border: 1px solid #e0e0e0; border-radius: 6px;"));
    m_pileLabel->setWordWrap(true);
    layout->addWidget(m_pileLabel);

    auto* slotRow = new QHBoxLayout;
    slotRow->addWidget(new QLabel(QStringLiteral("预约时段"), this));
    m_timeSlotCombo = new QComboBox(this);
    m_timeSlotCombo->addItem(QStringLiteral("尽快（立即开始）"));
    m_timeSlotCombo->addItem(QStringLiteral("1小时后"));
    m_timeSlotCombo->addItem(QStringLiteral("2小时后"));
    m_timeSlotCombo->addItem(QStringLiteral("今天晚间（19:00-21:00）"));
    slotRow->addWidget(m_timeSlotCombo, 1);
    layout->addLayout(slotRow);

    m_checkBtn = new QPushButton(QStringLiteral("检查未完成订单"), this);
    layout->addWidget(m_checkBtn);

    m_reserveBtn = new QPushButton(QStringLiteral("预约并开始充电"), this);
    m_reserveBtn->setEnabled(false);
    m_reserveBtn->setFixedHeight(42);
    layout->addWidget(m_reserveBtn);

    m_settlementBtn = new QPushButton(QStringLiteral("我的充电订单 / 结算"), this);
    layout->addWidget(m_settlementBtn);

    m_goPickPileBtn = new QPushButton(QStringLiteral("去'找桩'页选择电桩"), this);
    layout->addWidget(m_goPickPileBtn);

    layout->addStretch(1);

    connect(m_checkBtn, &QPushButton::clicked, this, &ChargingFlowWidget::checkUnfinishedOrder);
    connect(m_reserveBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onReserveClicked);
    connect(m_settlementBtn, &QPushButton::clicked, this, &ChargingFlowWidget::checkUnfinishedOrder);
    connect(m_goPickPileBtn, &QPushButton::clicked, this, &ChargingFlowWidget::goPickPile);

    // 退出登录后清理选桩与状态，避免历史用户残留
    connect(&AppSession::instance(), &AppSession::loggedOut, this, [this]() {
        m_pendingPile = QJsonObject();
        m_reserveBtn->setEnabled(false);
        m_pileLabel->setText(QStringLiteral("未选择电桩"));
        m_statusLabel->clear();
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

    const QString pileDesc = QStringLiteral("已选电桩：%1（%2/%3）@ %4")
        .arg(pile.value("pile_id").toInt())
        .arg(pile.value("type").toString())
        .arg(pile.value("power").toDouble())
        .arg(pile.value("station_name").toString());
    m_pileLabel->setText(pileDesc);
    m_reserveBtn->setEnabled(true);
    setStatus(QStringLiteral("电桩已选择，请先确认无未完成订单，再点击“预约并开始充电”。"), true);
}

void ChargingFlowWidget::checkUnfinishedOrder() {
    if (!AppSession::instance().isLoggedIn()) {
        setStatus(QStringLiteral("请先登录"), false);
        return;
    }
    if (m_busy) return;
    m_busy = true;
    setStatus(QStringLiteral("正在检查未完成订单…"), true);

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdOrderCheckUnfinished, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("订单检测失败：%1").arg(msg), false);
                return;
            }
            bool has = resp.value("has_unfinished").toBool();
            if (has) {
                int orderId = resp.value("order_id").toInt();
                setStatus(QStringLiteral("您有未完成的充电订单（单号 %1），请先结算。")
                              .arg(orderId), false);
                m_reserveBtn->setEnabled(false);
                emit orderInterrupted(orderId);
            } else {
                setStatus(QStringLiteral("无未完成订单，可以开始新的充电。"), true);
                m_reserveBtn->setEnabled(!m_pendingPile.isEmpty());
            }
        });
}

void ChargingFlowWidget::onReserveClicked() {
    if (m_pendingPile.isEmpty()) {
        setStatus(QStringLiteral("请先在“找桩”页选择一个空闲电桩"), false);
        return;
    }
    if (!AppSession::instance().isLoggedIn()) return;
    if (m_busy) {
        setStatus(QStringLiteral("正在处理中，请稍候…"), false);
        return;
    }

    // 业务判定（未完成订单/电桩是否可用）由服务端 ORDER_RESERVE 权威处理，
    // 客户端只发送“预约意图”，冲突以服务端返回为准。
    doReserve();
}

void ChargingFlowWidget::doReserve() {
    const int pileId = m_pendingPile.value("pile_id").toInt();
    const QString timeSlot = m_timeSlotCombo->currentText();

    m_busy = true;
    m_reserveBtn->setEnabled(false);
    setStatus(QStringLiteral("正在预约电桩…"), true);

    QJsonObject reserveData;
    reserveData["user_id"] = AppSession::instance().userId();
    reserveData["pile_id"] = pileId;
    reserveData["time_slot"] = timeSlot;

    NetClient::instance().sendRequest(Api::CmdOrderReserve, reserveData,
        [this, pileId](const QJsonObject& /*resp*/, int code, const QString& msg) {
            if (code != 0) {
                m_busy = false;
                m_reserveBtn->setEnabled(true);
                setStatus(QStringLiteral("预约失败：%1").arg(msg), false);
                return;
            }
            setStatus(QStringLiteral("预约成功，正在生成充电订单…"), true);
            createOrder(pileId);
        });
}

// 需求10：预约成功后生成充电订单（上电/状态流转由服务端处理）
void ChargingFlowWidget::createOrder(int pileId) {
    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["pile_id"] = pileId;

    NetClient::instance().sendRequest(Api::CmdOrderCreate, data,
        [this, pileId](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("生成订单失败：%1").arg(msg), false);
                m_reserveBtn->setEnabled(true);
                return;
            }
            int orderId = resp.value("order_id").toInt();
            setStatus(QStringLiteral("订单已生成（单号 %1），电桩开始充电，当前为【待结算】状态。"
                                     "充电完成后请到“结算”页面完成支付结算。").arg(orderId), true);
            m_reserveBtn->setEnabled(false);
        });
}
