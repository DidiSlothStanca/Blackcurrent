#pragma once

#include "track.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

namespace bc {

struct Playlist {
    QString id;
    QString name;
    QString description;
    qint64 createdAt = 0;
    QList<Track> tracks;
};

struct GenreStat {
    QString title;
    QString params; // parameter kategori YouTube Music (boleh kosong)
    int plays = 0;
};

struct ArtistStat {
    QString name;
    QString id; // browseId artis (boleh kosong)
    int plays = 0;
};

// Penyimpanan JSON lokal untuk playlist, koleksi, riwayat, dan preferensi.
class Storage
{
public:
    explicit Storage(const QString &dataDir = QString());

    QString dataDir() const { return m_dir; }

    // ----- playlist -----
    QList<Playlist> playlists() const;
    bool getPlaylist(const QString &id, Playlist *out) const;
    Playlist createPlaylist(const QString &name, const QString &description = QString());
    bool renamePlaylist(const QString &id, const QString &name,
                        const QString &description = QString());
    bool deletePlaylist(const QString &id);
    bool addToPlaylist(const QString &id, const Track &track);
    bool removeFromPlaylist(const QString &id, const QString &videoId);
    bool playlistContains(const QString &id, const QString &videoId) const;

    // ----- lagu disukai -----
    QList<Track> likedTracks() const;
    bool isLiked(const QString &videoId) const;
    bool toggleLike(const Track &track); // true bila kini disukai

    // ----- riwayat -----
    QList<Track> history(int limit = 0) const;
    void addHistory(const Track &track);
    void clearHistory();

    // ----- album disukai -----
    QList<AlbumRef> likedAlbums() const;
    bool isAlbumLiked(const QString &browseId) const;
    bool toggleLikeAlbum(const AlbumRef &album);

    // ----- hitungan putar -----
    int incrementPlaycount(const QString &videoId);
    int playcount(const QString &videoId) const;
    QList<QPair<QString, int>> topPlayed(int limit = 20) const;

    // ----- profil selera (genre & artis yang sering diputar) -----
    // Catat satu pemutaran. `contextGenre` = nama genre bila lagu diputar dari
    // halaman genre (kosong bila dari tempat lain).
    void recordListen(const Track &track, const QString &contextGenre = QString());
    // Simpan parameter kategori agar bisa dipakai lagi untuk rekomendasi.
    void noteGenre(const QString &title, const QString &params);
    // Pelajari hubungan artis -> genre dari isi halaman genre yang dibuka.
    void noteGenreTracks(const QString &title, const QList<Track> &tracks);
    QList<GenreStat> topGenres(int limit = 5) const;
    QList<ArtistStat> topArtists(int limit = 5) const;
    int genrePlays(const QString &title) const;

    // ----- histori pencarian -----
    QStringList searchHistory() const;
    void addSearchHistory(const QString &query);
    void clearSearchHistory();

    // ----- pengaturan -----
    QJsonValue setting(const QString &key, const QJsonValue &fallback = QJsonValue()) const;
    void setSetting(const QString &key, const QJsonValue &value);
    void setSetting(const QString &key, int value);
    void setSetting(const QString &key, double value);
    void setSetting(const QString &key, bool value);
    void setSetting(const QString &key, const QString &value);

    // ----- playlist terakhir (untuk "terakhir diputar") -----
    QString lastPlaylistId() const;

    static constexpr int kMaxHistory = 100;
    static constexpr int kMaxSearchHistory = 20;

private:
    QString pathFor(const QString &file) const;
    QJsonArray loadArray(const QString &file) const;
    void saveArray(const QString &file, const QJsonArray &arr) const;
    QJsonObject loadObject(const QString &file) const;
    void saveObject(const QString &file, const QJsonObject &obj) const;
    static Playlist playlistFromJson(const QJsonObject &o);

    QString m_dir;
    QJsonArray m_playlists;
    QJsonArray m_liked;
    QJsonArray m_likedAlbums;
    QJsonArray m_history;
    QJsonObject m_settings;
    QJsonObject m_playcounts;
    QJsonObject m_taste; // {"genres": {judul: {plays, params}}, "artists": {nama: {plays, id, genres: {judul: n}}}}
    QJsonArray m_searchHistory;
};

} // namespace bc
