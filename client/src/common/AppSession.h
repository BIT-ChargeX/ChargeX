#pragma once
#include <QObject>
#include <QString>

// 全局应用会话：保存登录用户信息与"当前位置(软件模拟GPS)"，
// 供 account / station_nav / charging 各模块共享，避免层层传参。
class AppSession : public QObject {
    Q_OBJECT
public:
    static AppSession& instance();

    bool isLoggedIn() const;
    int userId() const;
    QString phone() const;
    QString nickname() const;
    QString avatar() const;
    double balance() const;

    // 软件模拟 GPS：最近一次定位/手动输入的经纬度与地址
    double latitude() const;
    double longitude() const;
    QString address() const;

public slots:
    void setLogin(int userId, const QString& phone, const QString& nickname,
                  const QString& avatar, double balance);
    void logout();
    void setNickname(const QString& nickname);
    void setAvatar(const QString& avatar);
    void setBalance(double balance);
    void setPosition(double lat, double lng, const QString& address);

signals:
    void loginChanged();      // 登录成功
    void loggedOut();         // 退出登录
    void nicknameChanged(const QString& nickname);
    void avatarChanged(const QString& avatar);
    void balanceChanged(double balance);
    void positionChanged();

private:
    explicit AppSession(QObject* parent = nullptr);

    bool m_loggedIn = false;
    int m_userId = 0;
    QString m_phone;
    QString m_nickname;
    QString m_avatar;
    double m_balance = 0.0;

    double m_lat = 39.908823;   // 默认：北京市区
    double m_lng = 116.397470;
    QString m_address = "北京市";
};
