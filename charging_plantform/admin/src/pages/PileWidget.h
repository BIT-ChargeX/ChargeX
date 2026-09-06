#pragma once
#include <QWidget>

class QTableWidget;
class QPushButton;
class QLabel;

// 需求13：充电桩管理（经协议：列表/远程重启/操作日志）
// 双击电桩行可直接远程重启（故障恢复闲置），带二次确认。
class PileWidget : public QWidget {
    Q_OBJECT
public:
    explicit PileWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void onReboot();
    void onRowDoubleClicked(int row, int column);

private:
    void loadPiles();
    void loadOpsLog();
    void doReboot(int pileId);
    void updateRebootButton();
    bool canRebootRow(int row);

    QTableWidget* m_pileTable;
    QTableWidget* m_logTable;
    QPushButton* m_rebootBtn;
    QPushButton* m_refreshBtn;
    QLabel* m_countLabel;
};
