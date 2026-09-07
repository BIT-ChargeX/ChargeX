#pragma once
#include <QObject>
#include <QByteArray>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QList>
#include <QPair>

// 同步 HTTP 请求工具：本地 QNetworkAccessManager + 嵌套 QEventLoop 阻塞等待完成。
// 供 InfluxClient / MinioClient 在任意线程（含 TCP 连接工作线程）同步调用，
// 保证与现有 Service 同步返回 Reply 的调用方式一致。
struct HttpSyncResult {
    bool ok = false;
    QByteArray body;
};

inline HttpSyncResult httpSync(const QByteArray& verb, const QUrl& url,
                               const QByteArray& body,
                               const QList<QPair<QByteArray, QByteArray>>& headers = {}) {
    HttpSyncResult r;
    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req(url);
    for (const auto& h : headers) req.setRawHeader(h.first, h.second);
    QNetworkReply* reply = nam->sendCustomRequest(req, verb, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    r.ok = (reply->error() == QNetworkReply::NoError);
    r.body = reply->readAll();
    reply->deleteLater();
    nam->deleteLater();
    return r;
}
