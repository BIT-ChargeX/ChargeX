#include "SessionManager.h"
#include <QRandomGenerator>

namespace {
constexpr int kTtlHours = 24;
}

SessionManager& SessionManager::instance() {
    static SessionManager inst;
    return inst;
}

QString SessionManager::createSession(int adminId, const QString& name) {
    static const QString hex = QStringLiteral("0123456789abcdef");
    QString token;
    token.reserve(32);
    auto* rnd = QRandomGenerator::global();
    for (int i = 0; i < 32; ++i) token += hex.at(rnd->bounded(16));

    QWriteLocker lock(&m_lock);
    // 同一管理员新的登录替换旧会话
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (it.value().adminId == adminId) it = m_sessions.erase(it);
        else ++it;
    }
    m_sessions.insert(token, Session{adminId, name, QDateTime::currentDateTime()});
    return token;
}

bool SessionManager::verify(const QString& token, int* adminId, QString* name) const {
    if (token.isEmpty()) return false;
    QReadLocker lock(&m_lock);
    auto it = m_sessions.constFind(token);
    if (it == m_sessions.constEnd()) return false;
    if (it.value().createdAt.secsTo(QDateTime::currentDateTime()) > kTtlHours * 3600) {
        // 过期视为无效（移除留待 remove 或下次登录取代）
        return false;
    }
    if (adminId) *adminId = it.value().adminId;
    if (name) *name = it.value().name;
    return true;
}

void SessionManager::removeSession(const QString& token) {
    QWriteLocker lock(&m_lock);
    m_sessions.remove(token);
}
