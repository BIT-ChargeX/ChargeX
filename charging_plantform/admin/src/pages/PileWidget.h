#pragma once
#include <QWidget>

class QTableWidget;
class QPushButton;
class QLabel;
class QComboBox;
class QTimer;
class LineChartWidget;

// 需求13：充电桩管理（经协议：列表/远程重启/操作日志）
// 双击电桩行可远程重启；选中电桩下方展示“功率-时间”曲线（3s 自动刷新）。
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
    void loadTrend();
    void updateDurationColumn();
    int currentPileId();

    QTableWidget* m_pileTable;
    QTableWidget* m_logTable;
    QPushButton* m_rebootBtn;
    QPushButton* m_refreshBtn;
    QLabel* m_countLabel;
    QLabel* m_trendTitle;
    QComboBox* m_trendRange;
    LineChartWidget* m_line;
    QTimer* m_trendTimer;
    QTimer* m_durationTimer;
    int m_trendGen = 0;
};
