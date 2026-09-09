#include "SmtpClient.h"

#include <QSslSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QDebug>

QString SmtpClient::s_host;
quint16 SmtpClient::s_port = 465;
QString SmtpClient::s_username;
QString SmtpClient::s_authCode;
QString SmtpClient::s_from;

void SmtpClient::configure(const QString& host, quint16 port,
                           const QString& username, const QString& authCode,
                           const QString& from) {
    s_host = host;
    s_port = port;
    s_username = username;
    s_authCode = authCode;
    s_from = from;
}

bool SmtpClient::isConfigured() {
    return !s_host.isEmpty() && !s_username.isEmpty() && !s_authCode.isEmpty();
}

namespace {

// 阻塞等待直到可读一行（\r\n），超时返回 false
bool waitLine(QSslSocket& s, int timeoutMs) {
    QElapsedTimer t;
    t.start();
    while (!s.canReadLine()) {
        if (t.elapsed() >= timeoutMs) return false;
        if (!s.waitForReadyRead(timeoutMs - t.elapsed())) return false;
    }
    return true;
}

// 读取一条 SMTP 回复（可能多行，第4字符为 '-' 表示还有后续行），校验最终行前缀
bool expect(QSslSocket& s, const QByteArray& okPrefix, QString* err) {
    QByteArray line;
    while (true) {
        if (!waitLine(s, 10000)) { if (err) *err = QStringLiteral("SMTP 响应超时"); return false; }
        line = s.readLine().trimmed();
        if (line.size() >= 4 && line.at(3) == '-') continue;   // 多行回复的中间行
        break;
    }
    if (!line.startsWith(okPrefix)) {
        if (err) *err = QStringLiteral("SMTP 错误: %1").arg(QString::fromUtf8(line));
        return false;
    }
    return true;
}

// 发送一条命令并校验回复
bool sendCmd(QSslSocket& s, const QByteArray& cmd, const QByteArray& okPrefix, QString* err) {
    s.write(cmd);
    return expect(s, okPrefix, err);
}

// MIME 编码主题，支持中文
QByteArray encodeSubject(const QString& subject) {
    return "=?UTF-8?B?" + subject.toUtf8().toBase64() + "?=";
}

} // namespace

bool SmtpClient::sendPlainText(const QString& to, const QString& subject,
                               const QString& body) {
    if (!isConfigured() || to.isEmpty()) return false;

    QSslSocket s;
    s.connectToHostEncrypted(s_host, s_port);
    if (!s.waitForEncrypted(10000)) {
        qWarning() << "[Smtp] TLS 连接失败:" << s.errorString();
        return false;
    }

    QString err;
    // 220 问候
    if (!waitLine(s, 10000)) { qWarning() << "[Smtp] 未收到问候"; return false; }
    s.readLine();

    const QByteArray from = (s_from.isEmpty() ? s_username : s_from).toUtf8();
    if (!sendCmd(s, "EHLO charging\r\n", "250", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, "AUTH LOGIN\r\n", "334", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, s_username.toUtf8().toBase64() + "\r\n", "334", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, s_authCode.toUtf8().toBase64() + "\r\n", "235", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, "MAIL FROM:<" + from + ">\r\n", "250", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, "RCPT TO:<" + to.toUtf8() + ">\r\n", "250", &err)) { qWarning() << "[Smtp]" << err; return false; }
    if (!sendCmd(s, "DATA\r\n", "354", &err)) { qWarning() << "[Smtp]" << err; return false; }

    QByteArray mail;
    mail += "From: ChargeX <" + from + ">\r\n";
    mail += "To: " + to.toUtf8() + "\r\n";
    mail += "Subject: " + encodeSubject(subject) + "\r\n";
    mail += "MIME-Version: 1.0\r\n";
    mail += "Content-Type: text/plain; charset=utf-8\r\n";
    mail += "Content-Transfer-Encoding: base64\r\n";
    mail += "\r\n";
    mail += body.toUtf8().toBase64();
    mail += "\r\n.\r\n";

    if (!sendCmd(s, mail, "250", &err)) { qWarning() << "[Smtp]" << err; return false; }

    s.write("QUIT\r\n");
    s.flush();
    return true;
}
