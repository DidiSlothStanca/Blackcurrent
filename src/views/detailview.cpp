#include "i18n.h"
#include "views/detailview.h"

#include "mainwindow.h"
#include "storage.h"
#include "theme.h"
#include "views/viewutil.h"
#include "widgets.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace bc {

DetailView::DetailView(MainWindow *app, const QString &title, const QString &subtitle,
                       const QString &cover, const QList<Track> &tracks, Kind kind,
                       const QString &refId, QWidget *parent)
    : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ScrollArea(this);
    root->addWidget(scroll);

    const QList<Track> queue = tracks;
    QList<QWidget *> buttons;

    auto *playBtn = makeButton(icons::Kind::Play, TR("Putar"), QStringLiteral("primaryBtn"), icons::Role::OnAccent);
    connect(playBtn, &QPushButton::clicked, this, [this, queue]() {
        if (!queue.isEmpty())
            m_app->playContext(queue, 0);
    });
    buttons << playBtn;

    if (kind == Kind::Album && !refId.isEmpty()) {
        const bool likedNow = m_app->storage()->isAlbumLiked(refId);
        auto *likeBtn = makeButton(likedNow ? icons::Kind::HeartFilled : icons::Kind::Heart,
                                   likedNow ? TR("Disukai") : TR("Suka album"),
                                   QStringLiteral("ghostBtn"),
                                   likedNow ? icons::Role::Accent : icons::Role::Text);
        connect(likeBtn, &QPushButton::clicked, this, [this, likeBtn, refId, title, subtitle]() {
            AlbumRef a;
            a.browseId = refId;
            a.title = title;
            a.artists << subtitle;
            const bool liked = m_app->storage()->toggleLikeAlbum(a);
            likeBtn->setText(liked ? TR("Disukai") : TR("Suka album"));
            likeBtn->setIcon(icons::icon(liked ? icons::Kind::HeartFilled : icons::Kind::Heart,
                                         liked ? icons::Role::Accent : icons::Role::Text));
        });
        buttons << likeBtn;
    }

    scroll->addWidget(makeDetailHeader(title, subtitle, cover, buttons));

    if (tracks.isEmpty()) {
        scroll->addWidget(makeEmptyLabel(TR("Tidak ada lagu di sini.")));
        return;
    }

    const QString removeId = (kind == Kind::LocalPlaylist) ? refId : QString();
    const QList<SongRow *> rows =
        makeTrackRows(m_app, queue, kind != Kind::Album ? true : false, true, scroll->body(),
                      removeId);
    for (SongRow *r : rows)
        scroll->addWidget(r);
}

} // namespace bc
