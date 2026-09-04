#include "AdminSession.h"

AdminSession& AdminSession::instance() {
    static AdminSession inst;
    return inst;
}

AdminSession::AdminSession(QObject* parent) : QObject(parent) {}

bool AdminSession::isLoggedIn() const { return m_loggedIn; }
QString AdminSession::token() const { return m_token; }
int AdminSession::adminId() const { return m_adminId; }
QString AdminSession::adminName() const { return m_name; }

void AdminSession::attach(QJsonObject& data) const {
    if (m_loggedIn) data["token"] = m_token;
}

QJsonObject AdminSession::withToken(const QJsonObject& data) const {
    QJsonObject copy = data;
    attach(copy);
    return copy;
}

void AdminSession::setLogin(int adminId, const QString& name, const QString& token) {
    m_loggedIn = true;
    m_adminId = adminId;
    m_name = name;
    m_token = token;
    emit loginChanged();
}

void AdminSession::logout() {
    m_loggedIn = false;
    m_token.clear();
    m_adminId = 0;
    m_name.clear();
    emit loggedOut();
}
