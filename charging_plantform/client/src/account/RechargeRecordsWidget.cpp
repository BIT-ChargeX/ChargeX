#include "RechargeRecordsWidget.h"
#include "common/AppSession.h"
#include "common/NetClient.h"
#include "common/ApiDefs.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QColor>

RechargeRecordsWidget::RechargeRecordsWidget(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("充值记录"));
    setFixedWidth(480);
    setMinimumHeight(360);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* tip = new QLabel(QStringLiteral("支付方式：模拟支付"), this);
    tip->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(tip);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("编号"), QStringLiteral("金额"), QStringLiteral("时间")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);
}

void RechargeRecordsWidget::refresh() {
    if (!AppSession::instance().isLoggedIn()) return;

    QJsonObject data;
    data["user_id"] = AppSession::instance().userId();

    NetClient::instance().sendRequest(Api::CmdUserRechargeRecords, data,
        [this](const QJsonObject& resp, int code, const QString& /*msg*/) {
            if (code != 0) return;

            const QJsonArray items = resp.value("items").toArray();
            m_table->setRowCount(items.size());
            for (int i = 0; i < items.size(); ++i) {
                const QJsonObject it = items.at(i).toObject();
                m_table->setItem(i, 0,
                    new QTableWidgetItem(QStringLiteral("#%1").arg(it.value("recharge_id").toInt())));
                QTableWidgetItem* amount =
                    new QTableWidgetItem(QStringLiteral("+¥%1").arg(it.value("amount").toDouble(), 0, 'f', 2));
                amount->setForeground(QColor(QStringLiteral("#2e7d32")));
                m_table->setItem(i, 1, amount);
                m_table->setItem(i, 2, new QTableWidgetItem(it.value("time").toString()));
            }
        });
}
