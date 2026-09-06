#include "UserMgmtWidget.h"
#include "common/NetClient.h"
#include "common/AdminSession.h"
#include "common/ApiDefs.h"
#include "common/Theme.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QBrush>
#include <QMessageBox>

UserMgmtWidget::UserMgmtWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(QStringLiteral("手机号"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入手机号模糊搜索"));
    bar->addWidget(m_searchEdit, 1);
    m_searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    m_searchBtn->setObjectName(QStringLiteral("btnPrimary"));
    m_freezeBtn = new QPushButton(QStringLiteral("冻结选中"), this);
    m_freezeBtn->setObjectName(QStringLiteral("btnDanger"));
    m_unfreezeBtn = new QPushButton(QStringLiteral("解冻选中"), this);
    m_unfreezeBtn->setObjectName(QStringLiteral("btnSuccess"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    bar->addWidget(m_searchBtn);
    bar->addWidget(m_freezeBtn);
    bar->addWidget(m_unfreezeBtn);
    bar->addWidget(m_refreshBtn);
    layout->addLayout(bar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("用户ID"), QStringLiteral("手机号"), QStringLiteral("昵称"),
         QStringLiteral("余额(元)"), QStringLiteral("注册时间"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("cardCaption"));
    layout->addWidget(m_countLabel);

    connect(m_searchBtn, &QPushButton::clicked, this, &UserMgmtWidget::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &UserMgmtWidget::onSearch);
    connect(m_refreshBtn, &QPushButton::clicked, this, &UserMgmtWidget::refresh);
    connect(m_freezeBtn, &QPushButton::clicked, this, &UserMgmtWidget::onToggleFreeze);
    connect(m_unfreezeBtn, &QPushButton::clicked, this, &UserMgmtWidget::onToggleFreeze);

    refresh();
}

void UserMgmtWidget::onSearch() {
    loadUsers(m_searchEdit->text().trimmed());
}

void UserMgmtWidget::refresh() {
    loadUsers(m_searchEdit->text().trimmed());
}

void UserMgmtWidget::loadUsers(const QString& keyword) {
    QJsonObject data;
    AdminSession::instance().attach(data);
    if (!keyword.isEmpty()) data["phone_keyword"] = keyword;
    data["page"] = 1;

    NetClient::instance().sendRequest(Api::CmdUserList, data,
        [this](const QJsonObject& resp, int code, const QString& msg) {
            if (code != 0) {
                m_countLabel->setText(QStringLiteral("加载失败：%1").arg(msg));
                return;
            }
            const QJsonArray users = resp.value("users").toArray();
            m_table->setRowCount(users.size());
            for (int i = 0; i < users.size(); ++i) {
                const QJsonObject u = users.at(i).toObject();
                const int userId = u.value("user_id").toInt();
                const int status = u.value("status").toInt();
                const QStringList cols = {
                    QString::number(userId),
                    u.value("phone").toString(),
                    u.value("nickname").toString(),
                    QString::number(u.value("balance").toDouble(), 'f', 2),
                    u.value("reg_time").toString(),
                    status == 1 ? QStringLiteral("正常") : QStringLiteral("冻结"),
                };
                for (int c = 0; c < cols.size(); ++c) {
                    auto* it = new QTableWidgetItem(cols.at(c));
                    it->setTextAlignment(Qt::AlignCenter);
                    m_table->setItem(i, c, it);
                }
                m_table->item(i, 0)->setData(Qt::UserRole, userId);
                m_table->item(i, 5)->setData(Qt::UserRole, status);

                QTableWidgetItem* stItem = m_table->item(i, 5);
                if (status == 1) {
                    stItem->setForeground(QBrush(Theme::success()));
                    stItem->setBackground(QBrush(Theme::successContainer()));
                } else {
                    stItem->setForeground(QBrush(Theme::danger()));
                    stItem->setBackground(QBrush(Theme::dangerContainer()));
                }
            }
            m_countLabel->setText(QStringLiteral("共 %1 个用户").arg(users.size()));
        });
}

void UserMgmtWidget::onToggleFreeze() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    const int userId = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    const bool doFreeze = sender() == m_freezeBtn;

    QJsonObject data;
    AdminSession::instance().attach(data);
    data["user_id"] = userId;
    data["action"] = doFreeze ? QStringLiteral("freeze") : QStringLiteral("unfreeze");

    NetClient::instance().sendRequest(Api::CmdUserFreeze, data,
        [this](const QJsonObject&, int code, const QString& msg) {
            if (code != 0) {
                QMessageBox::warning(this, QStringLiteral("操作失败"), msg);
                return;
            }
            refresh();
        });
}
