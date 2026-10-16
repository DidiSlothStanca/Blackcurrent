#include "i18n.h"
#include "views/artistview.h"

#include "bridge.h"
#include "flowlayout.h"
#include "mainwindow.h"
#include "storage.h"
#include "theme.h"
#include "views/viewutil.h"
#include "widgets.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace bc {

ArtistView::ArtistView(MainWindow *app, const QString &browseId, QWidget *parent)
    : QWidget(parent), m_app(app), m_browseId(browseId)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_scroll = new ScrollArea(this);
    root->addWidget(m_scroll);
    m_scroll->addWidget(makeLoadingLabel(TR("Memuat artis…")));
    load();
}

void ArtistView::load()
{
    QJsonObject params;
    params[QStringLiteral("browse_id")] = m_browseId;
    m_app->bridge()->request(
        QStringLiteral("get_artist"), params,
        [this](bool ok, const QJsonValue &result, const QString &error) {
            m_scroll->clear();
            if (!ok || !result.isObject()) {
                m_scroll->addWidget(
                    makeEmptyLabel(TR("Gagal memuat artis: ") + error));
                return;
            }
            const QJsonObject data = result.toObject();
            QString name = data.value(QStringLiteral("name")).toString();
            if (name.isEmpty())
                name = data.value(QStringLiteral("title")).toString();
            const QString subscribers = data.value(QStringLiteral("subscribers")).toString();
            QString thumb;
            const QJsonArray thumbs = data.value(QStringLiteral("thumbnails")).toArray();
            if (!thumbs.isEmpty())
                thumb = upscaleThumb(
                    thumbs.last().toObject().value(QStringLiteral("url")).toString());

            const QJsonObject songsObj = data.value(QStringLiteral("songs")).toObject();
            const QJsonArray songResults = songsObj.value(QStringLiteral("results")).toArray();
            m_topSongs.clear();
            for (const QJsonValue &v : songResults) {
                if (v.isObject()) {
                    Track t = Track::fromSearch(v.toObject());
                    if (t.isValid())
                        m_topSongs << t;
                }
            }

            QList<AlbumRef> albums;
            for (const QString &key : {QStringLiteral("albums"), QStringLiteral("singles")}) {
                const QJsonObject shelf = data.value(key).toObject();
                for (const QJsonValue &v : shelf.value(QStringLiteral("results")).toArray()) {
                    const QJsonObject a = v.toObject();
                    AlbumRef ref;
                    ref.browseId = a.value(QStringLiteral("browseId")).toString();
                    ref.title = a.value(QStringLiteral("title")).toString();
                    ref.year = a.value(QStringLiteral("year")).toString();
                    const QJsonArray at = a.value(QStringLiteral("thumbnails")).toArray();
                    if (!at.isEmpty())
                        ref.thumbnail = upscaleThumb(
                            at.last().toObject().value(QStringLiteral("url")).toString());
                    if (!ref.browseId.isEmpty())
                        albums << ref;
                }
            }

            auto *playBtn = makeButton(icons::Kind::Play, TR("Putar"), QStringLiteral("primaryBtn"), icons::Role::OnAccent);
            const QList<Track> topCopy = m_topSongs;
            connect(playBtn, &QPushButton::clicked, this, [this, topCopy]() {
                if (!topCopy.isEmpty())
                    m_app->playContext(topCopy, 0);
            });
            QList<QWidget *> buttons{playBtn};
            m_scroll->addWidget(makeDetailHeader(
                name, subscribers.isEmpty() ? TR("Artis") : subscribers, thumb,
                buttons));

            // Lagu Teratas
            if (!m_topSongs.isEmpty()) {
                m_scroll->addWidget(makeSectionHeader(TR("Lagu Teratas"),
                                                      TR("Paling populer")));
                const QList<Track> top = m_topSongs.mid(0, qMin(5, m_topSongs.size()));
                const QList<SongRow *> rows =
                    makeTrackRows(m_app, top, false, true, m_scroll->body());
                for (SongRow *r : rows)
                    m_scroll->addWidget(r);
            }

            // Album & Single
            if (!albums.isEmpty()) {
                m_scroll->addWidget(makeSectionHeader(TR("Album & Single"),
                                                      TR("Rilis dari artis ini")));
                // Kisi responsif: kartu berukuran tetap & rapat, jumlah kolom
                // mengikuti lebar jendela (tidak lagi melebar/renggang).
                auto *grid = new QWidget;
                auto *gl = new FlowLayout(grid, 10, 10);
                gl->setContentsMargins(24, 0, 24, 10);
                for (const AlbumRef &a : albums) {
                    auto *card = new PlaylistCard(
                        a.title, a.year.isEmpty() ? QStringLiteral("Album") : a.year, a.thumbnail,
                        kCardWidth, grid);
                    const QString bid = a.browseId;
                    connect(card, &PlaylistCard::activated, this,
                            [this, bid]() { m_app->openAlbum(bid); });
                    gl->addWidget(card);
                }
                m_scroll->addWidget(grid);
            }

            // Semua Lagu
            const QString songsBrowseId = songsObj.value(QStringLiteral("browseId")).toString();
            m_scroll->addWidget(makeSectionHeader(TR("Semua Lagu"),
                                                  TR("Semua lagu dari artis ini")));
            m_allSongsSection = new QWidget;
            auto *al = new QVBoxLayout(m_allSongsSection);
            al->setContentsMargins(0, 0, 0, 0);
            al->setSpacing(0);
            m_scroll->addWidget(m_allSongsSection);

            const QList<Track> initial = m_topSongs;
            const QList<SongRow *> rows =
                makeTrackRows(m_app, initial, false, true, m_allSongsSection);
            for (SongRow *r : rows)
                al->addWidget(r);

            if (!songsBrowseId.isEmpty()) {
                auto *moreBtn =
                    makeButton(TR("Muat semua lagu"), QStringLiteral("ghostBtn"));
                const QString bid = songsBrowseId;
                connect(moreBtn, &QPushButton::clicked, this,
                        [this, bid]() { loadAllSongs(bid); });
                auto *wrap = new QWidget;
                auto *wl = new QHBoxLayout(wrap);
                wl->setContentsMargins(28, 6, 28, 10);
                wl->addWidget(moreBtn);
                wl->addStretch(1);
                al->addWidget(wrap);
            }
        }, this);
}

void ArtistView::loadAllSongs(const QString &browseId)
{
    QJsonObject params;
    params[QStringLiteral("browse_id")] = browseId;
    params[QStringLiteral("limit")] = 200;
    m_app->bridge()->request(
        QStringLiteral("get_playlist"), params,
        [this](bool ok, const QJsonValue &result, const QString &) {
            if (!ok || !result.isObject() || !m_allSongsSection)
                return;
            QList<Track> tracks;
            for (const QJsonValue &v : result.toObject().value(QStringLiteral("tracks")).toArray()) {
                const QJsonObject o = v.toObject();
                if (o.value(QStringLiteral("videoId")).toString().isEmpty())
                    continue;
                Track t = Track::fromSearch(o);
                if (t.isValid())
                    tracks << t;
            }
            if (tracks.isEmpty())
                return;
            auto *lay = qobject_cast<QVBoxLayout *>(m_allSongsSection->layout());
            clearLayout(lay);
            const QList<SongRow *> rows = makeTrackRows(m_app, tracks, false, true, m_allSongsSection);
            for (SongRow *r : rows)
                lay->addWidget(r);
        }, this);
}

} // namespace bc
