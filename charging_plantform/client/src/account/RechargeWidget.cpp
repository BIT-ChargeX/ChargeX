#include "RechargeWidget.h"
#include "RechargeRecordsWidget.h"
#include "common/AppSession.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMessageBox>

namespace {
// 单笔充值限额（与服务端 sys_config.recharge_limit 默认值一致，服务端仍为最终校验）
constexpr double kRechargeLimit = 5000.0;
}

RechargeWidget::RechargeWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("余额充值"));
    setFixedWidth(340);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_balanceLabel = new QLabel(this);
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_balanceLabel);

    auto* amountRow = new QHBoxLayout;
    amountRow->addWidget(new QLabel(QStringLiteral("充值金额"), this));
    m_amountSpin = new QDoubleSpinBox(this);
    m_amountSpin->setRange(1.0, kRechargeLimit);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setSingleStep(10.0);
    m_amountSpin->setSuffix(QStringLiteral(" 元"));
    m_amountSpin->setValue(50.0);
    amountRow->addWidget(m_amountSpin, 1);
    layout->addLayout(amountRow);

    auto* tip = new QLabel(QStringLiteral("演示项目：点击确认即视为支付成功\n单笔限额 ¥%1")
                               .arg(kRechargeLimit, 0, 'f', 2),
                           this);
    tip->setStyleSheet(QStringLiteral("color: #888;"));
    tip->setAlignment(Qt::AlignCenter);
    layout->addWidget(tip);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_confirmBtn = new QPushButton(QStringLiteral("确认充值（模拟支付）"), this);
    m_confirmBtn->setFixedHeight(40);
    layout->addWidget(m_confirmBtn);

    m_recordsBtn = new QPushButton(QStringLiteral("查看充值记录"), this);
    m_recordsBtn->setFixedHeight(36);
    layout->addWidget(m_recordsBtn);

    m_recordsWidget = new RechargeRecordsWidget(this);

    connect(m_confirmBtn, &QPushButton::clicked, this, &RechargeWidget::onConfirmClicked);
    connect(m_recordsBtn, &QPushButton::clicked, this, &RechargeWidget::onShowRecords);

    connect(&AppSession::instance(), &AppSession::balanceChanged, this,
            [this](double balance) {
                m_balanceLabel->setText(
                    QStringLiteral("当前余额：¥%1").arg(balance, 0, 'f', 2));
            });
    onBalanceChanged();
}

void RechargeWidget::onBalanceChanged() {
    m_balanceLabel->setText(
        QStringLiteral("当前余额：¥%1").arg(AppSession::instance().balance(), 0, 'f', 2));
}

void RechargeWidget::onShowRecords() {
    if (!AppSession::instance().isLoggedIn()) return;
    m_recordsWidget->refresh();
    m_recordsWidget->show();
    m_recordsWidget->raise();
    m_recordsWidget->activateWindow();
}

void RechargeWidget::onConfirmClicked() {
    if (!AppSession::instance().isLoggedIn()) return;

    const double amount = m_amountSpin->value();
    if (amount <= 0) {
        m_hintLabel->setText(QStringLiteral("充值金额必须大于 0"));
        return;
    }
    if (amount > kRechargeLimit) {
        m_hintLabel->setText(QStringLiteral("超过单笔限额 ¥%1").arg(kRechargeLimit, 0, 'f', 2));
        return;
    }

    m_confirmBtn->setEnabled(false);
    m_hintLabel->clear();

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["amount"] = amount;

    NetClient::instance().sendRequest(Api::CmdUserRecharge, data,
        [this, amount](const QJsonObject& resp, int code, const QString& msg) {
            m_confirmBtn->setEnabled(true);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("充值失败：%1").arg(msg));
                return;
            }
            AppSession::instance().setBalance(resp.value("balance").toDouble());
            m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
            m_hintLabel->setText(QStringLiteral("充值成功 +¥%1").arg(amount, 0, 'f', 2));
            QMessageBox::information(this, QStringLiteral("充值成功"),
                                     QStringLiteral("已模拟支付成功，余额已更新。"));
            accept();
        });
}
