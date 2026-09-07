#include "InfluxClient.h"
#include "HttpSync.h"

#include <QUrlQuery>
#include <QStringList>
#include <QDebug>

QString InfluxClient::s_url;
QString InfluxClient::s_org;
QString InfluxClient::s_bucket;
QString InfluxClient::s_token;

void InfluxClient::configure(const QString& url, const QString& org,
                             const QString& bucket, const QString& token) {
    s_url = url;
    s_org = org;
    s_bucket = bucket;
    s_token = token;
}

QString InfluxClient::line(const QString& measurement,
                           const QMap<QString, QString>& tags,
                           const QMap<QString, QVariant>& fields,
                           qint64 timestampMs) {
    QString l = measurement;
    for (auto it = tags.begin(); it != tags.end(); ++it)
        l += QStringLiteral(",%1=%2").arg(it.key(), it.value());
    l += ' ';

    QStringList fs;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        const QVariant& v = it.value();
        QString val;
        if (v.typeId() == QMetaType::Double) {
            val = QString::number(v.toDouble(), 'f', 2);
        } else {
            val = v.toString();
        }
        fs << QStringLiteral("%1=%2").arg(it.key(), val);
    }
    l += fs.join(',');
    l += ' ' + QString::number(timestampMs);   // 配合 precision=ms
    return l;
}

bool InfluxClient::write(const QString& lineProtocol) {
    if (s_url.isEmpty() || lineProtocol.isEmpty()) return false;

    QUrl url(s_url + QStringLiteral("/api/v2/write"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("org"), s_org);
    q.addQueryItem(QStringLiteral("bucket"), s_bucket);
    q.addQueryItem(QStringLiteral("precision"), QStringLiteral("ms"));
    url.setQuery(q);

    const QList<QPair<QByteArray, QByteArray>> headers = {
        { QByteArray("Authorization"), QByteArray("Token ") + s_token.toUtf8() },
        { QByteArray("Content-Type"), QByteArray("text/plain; charset=utf-8") },
    };

    const HttpSyncResult r = httpSync("POST", url, lineProtocol.toUtf8(), headers);
    if (!r.ok) qWarning() << "[Influx] write failed:" << r.body;
    return r.ok;
}

QByteArray InfluxClient::queryFlux(const QString& flux) {
    QUrl url(s_url + QStringLiteral("/api/v2/query"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("org"), s_org);
    url.setQuery(q);

    const QList<QPair<QByteArray, QByteArray>> headers = {
        { QByteArray("Authorization"), QByteArray("Token ") + s_token.toUtf8() },
        { QByteArray("Content-Type"), QByteArray("application/vnd.flux") },
        { QByteArray("Accept"), QByteArray("application/csv") },
    };

    const HttpSyncResult r = httpSync("POST", url, flux.toUtf8(), headers);
    if (!r.ok) qWarning() << "[Influx] query failed:" << r.body;
    return r.body;
}
