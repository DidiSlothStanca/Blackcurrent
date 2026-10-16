#pragma once

#include "i18n.h"

// Model data lagu (Track) + pembantu format/thumb — porting dari
// `player/models.py` versi Python.

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QRegularExpression>

namespace bc {

// --------------------------------------------------------------------------
// Utilitas
// ---------------------------------------------------------------------------

// Ubah durasi apa pun menjadi detik. Mengembalikan -1 bila tidak diketahui.
inline int parseDuration(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined())
        return -1;
    if (value.isDouble())
        return static_cast<int>(value.toDouble());
    if (value.isString()) {
        const QString s = value.toString().trimmed();
        if (s.isEmpty())
            return -1;
        bool ok = false;
        const int asInt = s.toInt(&ok);
        if (ok)
            return asInt;
        const QStringList parts = s.split(':');
        int seconds = 0;
        for (const QString &part : parts) {
            bool okPart = false;
            const int v = part.toInt(&okPart);
            if (!okPart)
                return -1;
            seconds = seconds * 60 + v;
        }
        return seconds;
    }
    if (value.isBool())
        return -1;
    return -1;
}

// Format detik menjadi "m:ss" atau "h:mm:ss". -1 => "--:--".
inline QString formatDuration(int seconds)
{
    if (seconds < 0)
        return QStringLiteral("--:--");
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

// Pilih URL thumbnail beresolusi terbaik dari daftar thumbnails.
inline QString bestThumb(const QJsonArray &thumbnails)
{
    QString best;
    int bestWidth = -1;
    for (const QJsonValue &v : thumbnails) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        const int width = o.value(QStringLiteral("width")).toInt(0);
        const QString url = o.value(QStringLiteral("url")).toString();
        if (best.isEmpty() || width >= bestWidth) {
            if (!url.isEmpty()) {
                best = url;
                bestWidth = width;
            }
        }
    }
    return best;
}

// Tingkatkan resolusi thumbnail Google (ganti parameter =w..-h..).
// HANYA untuk host gambar Google (lh3.googleusercontent.com / ggpht.com). URL lain
// (mis. i.ytimg.com/vi/ID/sddefault.jpg?sqp=...&rs=...) harus dibiarkan apa adanya:
// menambahkan "=w544-h544..." di sana menghasilkan URL rusak (404) -> thumbnail kosong.
inline QString upscaleThumb(const QString &url)
{
    if (url.isEmpty())
        return QString();
    const bool google = url.contains(QLatin1String("googleusercontent.com"))
                        || url.contains(QLatin1String("ggpht.com"));
    if (!google)
        return url;
    const int eq = url.lastIndexOf(QLatin1Char('='));
    QString base = url;
    if (eq >= 0) {
        // Buang hanya bila bagian setelah '=' memang parameter ukuran (w544-h544-..., s120-...).
        static const QRegularExpression sizeParam(QStringLiteral("^[wsh]\\d"));
        if (sizeParam.match(url.mid(eq + 1)).hasMatch())
            base = url.left(eq);
    }
    return base + QStringLiteral("=w544-h544-l90-rj");
}

// Thumbnail cadangan dari videoId. Hasil "radio"/rekomendasi YouTube Music tidak
// selalu menyertakan daftar thumbnail; gambar ini selalu ada untuk setiap video.
// mqdefault = 320x180 tanpa pita hitam, lalu dipotong persegi oleh ThumbnailLoader.
inline QString videoThumb(const QString &videoId)
{
    if (videoId.isEmpty())
        return QString();
    return QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(videoId);
}

// Ambil URL thumbnail dari nilai JSON yang bentuknya bisa bermacam-macam:
// string, array {url,width,height}, atau object tunggal.
inline QString thumbFromJson(const QJsonValue &v)
{
    if (v.isString())
        return v.toString();
    if (v.isArray())
        return bestThumb(v.toArray());
    if (v.isObject())
        return v.toObject().value(QStringLiteral("url")).toString();
    return QString();
}

// ---------------------------------------------------------------------------
// Track
// ---------------------------------------------------------------------------
struct Track {
    QString videoId;
    QString title;
    QStringList artists;
    QStringList artistIds;
    QString album;
    int duration = -1; // detik; -1 = tidak diketahui
    QString thumbnail;

    bool isValid() const { return !videoId.isEmpty(); }

    QString artistStr() const
    {
        return artists.isEmpty() ? TR("Artis tidak diketahui")
                                 : artists.join(QStringLiteral(", "));
    }

    QString displayDuration() const { return formatDuration(duration); }

    QString bigThumbnail() const { return upscaleThumb(thumbnail); }

    static void names(const QJsonArray &items, QStringList &outNames, QStringList &outIds)
    {
        for (const QJsonValue &v : items) {
            if (v.isObject()) {
                const QJsonObject o = v.toObject();
                const QString name = o.value(QStringLiteral("name")).toString();
                if (!name.isEmpty())
                    outNames << name;
                QString aid = o.value(QStringLiteral("id")).toString();
                if (aid.isEmpty())
                    aid = o.value(QStringLiteral("browseId")).toString();
                if (!aid.isEmpty())
                    outIds << aid;
            } else if (v.isString()) {
                outNames << v.toString();
            }
        }
    }

    // Mirip Track.from_search: menerima dict hasil pencarian/home.
    static Track fromSearch(const QJsonObject &item)
    {
        Track t;
        t.videoId = item.value(QStringLiteral("videoId")).toString();
        t.title = item.value(QStringLiteral("title")).toString();
        if (t.title.isEmpty())
            t.title = TR("Tanpa judul");
        names(item.value(QStringLiteral("artists")).toArray(), t.artists, t.artistIds);
        const QJsonValue albumVal = item.value(QStringLiteral("album"));
        if (albumVal.isObject())
            t.album = albumVal.toObject().value(QStringLiteral("name")).toString();
        else if (albumVal.isString())
            t.album = albumVal.toString();
        t.duration = parseDuration(item.value(QStringLiteral("duration")));
        t.thumbnail = thumbFromJson(item.value(QStringLiteral("thumbnails")));
        if (t.thumbnail.isEmpty())
            t.thumbnail = thumbFromJson(item.value(QStringLiteral("thumbnail"))); // hasil watch/radio
        if (t.thumbnail.isEmpty())
            t.thumbnail = videoThumb(t.videoId);
        return t;
    }

    // Dari dict yang diserialkan bridge (to_dict).
    static Track fromDict(const QJsonObject &d)
    {
        Track t;
        t.videoId = d.value(QStringLiteral("video_id")).toString();
        t.title = d.value(QStringLiteral("title")).toString();
        if (t.title.isEmpty())
            t.title = TR("Tanpa judul");
        const QJsonArray arr = d.value(QStringLiteral("artists")).toArray();
        for (const QJsonValue &v : arr)
            if (v.isString())
                t.artists << v.toString();
        const QJsonArray ids = d.value(QStringLiteral("artist_ids")).toArray();
        for (const QJsonValue &v : ids)
            if (v.isString())
                t.artistIds << v.toString();
        t.album = d.value(QStringLiteral("album")).toString();
        t.duration = parseDuration(d.value(QStringLiteral("duration")));
        t.thumbnail = thumbFromJson(d.value(QStringLiteral("thumbnail")));
        if (t.thumbnail.isEmpty())
            t.thumbnail = thumbFromJson(d.value(QStringLiteral("thumbnails")));
        if (t.thumbnail.isEmpty())
            t.thumbnail = videoThumb(t.videoId);
        return t;
    }

    QJsonObject toDict() const
    {
        QJsonObject o;
        o[QStringLiteral("video_id")] = videoId;
        o[QStringLiteral("title")] = title;
        o[QStringLiteral("artists")] = QJsonArray::fromStringList(artists);
        o[QStringLiteral("artist_ids")] = QJsonArray::fromStringList(artistIds);
        o[QStringLiteral("album")] = album;
        o[QStringLiteral("duration")] = duration >= 0 ? QJsonValue(duration) : QJsonValue();
        o[QStringLiteral("thumbnail")] = thumbnail;
        return o;
    }

    static QList<Track> listFromJson(const QJsonArray &arr)
    {
        QList<Track> out;
        for (const QJsonValue &v : arr) {
            if (!v.isObject())
                continue;
            Track t = fromDict(v.toObject());
            if (t.isValid())
                out << t;
        }
        return out;
    }

    bool operator==(const Track &o) const
    {
        return videoId == o.videoId && title == o.title;
    }
};

// ---------------------------------------------------------------------------
// Album / Playlist (kartu)
// ---------------------------------------------------------------------------
struct AlbumRef {
    QString browseId;
    QString title;
    QStringList artists;
    QString year;
    QString thumbnail;

    QString artistStr() const
    {
        return artists.isEmpty() ? QStringLiteral("Album") : artists.join(QStringLiteral(", "));
    }
};

struct PlaylistRef {
    QString playlistId;   // ID lokal (untuk playlist pengguna)
    QString browseId;     // ID YouTube Music (untuk playlist daring)
    QString title;
    QString subtitle;
    QString thumbnail;
    int count = 0;
    bool remote = false;
};

} // namespace bc
