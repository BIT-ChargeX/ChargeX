#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>

class QLineEdit;
class QPushButton;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QTimer;
class MapPickerWidget;

// 充电站服务模块-需求2/3/5 + 需求20：
//   地址输入（腾讯联想下拉，标题+区名）-> 当前坐标展示 -> 内嵌地图二次选点
//   -> 综合推荐列表（推荐第一、其余按到达时间）
// 负责人：刘恩东 / 徐文才   命令：STATION_RECOMMEND / STATION_NEARBY / STATION_DETAIL
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
    void onSuggestBtnClicked();                      // "候选"按钮：手动触发联想下拉
    void onRegionSelected(int index);
    void onAddressTextChanged(const QString& text);
    void doSuggest();
    void onSuggestionPicked(const QString& display);
    void onSuggestionItemClicked(QListWidgetItem* item);
    void onMapPicked(double lat, double lng);        // 地图二次选点 -> 最终经纬度

private:
    void queryNearby(double lat, double lng);
    void queryNearbyFallback();                       // STATION_RECOMMEND 失败时降级
    void renderStations(const QJsonArray& stations, bool routeOk);
    void requestDetailForNav(const QJsonObject& station);
    void addStationCard(const QJsonObject& station);
    void setStatus(const QString& text, bool ok);
    void updatePosLabel(double lat, double lng, const QString& name);
    void showSuggestPopup(const QStringList& titles);
    void hideSuggestPopup();

    QLineEdit* m_addressEdit;
    QPushButton* m_suggestBtn;
    QPushButton* m_locateAddrBtn;
    QComboBox* m_regionCombo;
    QLabel* m_posLabel;              // 当前经纬度展示
    MapPickerWidget* m_mapPicker;
    QListWidget* m_listWidget;
    QLabel* m_statusLabel;

    // 地址联想下拉（自绘弹出列表，替代 QCompleter——异步刷新时弹框更可靠）
    QWidget* m_suggestPopup;
    QListWidget* m_suggestList;
    QTimer* m_suggestTimer;
    QHash<QString, QJsonObject> m_suggestItems;   // 下拉显示文本 -> {title,address,district,lat,lng}
    bool m_suppressSuggest = false;               // 选中联想项回填输入框时避免再次触发请求
};
