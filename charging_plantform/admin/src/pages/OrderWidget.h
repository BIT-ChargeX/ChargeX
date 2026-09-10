#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QComboBox;
class QDateEdit;
class QCheckBox;
class QPushButton;
class QTableWidget;

// 管理端“订单管理”页（经 ORDER_MGMT_LIST / ORDER_MGMT_CANCEL）：
//   分页全量订单查询（状态/关键字/下单日期过滤），
//   按“单号/邮箱号/昵称/站点/电桩/类型/下单时间/金额/状态”展示订单快照，
//   仅可“取消预约占用”订单（服务端释放电桩并审计），双击行查看详情。
class OrderWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void onSearch();
    void onCancelReserved();
    void onPrevPage();
    void onNextPage();
    void onRowDoubleClicked(int row, int column);
    void onCurrentRowChanged(int row, int, int, int);

private:
    void loadOrders(int page);
    void openDetail(int row);
    void updateActionButtons();
    void updateNavButtons();
    void setBusy(bool busy);

    QComboBox* m_statusCombo;
    QLineEdit* m_keywordEdit;
    QDateEdit* m_startEdit;
    QDateEdit* m_endEdit;
    QCheckBox* m_dateFilterCheck;
    QPushButton* m_searchBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_prevBtn;
    QPushButton* m_nextBtn;
    QTableWidget* m_table;
    QLabel* m_countLabel;

    int m_page = 1;
    int m_pages = 1;
    int m_total = 0;
    bool m_busy = false;
};
