#include "i18n.h"
#include "views/homeview.h"

#include "bridge.h"
#include "mainwindow.h"
#include "storage.h"
#include "theme.h"
#include "widgets.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace bc {

namespace {
constexpr int kHScrollCards = 16;
constexpr int kRecentMax = 16;   // lagu di "Terakhir Diputar"
constexpr int kGridCols = 4;     // kolom ubin genre / pintasan
constexpr int kSuggestDelayMs = 250;
constexpr int kSuggestLimit = 8;

QString greetingText()
{
    const int hour = QDateTime::currentDateTime().time().hour();
    if (hour < 12)
        return TR("Selamat pagi");
    if (hour < 15)
        return TR("Selamat siang");
    if (hour < 18)
        return TR("Selamat sore");
    return TR("Selamat malam");
}
} // namespace

HomeView::HomeView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_search = new SearchBar(TR("Cari lagu, artis, atau album…"), this);
    root->addWidget(m_search);

    m_scroll = new ScrollArea(this);
    root->addWidget(m_scroll, 1);
    m_bodyLayout = m_scroll->contentLayout();

    auto *header = new QWidget;
    auto *hbox = new QVBoxLayout(header);
    hbox->setContentsMargins(28, 18, 28, 0);
    hbox->setSpacing(4);
    hbox->addWidget(makeLabel(greetingText(), QString(), 26, true));
    hbox->addWidget(makeLabel(
        TR("Siap mendengarkan? Semua lagu diputar langsung dari YouTube Music."),
        QStringLiteral("sub"), 10));
    m_bodyLayout->addWidget(header);

    m_quickSection = makeSection();
    m_recoSection = makeSection();
    m_recentSection = makeSection();
    m_shortcutSection = makeSection();
    m_albumSection = makeSection();
    m_genreSection = makeSection();
    m_historySection = makeSection();
    // Sisa tinggi layar diserap spacer di bawah. Tanpa ini, layout membagi sisa tinggi
    // ke semua bagian sehingga ukurannya jadi tidak seragam.
    m_bodyLayout->addStretch(1);

    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(kSuggestDelayMs);
    connect(m_suggestTimer, &QTimer::timeout, this,
            [this]() { fetchSuggestions(m_search->text().trimmed()); });

    connect(m_search, &SearchBar::textEdited, this, &HomeView::onTextEdited);
    connect(m_search, &SearchBar::submitted, this, [this](const QString &q) {
        m_search->hideSuggestions();
        if (q.isEmpty())
            m_app->showSearch();
        else
            m_app->navigate(QStringLiteral("search"), TR("Pencarian"), q);
    });
    connect(m_search, &SearchBar::suggestionActivated, this, [this](const QString &q) {
        m_search->hideSuggestions();
        m_app->navigate(QStringLiteral("search"), TR("Pencarian"), q);
    });

    refresh();
}

QWidget *HomeView::makeSection()
{
    auto *section = new QWidget;
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *lay = new QVBoxLayout(section);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    m_bodyLayout->addWidget(section);
    return section;
}

void HomeView::refresh()
{
    loadRecent();
    loadShortcuts();
    loadQuickPicks();
    loadRecommendations();
    loadAlbums();
    loadGenres();
    loadHistory();
    m_scroll->resetTop();
}

void HomeView::refreshHistory()
{
    loadRecent();
    loadHistory();
}

void HomeView::onTextEdited(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_search->hideSuggestions();
        return;
    }
    m_suggestTimer->start();
}

void HomeView::fetchSuggestions(const QString &query)
{
    if (query.isEmpty())
        return;
    const int seq = ++m_suggestSeq;
    QJsonObject params;
    params[QStringLiteral("query")] = query;
    params[QStringLiteral("limit")] = kSuggestLimit;
    m_app->bridge()->request(
        QStringLiteral("search_suggestions"), params,
        [this, seq, query](bool ok, const QJsonValue &result, const QString &) {
            if (seq != m_suggestSeq || !ok)
                return;
            if (m_search->text().trimmed() != query)
                return;
            QStringList items;
            for (const QJsonValue &v : result.toArray())
                if (v.isString())
                    items << v.toString();
            m_search->setSuggestions(items);
        }, this);
}

void HomeView::fillTrackHScroll(QWidget *section, const QString &title, const QString &subtitle,
                                const QList<Track> &tracks, const QString &actionText,
                                const std::function<void()> &action)
{
    auto *lay = qobject_cast<QVBoxLayout *>(section->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(title, subtitle, actionText, action));
    if (tracks.isEmpty()) {
        lay->addWidget(makeEmptyLabel(TR("Belum ada lagu untuk ditampilkan.")));
        return;
    }
    auto *hs = new HScrollArea(shelfHeightFor(kCardWidth), section);
    const QList<Track> queue = tracks.mid(0, kHScrollCards);
    for (int idx = 0; idx < queue.size(); ++idx) {
        auto *card = new SongCard(queue.at(idx), kCardWidth, hs->body());
        connect(card, &SongCard::activated, this,
                [this, queue, idx](const Track &) { m_app->playContext(queue, idx); });
        connect(card, &SongCard::menuRequested, this,
                [this](const Track &tr, const QPoint &pos) { m_app->showTrackMenu(tr, pos); });
        hs->contentLayout()->insertWidget(hs->contentLayout()->count() - 1, card);
    }
    lay->addWidget(hs);
}

// Grid 3 baris ke bawah x 3 kolom terlihat; sisanya (maks. 16 lagu) digulir ke samping.
void HomeView::fillTrackGrid(QWidget *section, const QString &title, const QString &subtitle,
                             const QList<Track> &tracks, const QString &actionText,
                             const std::function<void()> &action)
{
    auto *lay = qobject_cast<QVBoxLayout *>(section->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(title, subtitle, actionText, action));
    if (tracks.isEmpty()) {
        lay->addWidget(makeEmptyLabel(
            TR("Belum ada lagu yang diputar. Putar sebuah lagu dan ia akan muncul di sini.")));
        return;
    }
    const QList<Track> queue = tracks.mid(0, kRecentMax);
    auto *grid = new SongGridScroll(3, 3, section);
    grid->setTracks(queue);
    connect(grid, &SongGridScroll::trackActivated, this,
            [this, queue](int index) { m_app->playContext(queue, index); });
    connect(grid, &SongGridScroll::menuRequested, this,
            [this](const Track &tr, const QPoint &pos) { m_app->showTrackMenu(tr, pos); });
    lay->addWidget(grid);
}

void HomeView::fillError(QWidget *section, const QString &title, const QString &subtitle,
                         const QString &message, const std::function<void()> &retry)
{
    auto *lay = qobject_cast<QVBoxLayout *>(section->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(title, subtitle));
    auto *box = new QWidget;
    box->setObjectName(QStringLiteral("panel"));
    auto *bl = new QHBoxLayout(box);
    bl->setContentsMargins(16, 12, 16, 12);
    auto *lbl = makeLabel(message, QStringLiteral("sub"), 10);
    lbl->setWordWrap(true);
    bl->addWidget(lbl, 1);
    auto *btn = makeButton(TR("Coba lagi"), QStringLiteral("primaryBtn"));
    connect(btn, &QPushButton::clicked, this, [retry]() { retry(); });
    bl->addWidget(btn);
    auto *wrap = new QWidget;
    auto *wl = new QHBoxLayout(wrap);
    wl->setContentsMargins(24, 0, 24, 6);
    wl->addWidget(box);
    lay->addWidget(wrap);
}

void HomeView::loadRecent()
{
    const QList<Track> tracks = m_app->storage()->history(kRecentMax);
    const QList<Track> queue = tracks;
    fillTrackGrid(
        m_recentSection, TR("Terakhir Diputar"),
        TR("Lanjutkan dari lagu yang barusan kamu putar"), tracks,
        tracks.isEmpty() ? QString() : TR("Putar semua"),
        tracks.isEmpty()
            ? std::function<void()>()
            : std::function<void()>([this, queue]() { m_app->playContext(queue, 0); }));
}

void HomeView::loadShortcuts()
{
    auto *lay = qobject_cast<QVBoxLayout *>(m_shortcutSection->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(TR("Pintasan"),
                                     TR("Akses cepat ke koleksi kamu")));
    auto *grid = new QWidget;
    auto *gl = new QGridLayout(grid);
    gl->setContentsMargins(24, 0, 24, 8);
    gl->setSpacing(10);
    for (int c = 0; c < kGridCols; ++c)
        gl->setColumnStretch(c, 1);

    struct Item {
        QString title;
        QString color;
        std::function<void()> action;
    };
    const QList<Item> items = {
        {TR("Lagu Disukai"), theme::genreColors().at(0),
         [this]() { m_app->showLiked(); }},
        {TR("Album Disukai"), theme::genreColors().at(2),
         [this]() { m_app->showLikedAlbums(); }},
        {TR("Koleksi"), theme::genreColors().at(6),
         [this]() { m_app->showLibrary(); }},
        {TR("Riwayat Musik"), theme::genreColors().at(10),
         [this]() { m_app->showHistory(); }},
    };
    int i = 0;
    for (const auto &it : items) {
        auto *tile = new GenreTile(it.title, it.color, QString(), grid);
        connect(tile, &GenreTile::activated, this, [act = it.action]() { act(); });
        gl->addWidget(tile, i / kGridCols, i % kGridCols);
        ++i;
    }
    lay->addWidget(grid);
}

void HomeView::loadQuickPicks(bool personal)
{
    // "Pilihan Cepat" = lagu-lagu dari artis yang paling sering kamu putar.
    // Belum ada riwayat -> pakai pilihan cepat bawaan YouTube Music.
    const QList<ArtistStat> topArtists =
        personal ? m_app->storage()->topArtists(4) : QList<ArtistStat>();
    const bool byArtist = !topArtists.isEmpty();

    auto *lay = qobject_cast<QVBoxLayout *>(m_quickSection->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(TR("Pilihan Cepat"),
                                     TR("Memuat pilihan cepat…")));
    lay->addWidget(makeLoadingLabel(TR("Memuat pilihan cepat…")));

    QJsonObject params;
    params[QStringLiteral("limit")] = kHScrollCards;
    QString method = QStringLiteral("quick_picks");
    QString subtitle = TR("Pilihan hangat dari YouTube Music");
    if (byArtist) {
        QJsonArray arr;
        QStringList names;
        for (const ArtistStat &a : topArtists) {
            QJsonObject o;
            o[QStringLiteral("id")] = a.id;
            o[QStringLiteral("name")] = a.name;
            arr.append(o);
            if (names.size() < 3)
                names << a.name;
        }
        params[QStringLiteral("artists")] = arr;
        method = QStringLiteral("artist_songs");
        subtitle = TR("Dari artis yang paling sering kamu putar: %1")
                       .arg(names.join(QStringLiteral(", ")));
    }

    m_app->bridge()->request(
        method, params,
        [this, byArtist, subtitle](bool ok, const QJsonValue &result, const QString &error) {
            const QList<Track> tracks = ok ? Track::listFromJson(result.toArray()) : QList<Track>();
            if (byArtist && tracks.isEmpty()) {
                loadQuickPicks(false); // cadangan: pilihan cepat bawaan
                return;
            }
            if (!ok) {
                fillError(m_quickSection, TR("Pilihan Cepat"),
                          TR("Gagal memuat dari YouTube Music"), error,
                          [this]() { loadQuickPicks(); });
                return;
            }
            fillTrackHScroll(m_quickSection, TR("Pilihan Cepat"), subtitle, tracks,
                             TR("Segarkan"), [this]() { loadQuickPicks(); });
        }, this);
}

void HomeView::loadRecommendations()
{
    auto *lay = qobject_cast<QVBoxLayout *>(m_recoSection->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(TR("Rekomendasi Untukmu"),
                                     TR("Sedang menyusun pilihan…")));
    lay->addWidget(makeLoadingLabel(TR("Memuat rekomendasi…")));

    // Sumber 1..2: genre yang paling sering diputar. Sumber terakhir: lagu serupa
    // dari lagu favorit/terakhir (radio YouTube Music).
    const QList<GenreStat> genres = m_app->storage()->topGenres(2);

    const QList<QPair<QString, int>> top = m_app->storage()->topPlayed(20);
    const QList<Track> recent = m_app->storage()->history(10);
    QStringList seeds;
    for (int i = 0; i < top.size() && seeds.size() < 6; ++i)
        seeds << top.at(i).first;
    for (const Track &t : recent) {
        if (seeds.size() >= 6)
            break;
        if (!seeds.contains(t.videoId))
            seeds << t.videoId;
    }

    QString subtitle;
    if (!genres.isEmpty()) {
        QStringList names;
        for (const GenreStat &g : genres)
            names << g.title;
        subtitle = TR("Sesuai genre yang sering kamu putar: %1")
                       .arg(names.join(QStringLiteral(", ")));
    } else if (!seeds.isEmpty()) {
        subtitle = TR("Berdasarkan kebiasaan mendengarmu");
    } else {
        subtitle = TR("Populer dari YouTube Music");
    }

    struct Source {
        QString method;
        QJsonObject params;
    };
    QList<Source> sources;
    for (const GenreStat &g : genres) {
        Source src;
        src.method = QStringLiteral("genre_tracks");
        src.params[QStringLiteral("params")] = g.params;
        src.params[QStringLiteral("fallback_query")] = g.title;
        sources << src;
    }
    if (!seeds.isEmpty()) {
        Source src;
        src.method = QStringLiteral("recommendations");
        src.params[QStringLiteral("limit")] = kHScrollCards;
        src.params[QStringLiteral("seeds")] = QJsonArray::fromStringList(seeds);
        sources << src;
    }
    if (sources.isEmpty()) {
        Source src;
        src.method = QStringLiteral("quick_picks");
        src.params[QStringLiteral("limit")] = kHScrollCards;
        sources << src;
    }

    struct State {
        QList<QList<Track>> lists;
        int pending = 0;
        bool anyOk = false;
        QString error;
    };
    auto st = std::make_shared<State>();
    st->lists.resize(sources.size());
    st->pending = sources.size();

    auto finish = [this, st, subtitle]() {
        if (--st->pending > 0)
            return;
        if (!st->anyOk) {
            fillError(m_recoSection, TR("Rekomendasi Untukmu"),
                      TR("Gagal memuat dari YouTube Music"), st->error,
                      [this]() { loadRecommendations(); });
            return;
        }
        // Gabungkan bergantian (genre favorit lebih dulu), tanpa duplikat.
        QList<Track> merged;
        QSet<QString> seen;
        int longest = 0;
        for (const QList<Track> &l : st->lists)
            longest = qMax(longest, int(l.size()));
        for (int i = 0; i < longest && merged.size() < kHScrollCards; ++i) {
            for (const QList<Track> &l : st->lists) {
                if (i >= l.size() || seen.contains(l.at(i).videoId))
                    continue;
                seen.insert(l.at(i).videoId);
                merged << l.at(i);
                if (merged.size() >= kHScrollCards)
                    break;
            }
        }
        fillTrackHScroll(m_recoSection, TR("Rekomendasi Untukmu"), subtitle, merged,
                         TR("Segarkan"), [this]() { loadRecommendations(); });
    };

    for (int idx = 0; idx < sources.size(); ++idx) {
        m_app->bridge()->request(
            sources.at(idx).method, sources.at(idx).params,
            [st, idx, finish](bool ok, const QJsonValue &result, const QString &error) mutable {
                if (ok) {
                    st->lists[idx] = Track::listFromJson(result.toArray());
                    st->anyOk = true;
                } else if (st->error.isEmpty()) {
                    st->error = error;
                }
                finish();
            }, this);
    }
}

void HomeView::loadAlbums()
{
    auto *lay = qobject_cast<QVBoxLayout *>(m_albumSection->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(TR("Album Lagu"),
                                     TR("Album populer dari beranda YouTube Music")));
    lay->addWidget(makeLoadingLabel(TR("Memuat album…")));

    QJsonObject params;
    params[QStringLiteral("limit")] = 3;
    m_app->bridge()->request(
        QStringLiteral("home_shelves"), params,
        [this](bool ok, const QJsonValue &result, const QString &error) {
            if (!ok) {
                fillError(m_albumSection, TR("Album Lagu"),
                          TR("Gagal memuat daftar album"), error,
                          [this]() { loadAlbums(); });
                return;
            }
            QList<AlbumRef> albums;
            QSet<QString> seen;
            for (const QJsonValue &sv : result.toArray()) {
                const QJsonObject shelf = sv.toObject();
                for (const QJsonValue &iv : shelf.value(QStringLiteral("contents")).toArray()) {
                    if (albums.size() >= 6)
                        break;
                    const QJsonObject item = iv.toObject();
                    const QJsonObject alb = item.value(QStringLiteral("album")).toObject();
                    const QString bid = alb.value(QStringLiteral("id")).toString();
                    if (bid.isEmpty() || seen.contains(bid))
                        continue;
                    seen.insert(bid);
                    AlbumRef a;
                    a.browseId = bid;
                    a.title = alb.value(QStringLiteral("name")).toString();
                    if (a.title.isEmpty())
                        a.title = QStringLiteral("Album");
                    const QJsonArray artists = item.value(QStringLiteral("artists")).toArray();
                    if (!artists.isEmpty())
                        a.artists << artists.first()
                                         .toObject()
                                         .value(QStringLiteral("name"))
                                         .toString();
                    const QJsonArray thumbs = item.value(QStringLiteral("thumbnails")).toArray();
                    if (!thumbs.isEmpty())
                        a.thumbnail = upscaleThumb(
                            thumbs.last().toObject().value(QStringLiteral("url")).toString());
                    albums << a;
                }
                if (albums.size() >= 6)
                    break;
            }

            auto *lay2 = qobject_cast<QVBoxLayout *>(m_albumSection->layout());
            clearLayout(lay2);
            lay2->addWidget(makeSectionHeader(
                TR("Album Lagu"),
                TR("Album populer dari beranda YouTube Music")));
            if (albums.isEmpty()) {
                lay2->addWidget(makeEmptyLabel(TR("Album tidak tersedia saat ini.")));
                return;
            }
            auto *hs = new HScrollArea(shelfHeightFor(kCardWidth), m_albumSection);
            for (const AlbumRef &a : albums) {
                auto *card = new PlaylistCard(a.title, a.artistStr(), a.thumbnail, kCardWidth,
                                              hs->body());
                const QString bid = a.browseId;
                connect(card, &PlaylistCard::activated, this,
                        [this, bid]() { m_app->openAlbum(bid); });
                hs->contentLayout()->insertWidget(hs->contentLayout()->count() - 1, card);
            }
            lay2->addWidget(hs);
        }, this);
}

void HomeView::loadGenres()
{
    auto *lay = qobject_cast<QVBoxLayout *>(m_genreSection->layout());
    clearLayout(lay);
    lay->addWidget(makeSectionHeader(TR("Genre Lagu"),
                                     TR("Jelajahi mood dan genre pilihan")));
    lay->addWidget(makeLoadingLabel(TR("Memuat genre…")));

    m_app->bridge()->request(
        QStringLiteral("mood_categories"), QJsonObject(),
        [this](bool ok, const QJsonValue &result, const QString &) {
            struct Entry {
                QString title;
                QString params;
                int plays = 0;
            };
            QList<Entry> items;
            if (ok && result.isObject()) {
                const QJsonObject cats = result.toObject();
                for (const QString &key :
                     {QStringLiteral("Genres"), QStringLiteral("Moods & moments")}) {
                    for (const QJsonValue &ev : cats.value(key).toArray()) {
                        const QJsonObject e = ev.toObject();
                        const QString title = e.value(QStringLiteral("title")).toString();
                        if (title.isEmpty() || items.size() >= 60)
                            continue;
                        items.append(Entry{title, e.value(QStringLiteral("params")).toString(), 0});
                    }
                }
            }
            if (items.isEmpty()) {
                for (const QString &n : theme::genreNames())
                    items.append(Entry{n, QString(), 0});
            }

            // Genre yang paling sering kamu putar tampil paling depan.
            bool anyPlayed = false;
            for (Entry &e : items) {
                e.plays = m_app->storage()->genrePlays(e.title);
                anyPlayed = anyPlayed || e.plays > 0;
            }
            std::stable_sort(items.begin(), items.end(),
                             [](const Entry &a, const Entry &b) { return a.plays > b.plays; });
            if (items.size() > 8)
                items = items.mid(0, 8);

            auto *lay2 = qobject_cast<QVBoxLayout *>(m_genreSection->layout());
            clearLayout(lay2);
            lay2->addWidget(makeSectionHeader(
                TR("Genre Lagu"),
                anyPlayed ? TR("Diurutkan dari genre yang paling sering kamu putar")
                          : TR("Jelajahi mood dan genre pilihan")));
            auto *grid = new QWidget;
            auto *gl = new QGridLayout(grid);
            gl->setContentsMargins(24, 0, 24, 8);
            gl->setSpacing(10);
            for (int c = 0; c < kGridCols; ++c)
                gl->setColumnStretch(c, 1);
            int i = 0;
            for (const Entry &item : std::as_const(items)) {
                const QString color = theme::genreColors().at(i % theme::genreColors().size());
                const QString emoji = theme::genreEmoji().at(i % theme::genreEmoji().size());
                auto *tile = new GenreTile(item.title, color, emoji, grid);
                const QString title = item.title;
                const QString param = item.params;
                connect(tile, &GenreTile::activated, this,
                        [this, title, param]() { m_app->openGenre(title, param); });
                gl->addWidget(tile, i / kGridCols, i % kGridCols);
                ++i;
            }
            lay2->addWidget(grid);
        }, this);
}

void HomeView::loadHistory()
{
    const QList<Track> tracks = m_app->storage()->history(8);
    auto *lay = qobject_cast<QVBoxLayout *>(m_historySection->layout());
    clearLayout(lay);
    const QList<Track> queue = tracks;
    lay->addWidget(makeSectionHeader(
        TR("Riwayat Musik"), TR("Lagu yang baru kamu putar"),
        tracks.isEmpty() ? QString() : TR("Lihat semua"),
        [this]() { m_app->showHistory(); }));
    if (tracks.isEmpty()) {
        lay->addWidget(makeEmptyLabel(
            TR("Belum ada riwayat. Lagu yang kamu putar akan muncul di sini.")));
        return;
    }
    for (int i = 0; i < tracks.size(); ++i) {
        auto *row = new SongRow(tracks.at(i), i + 1, true, true, m_historySection);
        row->setLiked(m_app->storage()->isLiked(tracks.at(i).videoId));
        connect(row, &SongRow::playRequested, this,
                [this, queue](const Track &, int index) { m_app->playContext(queue, index); });
        connect(row, &SongRow::menuRequested, this,
                [this](const Track &tr, const QPoint &pos) { m_app->showTrackMenu(tr, pos); });
        connect(row, &SongRow::likeToggled, this,
                [this](const Track &tr) { m_app->toggleLike(tr); });
        connect(row, &SongRow::artistClicked, this,
                [this](const QString &id) { m_app->openArtist(id); });
        lay->addWidget(row);
    }
}

} // namespace bc
