#include "PointsWidget.h"
#include "common/AppSession.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

namespace {

// 可兑换项目（与 UserService::findRedeemItem 保持一致）
struct RedeemOption {
    QString id;
    QString text;
};

const RedeemOption kOptions[] = {
    {QStringLiteral("coupon_5"),  QStringLiteral("满10减5元优惠券（100积分）")},
    {QStringLiteral("coupon_10"), QStringLiteral("满20减10元优惠券（200积分）")},
    {QStringLiteral("coupon_30"), QStringLiteral("满50减30元优惠券（500积分）")},
    {QStringLiteral("deduct_5"),  QStringLiteral("充电费抵扣 ¥5（100积分）")},
    {QStringLiteral("deduct_20"), QStringLiteral("充电费抵扣 ¥20（400积分）")},
};

} // namespace

PointsWidget::PointsWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("碳积分"));
    setFixedWidth(520);
    setMinimumHeight(460);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_pointsLabel = new QLabel(this);
    m_pointsLabel->setStyleSheet(QStringLiteral("font-size: 18px; color: #2e7d32; font-weight: bold;"));
    m_pointsLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_pointsLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("类型"), QStringLiteral("来源"), QStringLiteral("时间"), QStringLiteral("分值")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    auto* redeemRow = new QHBoxLayout;
    redeemRow->addWidget(new QLabel(QStringLiteral("兑换"), this));
    m_itemCombo = new QComboBox(this);
    for (const auto& o : kOptions) {
        m_itemCombo->addItem(o.text, o.id);
    }
    redeemRow->addWidget(m_itemCombo, 1);
    m_redeemBtn = new QPushButton(QStringLiteral("立即兑换"), this);
    redeemRow->addWidget(m_redeemBtn);
    layout->addLayout(redeemRow);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d9534f;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    connect(m_redeemBtn, &QPushButton::clicked, this, &PointsWidget::onRedeemClicked);
}

void PointsWidget::setBusy(bool busy) {
    m_redeemBtn->setEnabled(!busy);
    m_itemCombo->setEnabled(!busy);
    m_redeemBtn->setText(busy ? QStringLiteral("兑换中…") : QStringLiteral("立即兑换"));
}

void PointsWidget::refresh() {
    if (!AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdUserPointsDetail, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }

            const int points = resp.value("points").toInt();
            m_pointsLabel->setText(QStringLiteral("当前碳积分：%1 分").arg(points));

            const QJsonArray items = resp.value("items").toArray();
            m_table->setRowCount(items.size());
            for (int i = 0; i < items.size(); ++i) {
                const QJsonObject it = items.at(i).toObject();
                m_table->setItem(i, 0, new QTableWidgetItem(it.value("type").toString()));
                m_table->setItem(i, 1, new QTableWidgetItem(it.value("source").toString()));
                m_table->setItem(i, 2, new QTableWidgetItem(it.value("time").toString()));
                QTableWidgetItem* pts =
                    new QTableWidgetItem(QStringLiteral("%1").arg(it.value("points").toInt()));
                pts->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_table->setItem(i, 3, pts);
            }
        });
}

void PointsWidget::onRedeemClicked() {
    if (!AppSession::instance().isLoggedIn()) return;

    const QString itemId = m_itemCombo->currentData().toString();
    const QString itemText = m_itemCombo->currentText();
    if (itemId.isEmpty()) return;

    if (QMessageBox::question(this, QStringLiteral("确认兑换"),
                              QStringLiteral("确定使用积分兑换「%1」吗？").arg(itemText))
        != QMessageBox::Yes) {
        return;
    }

    setBusy(true);
    m_hintLabel->clear();

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();
    data["item_id"] = itemId;

    NetClient::instance().sendRequest(Api::CmdUserPointsRedeem, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("兑换失败：%1").arg(msg));
                return;
            }

            // 抵扣充电费用会转入余额，同步更新全局会话
            if (resp.contains("balance")) {
                AppSession::instance().setBalance(resp.value("balance").toDouble());
            }

            m_hintLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
            m_hintLabel->setText(QStringLiteral("兑换成功：「%1」，最新积分 %2 分")
                                     .arg(resp.value("item_name").toString())
                                     .arg(resp.value("points").toInt()));
            refresh();
        });
}
