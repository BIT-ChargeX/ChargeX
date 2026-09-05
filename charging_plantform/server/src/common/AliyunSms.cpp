#include "AliyunSms.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QMap>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QSslSocket>

namespace {

constexpr const char* kEndpoint = "https://dysmsapi.aliyuncs.com/";
constexpr const char* kAction   = "SendSms";
constexpr const char* kVersion  = "2017-05-25";

// RFC3986 百分号编码（保留 A-Z a-z 0-9 - _ . ~）
QByteArray percentEncode(const QString& s) {
    return QUrl::toPercentEncoding(s,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~");
}

}

namespace AliyunSms {

bool send(const QString& phone, const QString& code, QString* err) {
    if (kAccessKeyId[0] == '\0' || kAccessKeySecret[0] == '\0'
        || kSignName[0] == '\0' || kTemplateCode[0] == '\0') {
        if (err) *err = QStringLiteral("阿里云短信凭证未配置（见 AliyunSms.h）");
        return false;
    }
    if (!QSslSocket::supportsSsl()) {
        if (err) *err = QStringLiteral("当前系统缺少 OpenSSL 库，无法通过 HTTPS 发送短信");
        return false;
    }

    QMap<QString, QString> params;
    params["AccessKeyId"] = kAccessKeyId;
    params["Action"] = kAction;
    params["Format"] = "JSON";
    params["PhoneNumbers"] = phone;
    params["RegionId"] = "cn-hangzhou";
    params["SignName"] = kSignName;
    params["SignatureMethod"] = "HMAC-SHA1";
    params["SignatureNonce"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    params["SignatureVersion"] = "1.0";
    params["TemplateCode"] = kTemplateCode;
    params["TemplateParam"] = QStringLiteral("{\"code\":\"%1\"}").arg(code);
    params["Timestamp"] = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd'T'HH:mm:ss'Z'");
    params["Version"] = kVersion;

    // 构造待签名字符串：按键升序拼接 编码后的 key=value
    QStringList kv;
    for (auto it = params.cbegin(); it != params.cend(); ++it)
        kv << QString::fromUtf8(percentEncode(it.key())) + "=" + QString::fromUtf8(percentEncode(it.value()));
    const QString canonical = kv.join("&");
    const QString stringToSign = QStringLiteral("POST&%1&%2")
        .arg(QString::fromUtf8(percentEncode("/")),
             QString::fromUtf8(percentEncode(canonical)));
    const QByteArray signature = QMessageAuthenticationCode::hash(
        stringToSign.toUtf8(),
        (QString::fromLatin1(kAccessKeySecret) + "&").toUtf8(),
        QCryptographicHash::Sha1).toBase64();
    params["Signature"] = QString::fromLatin1(signature);

    // 组装 form body（全部百分号编码）
    kv.clear();
    for (auto it = params.cbegin(); it != params.cend(); ++it)
        kv << QString::fromUtf8(percentEncode(it.key())) + "=" + QString::fromUtf8(percentEncode(it.value()));
    const QByteArray body = kv.join("&").toUtf8();

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QString::fromLatin1(kEndpoint)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply* reply = nam.post(req, body);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);

    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (err) *err = QStringLiteral("短信服务请求超时");
        return false;
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (err) *err = reply->errorString();
        return false;
    }

    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    const QString respCode = obj.value("Code").toString();
    const QString msg = obj.value("Message").toString();
    if (respCode == "OK") return true;

    if (err) *err = msg.isEmpty() ? QString::fromUtf8(data) : msg;
    return false;
}

}
