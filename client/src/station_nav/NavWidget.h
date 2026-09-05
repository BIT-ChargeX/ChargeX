#pragma once
#include <QDialog>
#include <QUrl>
#include <QJsonObject>

class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QWebEngineView;

// 需求5：充电站一键导航（腾讯地图 routeplan，QWebEngineView 加载）
// 未编译 QWebEngineWidgets 或未配置地图 key 时自动降级为浏览器打开/提示
class NavWidget : public QDialog {
    Q_OBJECT
public:
    explicit NavWidget(QWidget* parent = nullptr);

    // 设置目标电站（需含 lat/lng/name）；起点取当前模拟位置
    void setDestination(const QJsonObject& station);

    void startRoute();

private:
    void buildRouteUrl();
    void loadIntoView();

    QLineEdit* m_fromEdit;
    QLineEdit* m_toEdit;
    QComboBox* m_modeCombo;
    QPushButton* m_startBtn;
    QPushButton* m_browserBtn;
    QLabel* m_noteLabel;
    QWebEngineView* m_view = nullptr;

    QUrl m_routeUrl;
    QString m_toName;
    double m_toLat = 0.0;
    double m_toLng = 0.0;
};
