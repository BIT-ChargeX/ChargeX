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
#include <QMessageBox>

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
    m_pileLabel->setWordWrap(true);
    m_pileLabel->setStyleSheet(QStringLiteral("background: #eef4ff; padding: 12px;"
                                              "border: 1px solid #cfe0ff; border-radius: 6px;"));
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

    m_reserveBtn = new QPushButton(QStringLiteral("预约并开始充电"), this);
    m_reserveBtn->setEnabled(false);
    m_reserveBtn->setFixedHeight(46);
    m_reserveBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2e7d32; color: white; border-radius: 6px;"
        " font-size: 15px; font-weight: bold; }"
        "QPushButton:disabled { background: #bdbdbd; }"));
    layout->addWidget(m_reserveBtn);

    m_settleBtn = new QPushButton(this);
    m_settleBtn->setFixedHeight(40);
    m_settleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #fff3e0; color: #c62828; border: 1px solid #ffcc80;"
        " border-radius: 6px; font-weight: bold; }"));
    m_settleBtn->hide();
    layout->addWidget(m_settleBtn);

    m_goPickPileBtn = new QPushButton(QStringLiteral("去「找桩」页选择电桩"), this);
    layout->addWidget(m_goPickPileBtn);

    layout->addStretch(1);

    connect(m_reserveBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onReserveClicked);
    connect(m_settleBtn, &QPushButton::clicked, this, &ChargingFlowWidget::onSettleClicked);
    connect(m_goPickPileBtn, &QPushButton::clicked, this, &ChargingFlowWidget::goPickPile);

    // 退出登录后清理选桩与状态，避免历史用户残留
    connect(&AppSession::instance(), &AppSession::loggedOut, this, [this]() {
        m_pendingPile = QJsonObject();
        m_unfinishedOrderId = 0;
        m_reserveBtn->setEnabled(false);
        m_settleBtn->hide();
        m_pileLabel->setText(QStringLiteral("尚未选择电桩"));
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

    const QString pileDesc = QStringLiteral("已选电桩：编号 %1（%2 · %3kW）\n站点：%4")
        .arg(pile.value("pile_id").toInt())
        .arg(pile.value("type").toString())
        .arg(pile.value("power").toDouble())
        .arg(pile.value("station_name").toString());
    m_pileLabel->setText(pileDesc);
    m_reserveBtn->setEnabled(true);
    setStatus(QStringLiteral("电桩已选好，确认时段后点击「预约并开始充电」。"), true);
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
            const bool has = resp.value("has_unfinished").toBool();
            if (has) {
                m_unfinishedOrderId = resp.value("order_id").toInt();
                setStatus(QStringLiteral("您有未完成的充电订单（单号 #%1），请先结算后再开始新的充电。")
                              .arg(m_unfinishedOrderId), false);
                m_reserveBtn->setEnabled(!m_pendingPile.isEmpty());
                m_settleBtn->setText(QStringLiteral("结算"));
                m_settleBtn->show();
            } else {
                m_unfinishedOrderId = 0;
                m_settleBtn->hide();
                setStatus(QStringLiteral("无未完成订单，可以开始新的充电。"), true);
                m_reserveBtn->setEnabled(!m_pendingPile.isEmpty());
            }
        });
}

void ChargingFlowWidget::onSettleClicked() {
    if (m_unfinishedOrderId <= 0) return;
    emit settleRequested(m_unfinishedOrderId);
}

void ChargingFlowWidget::onReserveClicked() {
    if (m_pendingPile.isEmpty()) {
        setStatus(QStringLiteral("请先在「找桩」页选择一个空闲电桩"), false);
        return;
    }
    if (!AppSession::instance().isLoggedIn()) return;

    // 有未结算订单时点击"开始充电"：弹窗提醒，引导先结算
    if (m_unfinishedOrderId > 0) {
        QMessageBox box(QMessageBox::Warning, QStringLiteral("有未结算订单"),
            QStringLiteral("您有一笔未结算的充电订单（单号 #%1），请先结算后再开始新的充电。")
                .arg(m_unfinishedOrderId),
            QMessageBox::NoButton, this);
        QPushButton* settleBtn = box.addButton(QStringLiteral("去结算"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == settleBtn) {
            emit settleRequested(m_unfinishedOrderId);
        }
        return;
    }

    if (m_busy) {
        setStatus(QStringLiteral("正在处理中，请稍候…"), false);
        return;
    }

    // 业务判定（未完成订单/电桩是否可用）由服务端 ORDER_RESERVE 权威处理，
    // 客户端只发送"预约意图"，冲突以服务端返回为准。
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
        [this](const QJsonObject& resp, int code, const QString& msg) {
            m_busy = false;
            if (code != 0) {
                setStatus(QStringLiteral("生成订单失败：%1").arg(msg), false);
                m_reserveBtn->setEnabled(true);
                return;
            }
            const int orderId = resp.value("order_id").toInt();
            m_unfinishedOrderId = orderId;
            m_reserveBtn->setEnabled(false);
            setStatus(QStringLiteral("订单已生成（单号 #%1），电桩开始充电，当前为【待结算】状态。"
                                     "充电完成后点击下方按钮完成结算。").arg(orderId), true);
            m_settleBtn->setText(QStringLiteral("结算"));
            m_settleBtn->show();
        });
}
