#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QObject>

class QWebEngineView;
class QShowEvent;

// 网页 JS <-> Qt 桥接对象（注册进 QWebChannel，页面里以 channel.objects.bridge 访问）
class MapBridge : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

public slots:
    void pick(double lat, double lng) { emit picked(lat, lng); }
    void ready() { emit mapReady(); }

signals:
    void picked(double lat, double lng);
    void mapReady();
};

// 找桩页内嵌腾讯地图（JavaScript API GL）面板：
//   centerOn(lat, lng)      —— 地图以该坐标为中心（选中联想地址/区域时调用）
//   showStations(array)     —— 在地图上标记推荐站点
//   用户点击地图任意位置    —— 发出 pointPicked(lat, lng)（二次选点，得到最终经纬度）
// 未编译 WebEngine 时显示占位提示；JS 加载失败（key 无 JS GL 权限/断网）显示降级文案。
class MapPickerWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapPickerWidget(QWidget* parent = nullptr);

    void centerOn(double lat, double lng);
    void showStations(const QJsonArray& stations);

signals:
    void pointPicked(double lat, double lng);

protected:
    void showEvent(QShowEvent* event) override;   // 页面可见后再触发地图创建

private:
    void applyCenter(double lat, double lng);

    QWebEngineView* m_view = nullptr;
    MapBridge* m_bridge = nullptr;
    bool m_ready = false;      // 页面 JS 初始化完成（含降级情况）
    double m_lat = 39.908823;  // 待应用中心（页面未就绪时暂存）
    double m_lng = 116.397470;
};
