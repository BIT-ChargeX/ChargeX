#pragma once
#include <QWidget>

class QTableWidget;
class QPushButton;
class QLabel;

// 充电站管理（经协议）：站点列表 / 新增 / 站内电桩状态维护（故障与恢复）
class StationMgmtWidget : public QWidget {
    Q_OBJECT
public:
    explicit StationMgmtWidget(QWidget* parent = nullptr);

    void refresh();

private slots:
    void onAddStation();
    void onStationRowChanged(int row);
    void onSetFault();
    void onSetIdle();

private:
    void loadStations();
    void loadPilesOfStation(int stationId);
    void setPileStatus(int pileId, const QString& status);

    QTableWidget* m_stationTable;
    QTableWidget* m_pileTable;
    QPushButton* m_addBtn;
    QPushButton* m_faultBtn;
    QPushButton* m_idleBtn;
    QPushButton* m_refreshBtn;
    QLabel* m_statusLabel;
    int m_currentStationId = -1;
};
