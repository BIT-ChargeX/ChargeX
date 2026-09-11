#include "SettlementWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
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

    // 优惠券选择：下拉列出当前可用优惠券，选择后实时刷新预估抵扣
    m_couponCombo = new QComboBox(this);
    m_couponCombo->addItem(QStringLiteral("不使用优惠券"), 0);
    m_couponCombo->setEnabled(false);
    layout->addWidget(m_couponCombo);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(m_noteLabel);

    m_settleBtn = new QPushButton(QStringLiteral("确认结算（余额支付）"), this);
    m_settleBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_settleBtn->setFixedHeight(40);
    layout->addWidget(m_settleBtn);

    auto* btnRow = new QHBoxLayout;
    m_rechargeBtn = new QPushButton(QStringLiteral("去充值"), this);
    m_rechargeBtn->setObjectName(QStringLiteral("secondaryBtn"));
    m_okBtn = new QPushButton(QStringLiteral("关闭"), this);
    m_okBtn->setObjectName(QStringLiteral("secondaryBtn"));
    btnRow->addWidget(m_rechargeBtn);
    btnRow->addWidget(m_okBtn);
    layout->addLayout(btnRow);

    connect(m_settleBtn, &QPushButton::clicked, this, &SettlementWidget::onSettleClicked);
    connect(m_rechargeBtn, &QPushButton::clicked, this, &SettlementWidget::requestRecharge);
    connect(m_okBtn, &QPushButton::clicked, this, &SettlementWidget::accept);
    connect(m_couponCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettlementWidget::onCouponChanged);

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
<<<<<<< Updated upstream
=======
    m_estAmount = -1.0;
    m_selectedCouponId = 0;
    m_discount = 0.0;
    m_payAmount = 0.0;
    m_estimateLabel->hide();
    refreshCoupons();
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
=======
// 拉取本次结算的预计扣费（服务端只读计算，不扣费），成功后刷新预估卡
void SettlementWidget::queryPreview() {
    if (m_orderId <= 0 || !AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["order_id"] = m_orderId;
    data["redeem_id"] = m_selectedCouponId;

    NetClient::instance().sendRequest(Api::CmdOrderSettlePreview, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_estAmount = -1.0;
                m_discount = 0.0;
                m_payAmount = 0.0;
                m_estimateLabel->setStyleSheet(QString::fromLatin1(kEstStyleWarn));
                m_estimateLabel->setText(QStringLiteral("预估金额查询失败：%1").arg(msg));
                m_estimateLabel->show();
                return;
            }
            m_estAmount = resp.value("amount").toDouble();
            m_discount = resp.value("discount").toDouble();
            m_payAmount = resp.value("pay_amount").toDouble();
            updateEstimateDisplay();
            m_estimateLabel->show();
        });
}

void SettlementWidget::updateEstimateDisplay() {
    if (m_estAmount < 0.0) return;

    if (m_estAmount <= 0.0) {
        m_estimateLabel->setStyleSheet(QString::fromLatin1(kEstStyleNormal));
        m_estimateLabel->setText(QStringLiteral("该订单为预约单，结算不产生费用（0 元）"));
        return;
    }

    const double balance = AppSession::instance().balance();
    const double remaining = balance - m_payAmount;
    const bool insufficient = remaining < -1e-9;

    m_estimateLabel->setStyleSheet(QString::fromLatin1(
        insufficient ? kEstStyleWarn : kEstStyleNormal));
    QString text = QStringLiteral("本次预计扣费：¥%1").arg(m_estAmount, 0, 'f', 2);
    if (m_discount > 0.0) {
        text += QStringLiteral("（优惠券抵扣 ¥%1）").arg(m_discount, 0, 'f', 2);
        text += QStringLiteral("\n实付：¥%1").arg(m_payAmount, 0, 'f', 2);
    }
    text += QStringLiteral("\n预计剩余余额：¥%1").arg(remaining, 0, 'f', 2);
    if (insufficient) text += QStringLiteral("\n余额不足，请先充值");
    m_estimateLabel->setText(text);
}

void SettlementWidget::refreshCoupons() {
    if (!AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdUserCoupons, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            m_couponCombo->blockSignals(true);
            m_couponCombo->clear();
            m_couponCombo->addItem(QStringLiteral("不使用优惠券"), 0);
            if (code == 0) {
                const QJsonArray coupons = resp.value("coupons").toArray();
                for (const auto& v : coupons) {
                    const QJsonObject c = v.toObject();
                    if (c.value("used").toInt() != 0) continue;   // 只列可用
                    m_couponCombo->addItem(
                        QStringLiteral("%1（满%2减%3）")
                            .arg(c.value("item_name").toString())
                            .arg(c.value("threshold").toDouble(), 0, 'f', 2)
                            .arg(c.value("value").toDouble(), 0, 'f', 2),
                        c.value("redeem_id").toInt());
                }
            }
            m_couponCombo->setCurrentIndex(0);
            m_couponCombo->setEnabled(m_couponCombo->count() > 1);
            m_couponCombo->blockSignals(false);
        });
}

void SettlementWidget::onCouponChanged(int /*index*/) {
    m_selectedCouponId = m_couponCombo->currentData().toInt();
    queryPreview();   // 重新预估，反映优惠券抵扣
}

>>>>>>> Stashed changes
void SettlementWidget::onSettleClicked() {
    if (m_orderId <= 0) return;

    setBusy(true);
    m_noteLabel->setText(QStringLiteral("正在提交服务端结算…"));

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["order_id"] = m_orderId;
    data["redeem_id"] = m_selectedCouponId;

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
            const double amount = resp.value("amount").toDouble();     // 实付
            const double discount = resp.value("discount").toDouble();
            const double balance = resp.value("balance").toDouble();
            AppSession::instance().setBalance(balance);
            QString doneText = QStringLiteral("结算完成\n订单号：#%1\n实付金额：¥%2")
                                   .arg(m_orderId)
                                   .arg(amount, 0, 'f', 2);
            if (discount > 0.0)
                doneText += QStringLiteral("\n（优惠券抵扣 ¥%1）").arg(discount, 0, 'f', 2);
            m_orderLabel->setText(doneText);
            m_noteLabel->setText(QStringLiteral("电桩已释放为【闲置】，余额已扣减。"));
            m_settleBtn->setEnabled(false);
            m_selectedCouponId = 0;
            refreshCoupons();   // 优惠券已用，刷新下拉列表
            emit settled();
        });
}
