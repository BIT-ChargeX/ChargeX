#include "OrderWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QDateTime>
#include <QDate>
#include <QBrush>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

namespace {
constexpr int kPageSize = 20;
const QString kReserved = QStringLiteral("预约占用");
const QString kDone = QStringLiteral("已完成");

QString fmtTs(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : iso;
}

QString moneyText(const QString& status, double amount) {
    return status == kDone ? QStringLiteral("¥%1").arg(amount, 0, 'f', 2)
                           : QStringLiteral("-");
}

} // namespace

OrderWidget::OrderWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    // 顶部：状态 / 关键字 / 下单日期 / 搜索
    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(QStringLiteral("订单状态"), this));

    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItem(QStringLiteral("全部"), QString());
    m_statusCombo->addItem(kReserved, kReserved);
    m_statusCombo->addItem(QStringLiteral("充电中"), QStringLiteral("充电中"));
    m_statusCombo->addItem(QStringLiteral("待结算"), QStringLiteral("待结算"));
    m_statusCombo->addItem(kDone, kDone);
    m_statusCombo->addItem(QStringLiteral("已取消"), QStringLiteral("已取消"));
    bar->addWidget(m_statusCombo);

    bar->addSpacing(8);
    bar->addWidget(new QLabel(QStringLiteral("关键字"), this));
    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText(QStringLiteral("邮箱/昵称/桩号/站名"));
    m_keywordEdit->setMaximumWidth(200);
    bar->addWidget(m_keywordEdit, 1);

    bar->addSpacing(8);
    m_dateFilterCheck = new QCheckBox(QStringLiteral("按下单日期"), this);
    m_startEdit = new QDateEdit(this);
    m_startEdit->setCalendarPopup(true);
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startEdit->setDate(QDate::currentDate().addDays(-29));
    m_endEdit = new QDateEdit(this);
    m_endEdit->setCalendarPopup(true);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endEdit->setDate(QDate::currentDate());
    bar->addWidget(m_dateFilterCheck);
    bar->addWidget(m_startEdit);
    bar->addWidget(new QLabel(QStringLiteral("至"), this));
    bar->addWidget(m_endEdit);

    m_searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    m_searchBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_cancelBtn = new QPushButton(QStringLiteral("取消选中预约"), this);
    m_cancelBtn->setObjectName(QStringLiteral("btnDanger"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    bar->addWidget(m_searchBtn);
    bar->addWidget(m_cancelBtn);
    bar->addWidget(m_refreshBtn);
    layout->addLayout(bar);

    // 主表
    m_table = new QTableWidget(this);
    m_table->setColumnCount(9);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("单号"), QStringLiteral("邮箱号"), QStringLiteral("昵称"),
         QStringLiteral("站点"), QStringLiteral("电桩"), QStringLiteral("类型"),
         QStringLiteral("下单时间"), QStringLiteral("金额"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setToolTip(QStringLiteral("双击订单行可查看详情"));
    layout->addWidget(m_table, 1);

    // 分页条
    auto* pageBar = new QHBoxLayout;
    m_prevBtn = new QPushButton(QStringLiteral("上一页"), this);
    m_prevBtn->setObjectName(QStringLiteral("btnGhost"));
    m_nextBtn = new QPushButton(QStringLiteral("下一页"), this);
    m_nextBtn->setObjectName(QStringLiteral("btnGhost"));
    pageBar->addWidget(m_prevBtn);
    pageBar->addWidget(m_nextBtn);
    pageBar->addStretch(1);
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("cardCaption"));
    pageBar->addWidget(m_countLabel);
    layout->addLayout(pageBar);

    connect(m_searchBtn, &QPushButton::clicked, this, &OrderWidget::onSearch);
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &OrderWidget::onSearch);
    connect(m_refreshBtn, &QPushButton::clicked, this, &OrderWidget::refresh);
    connect(m_cancelBtn, &QPushButton::clicked, this, &OrderWidget::onCancelReserved);
    connect(m_prevBtn, &QPushButton::clicked, this, &OrderWidget::onPrevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &OrderWidget::onNextPage);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &OrderWidget::onRowDoubleClicked);
    connect(m_table, &QTableWidget::currentCellChanged,
            this, &OrderWidget::onCurrentRowChanged);

    m_cancelBtn->setEnabled(false);
    updateActionButtons();
    refresh();
}

void OrderWidget::onSearch() {
    if (m_busy) return;
    m_page = 1;
    loadOrders(1);
}

void OrderWidget::refresh() {
    if (m_busy) return;
    loadOrders(m_page);
}

void OrderWidget::onPrevPage() {
    if (m_busy || m_page <= 1) return;
    loadOrders(m_page - 1);
}

void OrderWidget::onNextPage() {
    if (m_busy || m_page >= m_pages) return;
    loadOrders(m_page + 1);
}

void OrderWidget::onCurrentRowChanged(int, int, int, int) {
    updateActionButtons();
}

void OrderWidget::loadOrders(int page) {
    if (m_busy) return;
    m_page = qMax(1, page);

    setBusy(true);
    m_countLabel->setText(QStringLiteral("加载中…"));

    QJsonObject data;
    AdminSession::instance().attach(data);
    data["page"] = m_page;
    data["page_size"] = kPageSize;
    const QString status = m_statusCombo->currentData().toString().trimmed();
    if (!status.isEmpty()) data["status"] = status;
    const QString keyword = m_keywordEdit->text().trimmed();
    if (!keyword.isEmpty()) data["keyword"] = keyword;
    if (m_dateFilterCheck->isChecked()) {
        data["start_date"] = m_startEdit->date().toString(Qt::ISODate);
        data["end_date"] = m_endEdit->date().toString(Qt::ISODate);
    }

    NetClient::instance().sendRequest(Api::CmdOrderMgmtList, data,
        [this, keyword, status](const QJsonObject& resp, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                m_table->setRowCount(0);
                m_countLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }

            const int total = resp.value("total").toInt();
            m_total = total;
            m_pages = qMax(1, static_cast<int>(std::ceil(total / double(kPageSize))));
            if (m_page > m_pages) {
                if (m_pages >= 1) {
                    m_page = m_pages;
                    loadOrders(m_page);
                    return;
                }
            }

            const QJsonArray orders = resp.value("orders").toArray();
            const bool hasFilter = !status.isEmpty() || !keyword.isEmpty()
                                   || m_dateFilterCheck->isChecked();

            if (orders.isEmpty()) {
                m_table->setRowCount(1);
                auto* it = new QTableWidgetItem(
                    hasFilter ? QStringLiteral("未找到匹配的订单，可调整筛选")
                              : QStringLiteral("暂无订单"));
                it->setTextAlignment(Qt::AlignCenter);
                it->setForeground(QBrush(Theme::textMuted()));
                m_table->setItem(0, 0, it);
                m_table->setSpan(0, 0, 1, m_table->columnCount());
            } else {
                m_table->setRowCount(orders.size());
                for (int i = 0; i < orders.size(); ++i) {
                    const QJsonObject o = orders.at(i).toObject();
                    const QString st = o.value("status").toString();
                    const QStringList cols = {
                        QString::number(o.value("order_id").toInt()),
                        o.value("email").toString(),
                        o.value("nickname").toString(),
                        o.value("station").toString(),
                        o.value("code").toString(),
                        o.value("type").toString(),
                        fmtTs(o.value("created_at").toString()),
                        moneyText(st, o.value("amount").toDouble()),
                        st,
                    };
                    for (int c = 0; c < cols.size(); ++c) {
                        auto* it = new QTableWidgetItem(cols.at(c));
                        it->setTextAlignment(Qt::AlignCenter);
                        m_table->setItem(i, c, it);
                    }
                    m_table->item(i, 0)->setData(Qt::UserRole, o.value("order_id").toInt());
                    QTableWidgetItem* stItem = m_table->item(i, 8);
                    stItem->setForeground(QBrush(Theme::orderStatusText(st)));
                    stItem->setBackground(QBrush(Theme::orderStatusBackground(st)));
                }
            }

            m_countLabel->setText(QStringLiteral("共 %1 条订单 · 第 %2 / %3 页")
                                      .arg(m_total).arg(m_page).arg(m_pages));
            updateNavButtons();
            updateActionButtons();
        });
}

void OrderWidget::onCancelReserved() {
    if (m_busy) return;
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) return;
    const QString status = m_table->item(row, 8)->text();
    if (status != kReserved) return;

    const int orderId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const QString email = m_table->item(row, 1)->text();
    if (QMessageBox::question(this, QStringLiteral("取消预约"),
                              QStringLiteral("确定取消订单 %1（用户 %2）的预约吗？"
                                             "取消后电桩将恢复为空闲。")
                                  .arg(orderId).arg(email),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    setBusy(true);
    QJsonObject data;
    AdminSession::instance().attach(data);
    data["order_id"] = orderId;

    NetClient::instance().sendRequest(Api::CmdOrderMgmtCancel, data,
        [this, orderId](const QJsonObject&, int code, const QString& msg) {
            setBusy(false);
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("取消失败"), msg);
                return;
            }
            QMessageBox::information(this, QStringLiteral("取消预约"),
                                     QStringLiteral("已取消订单 %1。").arg(orderId));
            refresh();
        });
}

void OrderWidget::onRowDoubleClicked(int row, int column) {
    Q_UNUSED(column)
    if (row >= 0) openDetail(row);
}

void OrderWidget::openDetail(int row) {
    if (row < 0 || !m_table->item(row, 0)) return;
    const int orderId = m_table->item(row, 0)->data(Qt::UserRole).toInt();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("订单详情 #%1").arg(orderId));

    auto* form = new QFormLayout;
    form->setContentsMargins(24, 20, 24, 16);
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeft);

    const auto addRow = [&](const QString& key, const QString& value) {
        auto* v = new QLabel(value);
        v->setWordWrap(true);
        auto* k = new QLabel(key);
        k->setObjectName(QStringLiteral("fieldLabel"));
        form->addRow(k, v);
        return v;
    };

    addRow(QStringLiteral("单号"), QString::number(orderId));
    addRow(QStringLiteral("用户"),
           QStringLiteral("%1（%2）").arg(m_table->item(row, 2)->text(),
                                          m_table->item(row, 1)->text()));
    addRow(QStringLiteral("站点"), m_table->item(row, 3)->text());
    addRow(QStringLiteral("电桩"), m_table->item(row, 4)->text());
    addRow(QStringLiteral("类型/功率"), m_table->item(row, 5)->text());
    addRow(QStringLiteral("下单时间"), m_table->item(row, 6)->text());
    addRow(QStringLiteral("金额"), m_table->item(row, 7)->text());
    QLabel* statusVal = addRow(QStringLiteral("状态"), m_table->item(row, 8)->text());
    statusVal->setStyleSheet(QStringLiteral("color:%1; font-weight:600;")
                                 .arg(Theme::orderStatusText(
                                          m_table->item(row, 8)->text())
                                          .name()));

    auto* root = new QVBoxLayout(&dlg);
    root->addLayout(form);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    closeBtn->setObjectName(QStringLiteral("btnGhost"));
    auto* hb = new QHBoxLayout;
    hb->addStretch(1);
    hb->addWidget(closeBtn);
    root->addLayout(hb);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.resize(520, 360);
    dlg.exec();
}

void OrderWidget::updateActionButtons() {
    const int row = m_table->currentRow();
    const bool has = !m_busy && row >= 0 && m_table->item(row, 8);
    const QString status = has ? m_table->item(row, 8)->text() : QString();
    m_cancelBtn->setEnabled(has && status == kReserved);
}

void OrderWidget::updateNavButtons() {
    m_prevBtn->setEnabled(!m_busy && m_page > 1);
    m_nextBtn->setEnabled(!m_busy && m_page < m_pages);
}

void OrderWidget::setBusy(bool busy) {
    m_busy = busy;
    m_searchBtn->setEnabled(!busy);
    m_refreshBtn->setEnabled(!busy);
    m_statusCombo->setEnabled(!busy);
    m_keywordEdit->setEnabled(!busy);
    m_dateFilterCheck->setEnabled(!busy);
    m_startEdit->setEnabled(!busy);
    m_endEdit->setEnabled(!busy);
    updateNavButtons();
    updateActionButtons();
}
