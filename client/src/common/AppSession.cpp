#include "AppSession.h"

AppSession& AppSession::instance() {
    static AppSession inst;
    return inst;
}

AppSession::AppSession(QObject* parent) : QObject(parent) {}

bool AppSession::isLoggedIn() const { return m_loggedIn; }
int AppSession::userId() const { return m_userId; }
QString AppSession::phone() const { return m_phone; }
QString AppSession::nickname() const { return m_nickname; }
QString AppSession::avatar() const { return m_avatar; }
double AppSession::balance() const { return m_balance; }
double AppSession::latitude() const { return m_lat; }
double AppSession::longitude() const { return m_lng; }
QString AppSession::address() const { return m_address; }

void AppSession::setLogin(int userId, const QString& phone, const QString& nickname,
                          const QString& avatar, double balance) {
    m_loggedIn = true;
    m_userId = userId;
    m_phone = phone;
    m_nickname = nickname;
    m_avatar = avatar;
    m_balance = balance;
    emit loginChanged();
    emit nicknameChanged(m_nickname);
    emit avatarChanged(m_avatar);
    emit balanceChanged(m_balance);
}

void AppSession::logout() {
    m_loggedIn = false;
    m_userId = 0;
    m_phone.clear();
    m_nickname.clear();
    m_avatar.clear();
    m_balance = 0.0;
    emit loggedOut();
}

void AppSession::setNickname(const QString& nickname) {
    m_nickname = nickname;
    emit nicknameChanged(m_nickname);
}

void AppSession::setAvatar(const QString& avatar) {
    m_avatar = avatar;
    emit avatarChanged(m_avatar);
}

void AppSession::setBalance(double balance) {
    m_balance = balance;
    emit balanceChanged(m_balance);
}

void AppSession::setPosition(double lat, double lng, const QString& address) {
    m_lat = lat;
    m_lng = lng;
    m_address = address;
    emit positionChanged();
}
