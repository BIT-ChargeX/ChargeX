#include "DeviceRuntimeWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QBrush>

DeviceRuntimeWidget::DeviceRuntimeWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* top = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("充电桩终端实时运行日志（状态变化/控制回执）"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    top->addWidget(title);
    top->addStretch(1);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("cardCaption"));
    top->addWidget(m_statusLabel);
    layout->addLayout(top);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("时间"), QStringLiteral("设备"), QStringLiteral("电桩"),
         QStringLiteral("事件"), QStringLiteral("状态"), QStringLiteral("电量"),
         QStringLiteral("功率(kW)"), QStringLiteral("说明")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(3000);
    connect(m_timer, &QTimer::timeout, this, &DeviceRuntimeWidget::refresh);
    m_timer->start();

    refresh();
}

void DeviceRuntimeWidget::refresh() {
    loadLogs();
}

void DeviceRuntimeWidget::loadLogs() {
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["limit"] = 100;

    NetClient::instance().sendRequest(Api::CmdPileRuntimeLogList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_statusLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }
            const int online = resp.value("online_piles").toInt();
            const int total = resp.value("total_piles").toInt();
            const QString ts = QDateTime::currentDateTime()
                                   .toString(QStringLiteral("HH:mm:ss"));
            m_statusLabel->setText(QStringLiteral("在线桩 %1/%2 · %3 刷新")
                                       .arg(online).arg(total).arg(ts));

            const QJsonArray logs = resp.value("logs").toArray();
            m_table->setRowCount(logs.size());
            for (int i = 0; i < logs.size(); ++i) {
                const QJsonObject o = logs.at(i).toObject();
                const int pileId = o.value("pile_id").toInt();
                const QString code_ = o.value("code").toString();
                const QString status = o.value("status").toString();

                const QStringList cols = {
                    o.value("ts").toString(),
                    o.value("device_id").toString(),
                    pileId > 0 ? QStringLiteral("%1 (%2)").arg(pileId).arg(code_)
                               : QStringLiteral("-"),
                    o.value("event").toString(),
                    status.isEmpty() ? QStringLiteral("-") : status,
                    o.value("soc").toInt() > 0 ? QString::number(o.value("soc").toInt())
                                                : QStringLiteral("-"),
                    o.value("cur_power").toDouble() > 0.0
                        ? QString::number(o.value("cur_power").toDouble(), 'f', 1)
                        : QStringLiteral("-"),
                    o.value("detail").toString(),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(c == 0 ? Qt::AlignLeft : Qt::AlignCenter);
                    m_table->setItem(i, c, it);
                }
                if (!status.isEmpty()) {
                    QTableWidgetItem* st = m_table->item(i, 4);
                    st->setForeground(QBrush(Theme::statusText(status)));
                    st->setBackground(QBrush(Theme::statusBackground(status)));
                }
            }
        });
}
