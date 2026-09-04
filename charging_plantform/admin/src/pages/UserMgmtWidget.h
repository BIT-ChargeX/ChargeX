#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;

// 需求14：用户账号管理（经 USER_LIST / USER_FREEZE 协议请求服务器）
class UserMgmtWidget : public QWidget {
    Q_OBJECT
public:
    explicit UserMgmtWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void onSearch();
    void onToggleFreeze();

private:
    void loadUsers(const QString& keyword = QString());

    QLineEdit* m_searchEdit;
    QPushButton* m_searchBtn;
    QPushButton* m_freezeBtn;
    QPushButton* m_unfreezeBtn;
    QPushButton* m_refreshBtn;
    QTableWidget* m_table;
    QLabel* m_countLabel;
};
