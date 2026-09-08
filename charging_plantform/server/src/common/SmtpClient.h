#pragma once
#include <QString>

// 最小 SMTP 客户端（QQ/163 邮箱，SSL 465 端口）：同步阻塞发送纯文本邮件。
// 用于忘记密码等场景发送邮箱验证码；未配置时业务可降级为"演示模式"（验证码走服务端日志）。
class SmtpClient {
public:
    // host 如 smtp.qq.com / smtp.163.com；username 即发件邮箱；authCode 为邮箱的 SMTP 授权码
    static void configure(const QString& host, quint16 port,
                          const QString& username, const QString& authCode,
                          const QString& from);

    // 是否已配置完整
    static bool isConfigured();

    // 发送纯文本邮件（同步阻塞），成功返回 true
    static bool sendPlainText(const QString& to, const QString& subject,
                              const QString& body);

private:
    static QString s_host;
    static quint16 s_port;
    static QString s_username;
    static QString s_authCode;
    static QString s_from;
};
