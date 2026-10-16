#include "i18n.h"
#include "views/libraryview.h"

#include "mainwindow.h"
#include "storage.h"
#include "theme.h"
#include "views/viewutil.h"
#include "widgets.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bc {

// ------------------------------------------------------------- LibraryView

LibraryView::LibraryView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_scroll = new ScrollArea(this);
    root->addWidget(m_scroll);
    refresh();
}

void LibraryView::refresh()
{
    m_scroll->clear();

    auto *header = new QWidget;
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(28, 22, 28, 8);
    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 0, 0, 0);
    col->addWidget(makeLabel(TR("Koleksi"), QString(), 22, true));
    col->addWidget(makeLabel(TR("Playlist yang kamu buat"), QStringLiteral("sub"), 10));
    hl->addLayout(col);
    hl->addStretch();
    auto *newBtn = makeButton(TR("+  Playlist baru"), QStringLiteral("primaryBtn"));
    connect(newBtn, &QPushButton::clicked, m_app, [this]() { m_app->createPlaylistDialog(); });
    hl->addWidget(newBtn, 0, Qt::AlignTop);
    m_scroll->addWidget(header);

    const QList<Playlist> lists = m_app->storage()->playlists();
    if (lists.isEmpty()) {
        m_scroll->addWidget(makeEmptyLabel(
            TR("Belum ada playlist. Buat satu untuk mulai mengoleksi lagu.")));
        return;
    }

    auto *grid = new QWidget;
    auto *gl = new QGridLayout(grid);
    gl->setContentsMargins(24, 0, 24, 10);
    gl->setSpacing(12);
    int i = 0;
    for (const Playlist &p : lists) {
        const int count = p.tracks.size();
        auto *card = new PlaylistCard(
            p.name,
            TR("%1 lagu").arg(count),
            count > 0 ? p.tracks.first().thumbnail : QString(), 160, grid);
        const QString id = p.id;
        connect(card, &PlaylistCard::activated, this,
                [this, id]() { m_app->openLocalPlaylist(id); });
        connect(card, &PlaylistCard::menuRequested, this, [this, id, card](const QPoint &pos) {
            QMenu menu;
            QAction *openAct = menu.addAction(TR("Buka"));
            QAction *renameAct = menu.addAction(TR("Ganti nama"));
            QAction *deleteAct = menu.addAction(TR("Hapus"));
            QAction *chosen = menu.exec(pos);
            if (chosen == openAct) {
                m_app->openLocalPlaylist(id);
            } else if (chosen == renameAct) {
                Playlist pl;
                if (m_app->storage()->getPlaylist(id, &pl)) {
                    bool ok = false;
                    const QString name = QInputDialog::getText(this, TR("Ganti nama"),
                                                               TR("Nama playlist:"),
                                                               QLineEdit::Normal, pl.name, &ok);
                    if (ok && !name.trimmed().isEmpty()) {
                        m_app->storage()->renamePlaylist(id, name, pl.description);
                        m_app->refreshSidebarPlaylists();
                        refresh();
                    }
                }
            } else if (chosen == deleteAct) {
                if (QMessageBox::question(this, TR("Hapus playlist"),
                                          TR("Hapus playlist ini?"))
                    == QMessageBox::Yes) {
                    m_app->storage()->deletePlaylist(id);
                    m_app->refreshSidebarPlaylists();
                    refresh();
                }
            }
        });
        gl->addWidget(card, i / 4, i % 4, Qt::AlignLeft | Qt::AlignTop);
        ++i;
    }
    m_scroll->addWidget(grid);
}

// --------------------------------------------------------------- LikedView

LikedView::LikedView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ScrollArea(this);
    root->addWidget(scroll);

    const QList<Track> tracks = m_app->storage()->likedTracks();
    auto *playBtn = makeButton(icons::Kind::Play, TR("Putar semua"), QStringLiteral("primaryBtn"), icons::Role::OnAccent);
    const QList<Track> queue = tracks;
    connect(playBtn, &QPushButton::clicked, this, [this, queue]() {
        if (!queue.isEmpty())
            m_app->playContext(queue, 0);
    });
    scroll->addWidget(makeDetailHeader(
        TR("Lagu Disukai"),
        TR("%1 lagu").arg(tracks.size()),
        tracks.isEmpty() ? QString() : tracks.first().bigThumbnail(), {playBtn}));

    if (tracks.isEmpty()) {
        scroll->addWidget(makeEmptyLabel(
            TR("Belum ada lagu disukai. Tekan ikon hati pada lagu untuk menyimpannya.")));
        return;
    }
    const QList<SongRow *> rows = makeTrackRows(m_app, tracks, true, true, scroll->body());
    for (SongRow *r : rows)
        scroll->addWidget(r);
}

// --------------------------------------------------------- LikedAlbumsView

LikedAlbumsView::LikedAlbumsView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ScrollArea(this);
    root->addWidget(scroll);

    scroll->addWidget(makeSectionHeader(TR("Album Disukai"),
                                        TR("Album yang kamu simpan")));
    const QList<AlbumRef> albums = m_app->storage()->likedAlbums();
    if (albums.isEmpty()) {
        scroll->addWidget(makeEmptyLabel(
            TR("Belum ada album disukai. Buka album lalu tekan Suka.")));
        return;
    }
    auto *grid = new QWidget;
    auto *gl = new QGridLayout(grid);
    gl->setContentsMargins(24, 0, 24, 10);
    gl->setSpacing(12);
    int i = 0;
    for (const AlbumRef &a : albums) {
        auto *card = new PlaylistCard(a.title, a.artistStr(), a.thumbnail, 160, grid);
        const QString bid = a.browseId;
        connect(card, &PlaylistCard::activated, this, [this, bid]() { m_app->openAlbum(bid); });
        gl->addWidget(card, i / 4, i % 4, Qt::AlignLeft | Qt::AlignTop);
        ++i;
    }
    scroll->addWidget(grid);
}

// ------------------------------------------------------------- HistoryView

HistoryView::HistoryView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ScrollArea(this);
    root->addWidget(scroll);

    const QList<Track> tracks = m_app->storage()->history();
    auto *row = new QWidget;
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(28, 22, 28, 8);
    auto *col = new QVBoxLayout;
    col->addWidget(makeLabel(TR("Riwayat Musik"), QString(), 22, true));
    col->addWidget(makeLabel(TR("Lagu yang baru kamu putar"), QStringLiteral("sub"), 10));
    hl->addLayout(col);
    hl->addStretch();
    if (!tracks.isEmpty()) {
        auto *playAll = makeButton(icons::Kind::Play, TR("Putar semua"), QStringLiteral("ghostBtn"), icons::Role::Text);
        const QList<Track> queue = tracks;
        connect(playAll, &QPushButton::clicked, this,
                [this, queue]() { m_app->playContext(queue, 0); });
        hl->addWidget(playAll, 0, Qt::AlignTop);
    }
    auto *clear = makeButton(TR("Bersihkan"), QStringLiteral("ghostBtn"));
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_app->storage()->clearHistory();
        m_app->navigate(QStringLiteral("history"), TR("Riwayat Musik"));
    });
    hl->addWidget(clear, 0, Qt::AlignTop);
    scroll->addWidget(row);

    if (tracks.isEmpty()) {
        scroll->addWidget(makeEmptyLabel(
            TR("Belum ada riwayat. Lagu yang kamu putar akan muncul di sini.")));
        return;
    }
    const QList<SongRow *> rows = makeTrackRows(m_app, tracks, true, true, scroll->body());
    for (SongRow *r : rows)
        scroll->addWidget(r);
}

} // namespace bc
