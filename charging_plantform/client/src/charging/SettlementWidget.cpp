#include "SettlementWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMessageBox>

SettlementWidget::SettlementWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("订单结算"));
    setFixedWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_orderLabel = new QLabel(this);
    m_orderLabel->setWordWrap(true);
    m_orderLabel->setStyleSheet(QStringLiteral("background: #fff3e0; padding: 12px;"
                                               "border-radius: 6px; font-size: 14px;"));
    m_orderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_orderLabel);

    m_balanceLabel = new QLabel(this);
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_balanceLabel);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(m_noteLabel);

    m_settleBtn = new QPushButton(QStringLiteral("确认结算（余额支付）"), this);
    m_settleBtn->setFixedHeight(40);
    layout->addWidget(m_settleBtn);

    auto* btnRow = new QHBoxLayout;
    m_rechargeBtn = new QPushButton(QStringLiteral("去充值"), this);
    m_okBtn = new QPushButton(QStringLiteral("关闭"), this);
    btnRow->addWidget(m_rechargeBtn);
    btnRow->addWidget(m_okBtn);
    layout->addLayout(btnRow);

    connect(m_settleBtn, &QPushButton::clicked, this, &SettlementWidget::onSettleClicked);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &SettlementWidget::requestRecharge);
    connect(m_okBtn, &QPushButton::clicked, this, &SettlementWidget::accept);

    m_balanceLabel->setText(
        QStringLiteral("当前余额：¥%1").arg(AppSession::instance().balance(), 0, 'f', 2));
    connect(&AppSession::instance(), &AppSession::balanceChanged, this,
            [this](double b) {
                m_balanceLabel->setText(QStringLiteral("当前余额：¥%1").arg(b, 0, 'f', 2));
            });
}

void SettlementWidget::setBusy(bool busy) {
    m_settleBtn->setEnabled(!busy);
    m_settleBtn->setText(busy ? QStringLiteral("结算中…")
                              : QStringLiteral("确认结算（余额支付）"));
}

void SettlementWidget::setOrderText(const QString& text) {
    m_orderLabel->setText(text);
    m_noteLabel->setText(QStringLiteral("结算由服务端 ORDER_SETTLE 计算费用并扣减余额，"
                                        "客户端不参与业务判定。"));
}

void SettlementWidget::openWithOrder(int orderId) {
    m_orderId = orderId;
    if (orderId > 0) {
        setOrderText(QStringLiteral("您有未完成的充电订单\n订单号：#%1\n状态：充电中（待结算）")
                         .arg(orderId));
        m_settleBtn->setEnabled(true);
    } else {
        setOrderText(QStringLiteral("正在查询未完成订单…"));
        m_settleBtn->setEnabled(false);
        queryUnfinished();
    }
    show();
    raise();
    activateWindow();
}

void SettlementWidget::queryUnfinished() {
    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdOrderCheckUnfinished, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) {
                setOrderText(QStringLiteral("查询失败，请稍后重试。"));
                return;
            }
            if (!resp.value("has_unfinished").toBool()) {
                setOrderText(QStringLiteral("当前没有未完成的充电订单，可以正常充电。"));
                return;
            }
            m_orderId = resp.value("order_id").toInt();
            setOrderText(QStringLiteral("您有未完成的充电订单\n订单号：#%1\n状态：充电中（待结算）")
                             .arg(m_orderId));
            m_settleBtn->setEnabled(true);
        });
}

void SettlementWidget::onSettleClicked() {
    if (m_orderId <= 0) return;

    setBusy(true);
    m_noteLabel->setText(QStringLiteral("正在提交服务端结算…"));

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["order_id"] = m_orderId;

    NetClient::instance().sendRequest(Api::CmdOrderSettle, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_noteLabel->setText(msg);
                if (code == Api::StateConflict && msg.contains(QStringLiteral("余额不足"))) {
                    QMessageBox::information(this, QStringLiteral("余额不足"),
                                             msg + QStringLiteral("\n请先充值后再次结算。"));
                }
                return;
            }
            const double amount = resp.value("amount").toDouble();
            const double balance = resp.value("balance").toDouble();
            AppSession::instance().setBalance(balance);
            m_orderLabel->setText(QStringLiteral("结算完成\n订单号：#%1\n实付金额：¥%2")
                                      .arg(m_orderId)
                                      .arg(amount, 0, 'f', 2));
            m_noteLabel->setText(QStringLiteral("电桩已释放为【闲置】，余额已扣减。"));
            m_settleBtn->setEnabled(false);
            emit settled();
        });
}
