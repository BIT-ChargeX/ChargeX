#include "OrderListWidget.h"
#include "common/NetClient.h"
#include "common/AppSession.h"
#include "common/ApiDefs.h"

#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QBrush>
#include <QColor>

namespace {

bool isUnfinished(const QString& status) {
    return status == QStringLiteral("预约占用")
        || status == QStringLiteral("充电中")
        || status == QStringLiteral("待结算");
}

} // namespace

OrderListWidget::OrderListWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("我的充电订单"));
    setFixedWidth(760);
    setMinimumHeight(480);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    m_bannerLabel = new QLabel(this);
    m_bannerLabel->setWordWrap(true);
    m_bannerLabel->setStyleSheet(QStringLiteral("background: #fff3e0; color: #c62828;"
                                                "padding: 10px; border-radius: 6px;"));
    m_bannerLabel->hide();
    layout->addWidget(m_bannerLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("单号"), QStringLiteral("站点"), QStringLiteral("电桩"),
         QStringLiteral("类型"), QStringLiteral("开始时间"), QStringLiteral("金额"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    auto* btnRow = new QHBoxLayout;
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_settleBtn = new QPushButton(QStringLiteral("结算选中订单"), this);
    m_settleBtn->setEnabled(false);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(m_settleBtn);
    btnRow->addStretch(1);
    layout->addLayout(btnRow);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #888;"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    connect(refreshBtn, &QPushButton::clicked, this, &OrderListWidget::refresh);
    connect(m_settleBtn, &QPushButton::clicked, this, &OrderListWidget::onSettleClicked);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &OrderListWidget::updateSettleEnabled);
}

void OrderListWidget::showUnfinishedBanner(int unfinishedCount) {
    if (unfinishedCount > 0) {
        m_bannerLabel->setText(QStringLiteral("您有 %1 笔未完成订单，选中后点击「结算选中订单」完成支付。")
                                   .arg(unfinishedCount));
        m_bannerLabel->show();
    } else {
        m_bannerLabel->hide();
    }
}

void OrderListWidget::updateSettleEnabled() {
    const int row = m_table->currentRow();
    bool enabled = false;
    if (row >= 0 && row < m_orders.size()) {
        const QJsonObject o = m_orders.at(row).toObject();
        enabled = isUnfinished(o.value("status").toString());
    }
    m_settleBtn->setEnabled(enabled);
}

void OrderListWidget::refresh() {
    if (!AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdOrderList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_hintLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }
            m_orders = resp.value("orders").toArray();

            int unfinished = 0;
            m_table->setRowCount(m_orders.size());
            for (int i = 0; i < m_orders.size(); ++i) {
                const QJsonObject o = m_orders.at(i).toObject();
                const QString status = o.value("status").toString();
                if (isUnfinished(status)) ++unfinished;

                QString startText = o.value("start_time").toString();
                if (startText.isEmpty()) startText = o.value("reserve_time").toString();
                if (startText.isEmpty()) startText = o.value("created_at").toString();

                const QString amountText = (status == QStringLiteral("已完成"))
                    ? QStringLiteral("¥%1").arg(o.value("amount").toDouble(), 0, 'f', 2)
                    : QStringLiteral("--");

                const QStringList cols = {
                    QStringLiteral("#%1").arg(o.value("order_id").toInt()),
                    o.value("station_name").toString(),
                    o.value("pile_code").toString(),
                    o.value("type").toString(),
                    startText,
                    amountText,
                    status,
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols[c]);
                    it->setTextAlignment(Qt::AlignCenter);
                    if (c == 6 && isUnfinished(status)) {
                        it->setForeground(QBrush(QColor(QStringLiteral("#c62828"))));
                    }
                    m_table->setItem(i, c, it);
                }
            }

            showUnfinishedBanner(unfinished);
            m_hintLabel->setText(m_orders.isEmpty()
                ? QStringLiteral("暂无充电订单，去「找桩」页开始一次充电吧。")
                : QStringLiteral("共 %1 笔订单，按时间倒序排列。").arg(m_orders.size()));
            updateSettleEnabled();
        });
}

void OrderListWidget::onSettleClicked() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_orders.size()) return;
    const int orderId = m_orders.at(row).toObject().value("order_id").toInt();
    if (orderId <= 0) return;
    emit settleRequested(orderId);
}
