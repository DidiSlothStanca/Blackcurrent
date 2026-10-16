#pragma once

#include "mainwindow.h"
#include "theme.h"
#include "widgets.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace bc {

// Buat baris lagu lengkap dengan sambungan ke aksi aplikasi.
inline QList<SongRow *> makeTrackRows(MainWindow *app, const QList<Track> &queue, bool showAlbum,
                                      bool showCover, QWidget *parent,
                                      const QString &removePlaylistId = QString())
{
    QList<SongRow *> rows;
    for (int i = 0; i < queue.size(); ++i) {
        const Track &t = queue.at(i);
        auto *row = new SongRow(t, i + 1, showAlbum, showCover, parent);
        row->setLiked(app->storage()->isLiked(t.videoId));
        QObject::connect(row, &SongRow::playRequested, app,
                         [app, queue](const Track &, int idx) { app->playContext(queue, idx); });
        QObject::connect(row, &SongRow::menuRequested, app,
                         [app, removePlaylistId](const Track &tr, const QPoint &pos) {
                             app->showTrackMenu(tr, pos, removePlaylistId);
                         });
        QObject::connect(row, &SongRow::likeToggled, app,
                         [app](const Track &tr) { app->toggleLike(tr); });
        QObject::connect(row, &SongRow::artistClicked, app,
                         [app](const QString &id) { app->openArtist(id); });
        rows << row;
    }
    return rows;
}

// Header detail (cover + judul + subjudul + tombol aksi).
inline QWidget *makeDetailHeader(const QString &title, const QString &subtitle,
                                 const QString &coverUrl, const QList<QWidget *> &buttons)
{
    auto *wrap = new QWidget;
    auto *hl = new QHBoxLayout(wrap);
    hl->setContentsMargins(28, 18, 28, 14);
    hl->setSpacing(20);

    auto *thumb = new ThumbLabel(180, wrap);
    if (!coverUrl.isEmpty())
        thumb->setUrl(coverUrl, true);
    hl->addWidget(thumb, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 12, 0, 0);
    col->setSpacing(8);
    col->addWidget(makeLabel(subtitle.toUpper(), QStringLiteral("sub"), 9, true));
    auto *t = makeLabel(title, QString(), 24, true);
    t->setWordWrap(true);
    col->addWidget(t);

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    for (QWidget *b : buttons)
        row->addWidget(b, 0, Qt::AlignLeft);
    row->addStretch(1);
    col->addLayout(row);
    col->addStretch(1);
    hl->addLayout(col, 1);
    return wrap;
}

} // namespace bc
