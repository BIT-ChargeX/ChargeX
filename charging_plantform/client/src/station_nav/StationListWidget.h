#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QHash>

class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QComboBox;
class QListWidget;
class QLabel;
class QCompleter;
class QStringListModel;
class QTimer;

// 充电站服务模块-需求2/3：附近充电站查询（卡片列表）+ 进入站点/触发导航
// 负责人：刘恩东 / 徐文才   命令：STATION_NEARBY / STATION_DETAIL
class StationListWidget : public QWidget {
    Q_OBJECT
public:
    explicit StationListWidget(QWidget* parent = nullptr);

    // 主页切到本页时刷新：用当前模拟位置重新查询
    void refreshNearby();

signals:
    void stationDetailRequested(const QJsonObject& station);
    void navRequested(const QJsonObject& station);   // 携带 address/lat/lng 的完整站点

private slots:
    void onLocateByAddress();
    void onLocateByCoords();
    void onRegionSelected(int index);
    void onAddressTextChanged(const QString& text);
    void doSuggest();
    void onSuggestionPicked(const QString& display);

private:
    void queryNearby(double lat, double lng);
    void queryNearbyFallback();                       // STATION_RECOMMEND 失败时降级
    void renderStations(const QJsonArray& stations, bool routeOk);
    void requestDetailForNav(const QJsonObject& station);
    void addStationCard(const QJsonObject& station);
    void setStatus(const QString& text, bool ok);

    QLineEdit* m_addressEdit;
    QDoubleSpinBox* m_latSpin;
    QDoubleSpinBox* m_lngSpin;
    QPushButton* m_locateAddrBtn;
    QPushButton* m_locateCoordBtn;
    QComboBox* m_regionCombo;
    QListWidget* m_listWidget;
    QLabel* m_statusLabel;

    // 地址联想下拉
    QCompleter* m_suggestCompleter;
    QStringListModel* m_suggestModel;
    QTimer* m_suggestTimer;
    QHash<QString, QJsonObject> m_suggestItems;   // 下拉显示文本 -> {title,address,lat,lng}
    bool m_suppressSuggest = false;               // 选中联想项回填输入框时避免再次触发请求
};
