#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QList>

class QLabel;
class QTableWidget;
class QPushButton;
class QTimer;
class QHideEvent;

// 充电站服务模块-需求3/4：充电站信息展示 + 电桩详情查询
// 负责人：刘恩东   命令：STATION_DETAIL / PILE_DETAIL_LIST
// 电桩状态由 piledev 实时上报数据库，本页打开期间每 10s 轮询刷新（需求"实时详细信息"）
class StationDetailWidget : public QDialog {
    Q_OBJECT
public:
    explicit StationDetailWidget(QWidget* parent = nullptr);

    void showStation(const QJsonObject& station);

signals:
    void pilePicked(const QJsonObject& pile);      // "去充电"选中的桩（含站点信息）
    void navRequested(const QJsonObject& station); // 完整站点信息

private slots:
    void onGoChargingClicked();
    void onNavClicked();

protected:
    void hideEvent(QHideEvent* event) override;    // 关页即停轮询

private:
    void loadDetail();
    void loadPiles();
    void fillStationHeader(const QJsonObject& s);

    QLabel* m_nameLabel;
    QLabel* m_addrLabel;
    QLabel* m_priceLabel;
    QLabel* m_statusLabel;
    QTableWidget* m_pileTable;
    QPushButton* m_goChargingBtn;
    QPushButton* m_navBtn;
    QTimer* m_refreshTimer;

    QJsonObject m_station;   // 列表带来的基础字段
    QJsonObject m_full;      // 详情接口补充后的完整字段（含地址/经纬度）
    QList<QJsonObject> m_piles;
};
