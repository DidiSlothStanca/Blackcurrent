#include "i18n.h"
#include "storage.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QPair>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace bc {
namespace {

QJsonObject trackToJson(const Track &t) { return t.toDict(); }

Track trackFromJson(const QJsonObject &o) { return Track::fromDict(o); }

} // namespace

namespace {

// Folder data bawaan: ~/.config/blackcurrant-music-player (menghormati XDG_CONFIG_HOME).
// Tidak lagi di sebelah file eksekusi, sebab di AppImage folder itu read-only.
QString defaultDataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/blackcurrant-music-player");
}

// Versi lama menyimpan data di <folder aplikasi>/data. Salin file JSON-nya sekali
// saja ke folder baru (tidak menimpa file yang sudah ada).
void migrateLegacyData(const QString &newDir)
{
    const QDir legacy(QCoreApplication::applicationDirPath() + QStringLiteral("/data"));
    if (!legacy.exists() || legacy.absolutePath() == QDir(newDir).absolutePath())
        return;
    const QStringList files = legacy.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &f : files) {
        const QString dst = newDir + QLatin1Char('/') + f;
        if (!QFileInfo::exists(dst))
            QFile::copy(legacy.filePath(f), dst);
    }
}

} // namespace

Storage::Storage(const QString &dataDir)
{
    bool usingDefaultDir = false;
    if (!dataDir.isEmpty()) {
        m_dir = dataDir;
    } else {
        const QByteArray env = qgetenv("BC_DATA_DIR");
        if (!env.isEmpty())
            m_dir = QString::fromLocal8Bit(env);
        else {
            m_dir = defaultDataDir();
            usingDefaultDir = true;
        }
    }
    QDir().mkpath(m_dir);
    if (usingDefaultDir)
        migrateLegacyData(m_dir);

    m_playlists = loadArray(QStringLiteral("playlists.json"));
    m_liked = loadArray(QStringLiteral("liked.json"));
    m_likedAlbums = loadArray(QStringLiteral("liked_albums.json"));
    m_history = loadArray(QStringLiteral("history.json"));
    m_settings = loadObject(QStringLiteral("settings.json"));
    m_playcounts = loadObject(QStringLiteral("playcounts.json"));
    m_taste = loadObject(QStringLiteral("taste.json"));
    m_searchHistory = loadArray(QStringLiteral("search_history.json"));

    if (!m_settings.contains(QStringLiteral("volume")))
        m_settings[QStringLiteral("volume")] = 80;
    if (!m_settings.contains(QStringLiteral("last_view")))
        m_settings[QStringLiteral("last_view")] = QStringLiteral("home");
}

QString Storage::pathFor(const QString &file) const
{
    return m_dir + QLatin1Char('/') + file;
}

QJsonArray Storage::loadArray(const QString &file) const
{
    const QJsonObject o = loadObject(file);
    if (o.contains(QStringLiteral("__array")))
        return o.value(QStringLiteral("__array")).toArray();
    return {};
}

void Storage::saveArray(const QString &file, const QJsonArray &arr) const
{
    QJsonObject wrapper;
    wrapper[QStringLiteral("__array")] = arr;
    saveObject(file, wrapper);
}

QJsonObject Storage::loadObject(const QString &file) const
{
    QFile f(pathFor(file));
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = f.readAll();
    f.close();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError)
        return {};
    if (doc.isArray()) {
        QJsonObject wrapper;
        wrapper[QStringLiteral("__array")] = doc.array();
        return wrapper;
    }
    return doc.object();
}

void Storage::saveObject(const QString &file, const QJsonObject &obj) const
{
    // Bila pembungkus array, simpan sebagai array agar sama dengan versi Python.
    QByteArray data;
    if (obj.contains(QStringLiteral("__array")) && obj.size() == 1) {
        data = QJsonDocument(obj.value(QStringLiteral("__array")).toArray()).toJson(QJsonDocument::Indented);
    } else {
        data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    }
    const QString tmp = pathFor(file) + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(data);
    f.close();
    QFile::remove(pathFor(file));
    QFile::rename(tmp, pathFor(file));
}

// --------------------------------------------------------------- playlist

Playlist Storage::playlistFromJson(const QJsonObject &o)
{
    Playlist p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.name = o.value(QStringLiteral("name")).toString();
    p.description = o.value(QStringLiteral("description")).toString();
    p.createdAt = static_cast<qint64>(o.value(QStringLiteral("created_at")).toDouble());
    const QJsonArray tracks = o.value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue &v : tracks) {
        if (!v.isObject())
            continue;
        const Track t = Track::fromDict(v.toObject());
        if (t.isValid())
            p.tracks << t;
    }
    return p;
}

QList<Playlist> Storage::playlists() const
{
    QList<Playlist> out;
    for (const QJsonValue &v : m_playlists) {
        if (v.isObject())
            out << playlistFromJson(v.toObject());
    }
    return out;
}

bool Storage::getPlaylist(const QString &id, Playlist *out) const
{
    for (const QJsonValue &v : m_playlists) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == id) {
            if (out)
                *out = playlistFromJson(o);
            return true;
        }
    }
    return false;
}

Playlist Storage::createPlaylist(const QString &name, const QString &description)
{
    Playlist p;
    p.id = QString::number(QRandomGenerator::global()->generate64(), 16).right(12);
    p.name = name.trimmed().isEmpty() ? TR("Playlist tanpa judul") : name.trimmed();
    p.description = description.trimmed();
    p.createdAt = QDateTime::currentSecsSinceEpoch();

    QJsonObject o;
    o[QStringLiteral("id")] = p.id;
    o[QStringLiteral("name")] = p.name;
    o[QStringLiteral("description")] = p.description;
    o[QStringLiteral("created_at")] = static_cast<double>(p.createdAt);
    o[QStringLiteral("tracks")] = QJsonArray();
    m_playlists.append(o);
    saveArray(QStringLiteral("playlists.json"), m_playlists);
    return p;
}

bool Storage::renamePlaylist(const QString &id, const QString &name, const QString &description)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        QJsonObject o = m_playlists.at(i).toObject();
        if (o.value(QStringLiteral("id")).toString() != id)
            continue;
        const QString trimmed = name.trimmed();
        if (!trimmed.isEmpty())
            o[QStringLiteral("name")] = trimmed;
        o[QStringLiteral("description")] = description.trimmed();
        m_playlists.replace(i, o);
        saveArray(QStringLiteral("playlists.json"), m_playlists);
        return true;
    }
    return false;
}

bool Storage::deletePlaylist(const QString &id)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
            m_playlists.removeAt(i);
            saveArray(QStringLiteral("playlists.json"), m_playlists);
            return true;
        }
    }
    return false;
}

bool Storage::playlistContains(const QString &id, const QString &videoId) const
{
    for (const QJsonValue &v : m_playlists) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() != id)
            continue;
        for (const QJsonValue &tv : o.value(QStringLiteral("tracks")).toArray()) {
            if (tv.toObject().value(QStringLiteral("video_id")).toString() == videoId)
                return true;
        }
    }
    return false;
}

bool Storage::addToPlaylist(const QString &id, const Track &track)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        QJsonObject o = m_playlists.at(i).toObject();
        if (o.value(QStringLiteral("id")).toString() != id)
            continue;
        QJsonArray tracks = o.value(QStringLiteral("tracks")).toArray();
        for (const QJsonValue &tv : tracks) {
            if (tv.toObject().value(QStringLiteral("video_id")).toString() == track.videoId)
                return false; // sudah ada
        }
        tracks.append(track.toDict());
        o[QStringLiteral("tracks")] = tracks;
        m_playlists.replace(i, o);
        saveArray(QStringLiteral("playlists.json"), m_playlists);
        return true;
    }
    return false;
}

bool Storage::removeFromPlaylist(const QString &id, const QString &videoId)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        QJsonObject o = m_playlists.at(i).toObject();
        if (o.value(QStringLiteral("id")).toString() != id)
            continue;
        const QJsonArray tracks = o.value(QStringLiteral("tracks")).toArray();
        QJsonArray kept;
        for (const QJsonValue &tv : tracks) {
            if (tv.toObject().value(QStringLiteral("video_id")).toString() != videoId)
                kept.append(tv);
        }
        if (kept.size() == tracks.size())
            return false;
        o[QStringLiteral("tracks")] = kept;
        m_playlists.replace(i, o);
        saveArray(QStringLiteral("playlists.json"), m_playlists);
        return true;
    }
    return false;
}

// ------------------------------------------------------------ lagu disukai

QList<Track> Storage::likedTracks() const
{
    QList<Track> out;
    for (const QJsonValue &v : m_liked) {
        const Track t = Track::fromDict(v.toObject());
        if (t.isValid())
            out << t;
    }
    return out;
}

bool Storage::isLiked(const QString &videoId) const
{
    for (const QJsonValue &v : m_liked) {
        if (v.toObject().value(QStringLiteral("video_id")).toString() == videoId)
            return true;
    }
    return false;
}

bool Storage::toggleLike(const Track &track)
{
    for (int i = 0; i < m_liked.size(); ++i) {
        if (m_liked.at(i).toObject().value(QStringLiteral("video_id")).toString() == track.videoId) {
            m_liked.removeAt(i);
            saveArray(QStringLiteral("liked.json"), m_liked);
            return false;
        }
    }
    m_liked.prepend(track.toDict());
    saveArray(QStringLiteral("liked.json"), m_liked);
    return true;
}

// ----------------------------------------------------------------- riwayat

QList<Track> Storage::history(int limit) const
{
    QList<Track> out;
    for (const QJsonValue &v : m_history) {
        const Track t = Track::fromDict(v.toObject());
        if (t.isValid())
            out << t;
    }
    if (limit > 0 && out.size() > limit)
        out = out.mid(0, limit);
    return out;
}

void Storage::addHistory(const Track &track)
{
    QJsonArray next;
    for (const QJsonValue &v : m_history) {
        if (v.toObject().value(QStringLiteral("video_id")).toString() != track.videoId)
            next.append(v);
    }
    next.prepend(track.toDict());
    while (next.size() > kMaxHistory)
        next.removeLast();
    m_history = next;
    saveArray(QStringLiteral("history.json"), m_history);
}

void Storage::clearHistory()
{
    m_history = QJsonArray();
    saveArray(QStringLiteral("history.json"), m_history);
}

// --------------------------------------------------------- album disukai

QList<AlbumRef> Storage::likedAlbums() const
{
    QList<AlbumRef> out;
    for (const QJsonValue &v : m_likedAlbums) {
        const QJsonObject o = v.toObject();
        AlbumRef a;
        a.browseId = o.value(QStringLiteral("browse_id")).toString();
        a.title = o.value(QStringLiteral("title")).toString();
        const QString artist = o.value(QStringLiteral("artist")).toString();
        if (!artist.isEmpty())
            a.artists << artist;
        a.thumbnail = o.value(QStringLiteral("thumbnail")).toString();
        out << a;
    }
    return out;
}

bool Storage::isAlbumLiked(const QString &browseId) const
{
    for (const QJsonValue &v : m_likedAlbums) {
        if (v.toObject().value(QStringLiteral("browse_id")).toString() == browseId)
            return true;
    }
    return false;
}

bool Storage::toggleLikeAlbum(const AlbumRef &album)
{
    if (album.browseId.isEmpty())
        return false;
    for (int i = 0; i < m_likedAlbums.size(); ++i) {
        if (m_likedAlbums.at(i).toObject().value(QStringLiteral("browse_id")).toString() == album.browseId) {
            m_likedAlbums.removeAt(i);
            saveArray(QStringLiteral("liked_albums.json"), m_likedAlbums);
            return false;
        }
    }
    QJsonObject o;
    o[QStringLiteral("browse_id")] = album.browseId;
    o[QStringLiteral("title")] = album.title.isEmpty() ? QStringLiteral("Album") : album.title;
    o[QStringLiteral("artist")] = album.artists.isEmpty() ? QString() : album.artists.first();
    o[QStringLiteral("thumbnail")] = album.thumbnail;
    m_likedAlbums.prepend(o);
    saveArray(QStringLiteral("liked_albums.json"), m_likedAlbums);
    return true;
}

// ----------------------------------------------------------- hitungan putar

int Storage::incrementPlaycount(const QString &videoId)
{
    const int count = m_playcounts.value(videoId).toInt(0) + 1;
    m_playcounts[videoId] = count;
    saveObject(QStringLiteral("playcounts.json"), m_playcounts);
    return count;
}

int Storage::playcount(const QString &videoId) const
{
    return m_playcounts.value(videoId).toInt(0);
}

QList<QPair<QString, int>> Storage::topPlayed(int limit) const
{
    QList<QPair<QString, int>> items;
    for (auto it = m_playcounts.begin(); it != m_playcounts.end(); ++it)
        items << qMakePair(it.key(), it.value().toInt());
    std::sort(items.begin(), items.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });
    if (limit > 0 && items.size() > limit)
        items = items.mid(0, limit);
    return items;
}

// ------------------------------------------------------- profil selera

namespace {
const QString kGenres = QStringLiteral("genres");
const QString kArtists = QStringLiteral("artists");

// Cocokkan judul genre tanpa peduli huruf besar/kecil.
QString matchKey(const QJsonObject &obj, const QString &title)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.key().compare(title, Qt::CaseInsensitive) == 0)
            return it.key();
    }
    return title;
}
} // namespace

void Storage::noteGenre(const QString &title, const QString &params)
{
    const QString t = title.trimmed();
    if (t.isEmpty())
        return;
    QJsonObject genres = m_taste.value(kGenres).toObject();
    const QString key = matchKey(genres, t);
    QJsonObject g = genres.value(key).toObject();
    if (!params.isEmpty() && g.value(QStringLiteral("params")).toString() == params)
        return; // tidak ada perubahan
    if (!params.isEmpty())
        g[QStringLiteral("params")] = params;
    if (!g.contains(QStringLiteral("plays")))
        g[QStringLiteral("plays")] = 0;
    genres[key] = g;
    m_taste[kGenres] = genres;
    saveObject(QStringLiteral("taste.json"), m_taste);
}

void Storage::noteGenreTracks(const QString &title, const QList<Track> &tracks)
{
    const QString t = title.trimmed();
    if (t.isEmpty() || tracks.isEmpty())
        return;
    QJsonObject genres = m_taste.value(kGenres).toObject();
    const QString gkey = matchKey(genres, t);
    QJsonObject artists = m_taste.value(kArtists).toObject();
    for (const Track &tr : tracks) {
        for (int i = 0; i < tr.artists.size(); ++i) {
            const QString name = tr.artists.at(i);
            if (name.isEmpty())
                continue;
            QJsonObject a = artists.value(name).toObject();
            QJsonObject ag = a.value(QStringLiteral("genres")).toObject();
            ag[gkey] = ag.value(gkey).toInt(0) + 1;
            a[QStringLiteral("genres")] = ag;
            if (i < tr.artistIds.size() && !tr.artistIds.at(i).isEmpty())
                a[QStringLiteral("id")] = tr.artistIds.at(i);
            artists[name] = a;
        }
    }
    QJsonObject g = genres.value(gkey).toObject();
    if (!g.contains(QStringLiteral("plays")))
        g[QStringLiteral("plays")] = 0;
    genres[gkey] = g;
    m_taste[kGenres] = genres;
    m_taste[kArtists] = artists;
    saveObject(QStringLiteral("taste.json"), m_taste);
}

void Storage::recordListen(const Track &track, const QString &contextGenre)
{
    QJsonObject genres = m_taste.value(kGenres).toObject();
    QJsonObject artists = m_taste.value(kArtists).toObject();

    // 1) Hitung artis.
    for (int i = 0; i < track.artists.size(); ++i) {
        const QString name = track.artists.at(i);
        if (name.isEmpty())
            continue;
        QJsonObject a = artists.value(name).toObject();
        a[QStringLiteral("plays")] = a.value(QStringLiteral("plays")).toInt(0) + 1;
        if (i < track.artistIds.size() && !track.artistIds.at(i).isEmpty())
            a[QStringLiteral("id")] = track.artistIds.at(i);
        artists[name] = a;
    }

    // 2) Tentukan genre lagu ini.
    QString genre = contextGenre.trimmed();
    if (!genre.isEmpty()) {
        genre = matchKey(genres, genre);
        // Diputar langsung dari halaman genre: perkuat hubungan artis -> genre.
        for (const QString &name : track.artists) {
            if (name.isEmpty())
                continue;
            QJsonObject a = artists.value(name).toObject();
            QJsonObject ag = a.value(QStringLiteral("genres")).toObject();
            ag[genre] = ag.value(genre).toInt(0) + 2;
            a[QStringLiteral("genres")] = ag;
            artists[name] = a;
        }
    } else {
        // Dari tempat lain: tebak dari genre yang paling sering dikaitkan dgn artisnya.
        QHash<QString, int> score;
        for (const QString &name : track.artists) {
            const QJsonObject ag =
                artists.value(name).toObject().value(QStringLiteral("genres")).toObject();
            for (auto it = ag.begin(); it != ag.end(); ++it)
                score[it.key()] += it.value().toInt(0);
        }
        int best = 0;
        for (auto it = score.begin(); it != score.end(); ++it) {
            if (it.value() > best) {
                best = it.value();
                genre = it.key();
            }
        }
    }

    if (!genre.isEmpty()) {
        QJsonObject g = genres.value(genre).toObject();
        g[QStringLiteral("plays")] = g.value(QStringLiteral("plays")).toInt(0) + 1;
        genres[genre] = g;
    }

    m_taste[kGenres] = genres;
    m_taste[kArtists] = artists;
    saveObject(QStringLiteral("taste.json"), m_taste);
}

QList<GenreStat> Storage::topGenres(int limit) const
{
    QList<GenreStat> out;
    const QJsonObject genres = m_taste.value(kGenres).toObject();
    for (auto it = genres.begin(); it != genres.end(); ++it) {
        const QJsonObject g = it.value().toObject();
        const int plays = g.value(QStringLiteral("plays")).toInt(0);
        if (plays <= 0)
            continue;
        GenreStat s;
        s.title = it.key();
        s.params = g.value(QStringLiteral("params")).toString();
        s.plays = plays;
        out << s;
    }
    std::sort(out.begin(), out.end(), [](const GenreStat &a, const GenreStat &b) {
        if (a.plays != b.plays)
            return a.plays > b.plays;
        return a.title < b.title;
    });
    if (limit > 0 && out.size() > limit)
        out = out.mid(0, limit);
    return out;
}

int Storage::genrePlays(const QString &title) const
{
    const QJsonObject genres = m_taste.value(kGenres).toObject();
    for (auto it = genres.begin(); it != genres.end(); ++it) {
        if (it.key().compare(title, Qt::CaseInsensitive) == 0)
            return it.value().toObject().value(QStringLiteral("plays")).toInt(0);
    }
    return 0;
}

QList<ArtistStat> Storage::topArtists(int limit) const
{
    QList<ArtistStat> out;
    const QJsonObject artists = m_taste.value(kArtists).toObject();
    for (auto it = artists.begin(); it != artists.end(); ++it) {
        const QJsonObject a = it.value().toObject();
        const int plays = a.value(QStringLiteral("plays")).toInt(0);
        if (plays <= 0)
            continue;
        ArtistStat s;
        s.name = it.key();
        s.id = a.value(QStringLiteral("id")).toString();
        s.plays = plays;
        out << s;
    }
    std::sort(out.begin(), out.end(), [](const ArtistStat &a, const ArtistStat &b) {
        if (a.plays != b.plays)
            return a.plays > b.plays;
        return a.name < b.name;
    });
    if (limit > 0 && out.size() > limit)
        out = out.mid(0, limit);
    return out;
}

// -------------------------------------------------------- histori pencarian

QStringList Storage::searchHistory() const
{
    QStringList out;
    for (const QJsonValue &v : m_searchHistory) {
        if (v.isString())
            out << v.toString();
    }
    return out;
}

void Storage::addSearchHistory(const QString &query)
{
    const QString q = query.trimmed();
    if (q.isEmpty())
        return;
    QJsonArray next;
    for (const QJsonValue &v : m_searchHistory) {
        if (v.isString() && v.toString().compare(q, Qt::CaseSensitive) != 0)
            next.append(v);
    }
    next.prepend(q);
    while (next.size() > kMaxSearchHistory)
        next.removeLast();
    m_searchHistory = next;
    saveArray(QStringLiteral("search_history.json"), m_searchHistory);
}

void Storage::clearSearchHistory()
{
    m_searchHistory = QJsonArray();
    saveArray(QStringLiteral("search_history.json"), m_searchHistory);
}

// -------------------------------------------------------------- pengaturan

QJsonValue Storage::setting(const QString &key, const QJsonValue &fallback) const
{
    if (m_settings.contains(key))
        return m_settings.value(key);
    return fallback;
}

void Storage::setSetting(const QString &key, const QJsonValue &value)
{
    m_settings[key] = value;
    saveObject(QStringLiteral("settings.json"), m_settings);
}

void Storage::setSetting(const QString &key, int value) { setSetting(key, QJsonValue(value)); }
void Storage::setSetting(const QString &key, double value) { setSetting(key, QJsonValue(value)); }
void Storage::setSetting(const QString &key, bool value) { setSetting(key, QJsonValue(value)); }
void Storage::setSetting(const QString &key, const QString &value) { setSetting(key, QJsonValue(value)); }

QString Storage::lastPlaylistId() const
{
    return m_settings.value(QStringLiteral("last_playlist")).toString();
}

} // namespace bc
