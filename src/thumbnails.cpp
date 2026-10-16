#include "thumbnails.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

namespace bc {

ThumbnailLoader &ThumbnailLoader::instance()
{
    // Harus dimiliki QCoreApplication: bila berupa objek static biasa, QPixmap di
    // cache baru dihancurkan SETELAH QApplication hilang -> crash saat keluar di Linux.
    static ThumbnailLoader *loader = new ThumbnailLoader(QCoreApplication::instance());
    return *loader;
}

ThumbnailLoader::ThumbnailLoader(QObject *parent) : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void ThumbnailLoader::setCacheDir(const QString &dir)
{
    m_dir = dir;
    QDir().mkpath(dir);
}

QString ThumbnailLoader::key(const QString &url, int size, bool rounded) const
{
    return QStringLiteral("%1|%2|%3").arg(url).arg(size).arg(rounded ? 1 : 0);
}

QString ThumbnailLoader::diskPath(const QString &url, int size, bool rounded) const
{
    if (m_dir.isEmpty())
        return QString();
    const QByteArray hash =
        QCryptographicHash::hash((url + QStringLiteral("#") + QString::number(size)
                                  + (rounded ? QStringLiteral("r") : QString()))
                                     .toUtf8(),
                                 QCryptographicHash::Md5)
            .toHex();
    return m_dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".png");
}

QPixmap ThumbnailLoader::roundPixmap(const QPixmap &src, int radius)
{
    if (src.isNull())
        return src;
    QPixmap out(src.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, src.width(), src.height()), radius, radius);
    p.setClipPath(path);
    p.drawPixmap(0, 0, src);
    p.end();
    return out;
}

QPixmap ThumbnailLoader::cached(const QString &url, int size, bool rounded) const
{
    return m_cache.value(key(url, size, rounded));
}

void ThumbnailLoader::load(const QString &url, int size, bool rounded, Callback cb)
{
    if (url.isEmpty()) {
        if (cb)
            cb(QPixmap());
        return;
    }
    const QString k = key(url, size, rounded);
    if (m_cache.contains(k)) {
        if (cb)
            cb(m_cache.value(k));
        return;
    }

    if (m_waiting.contains(k)) {
        if (cb)
            m_waiting[k].append(cb);
        return;
    }

    m_waiting.insert(k, cb ? QList<Callback>{cb} : QList<Callback>{});

    const QString disk = diskPath(url, size, rounded);
    if (!disk.isEmpty() && QFile::exists(disk)) {
        QPixmap pm;
        if (pm.load(disk)) {
            m_cache.insert(k, pm);
            flush(k);
            return;
        }
    }

    fetch(url, k, disk, size, rounded);
}

void ThumbnailLoader::fetch(const QString &url, const QString &k, const QString &disk, int size,
                            bool rounded)
{
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) BlackcurrantMusic/1.0"));
    req.setTransferTimeout(15000); // jaringan macet jangan membuat thumbnail menggantung selamanya
    QNetworkReply *reply = m_net->get(req);
    m_active.insert(k, size);
    connect(reply, &QNetworkReply::finished, this, [this, reply, k, disk, size, rounded]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        QPixmap pm;
        bool ok = false;
        if (reply->error() == QNetworkReply::NoError && !data.isEmpty())
            ok = pm.loadFromData(data);
        if (!ok) {
            // Gagal (offline sesaat, 404, timeout): JANGAN simpan pixmap kosong di cache.
            // Dulu kegagalan disimpan permanen sehingga thumbnail tetap kosong sampai
            // aplikasi dibuka ulang. Sekarang percobaan berikutnya mengunduh ulang.
            flush(k);
            return;
        }
        QPixmap scaled = pm.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
        const int x = qMax(0, (scaled.width() - size) / 2);
        const int y = qMax(0, (scaled.height() - size) / 2);
        scaled = scaled.copy(x, y, size, size);
        if (rounded)
            scaled = roundPixmap(scaled, 10);
        m_cache.insert(k, scaled);
        if (!disk.isEmpty())
            scaled.save(disk, "PNG");
        flush(k);
    });
}

void ThumbnailLoader::flush(const QString &k)
{
    if (!m_waiting.contains(k))
        return;
    const QPixmap pm = m_cache.value(k);
    const QList<Callback> cbs = m_waiting.take(k);
    m_active.remove(k);
    for (const Callback &cb : cbs) {
        if (cb)
            cb(pm);
    }
}

} // namespace bc
