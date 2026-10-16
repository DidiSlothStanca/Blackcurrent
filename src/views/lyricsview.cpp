#include "i18n.h"
#include "views/lyricsview.h"

#include "bridge.h"
#include "mainwindow.h"
#include "theme.h"
#include "widgets.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace bc {

LyricsView::LyricsView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    setObjectName(QStringLiteral("lyricsPanel"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    titleRow->addWidget(new icons::IconWidget(icons::Kind::Mic, icons::Role::Accent, 22));
    titleRow->addWidget(makeLabel(TR("Lirik"), QString(), 14, true));
    titleRow->addStretch();
    root->addLayout(titleRow);

    m_headerTitle = makeLabel(QString(), QStringLiteral("sub"), 10);
    m_headerTitle->setWordWrap(true);
    root->addWidget(m_headerTitle);

    m_scroll = new ScrollArea(this);
    root->addWidget(m_scroll, 1);

    m_body = makeLabel(TR("Pilih lagu untuk melihat liriknya."), QStringLiteral("sub"),
                       10);
    m_body->setWordWrap(true);
    m_body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_body->setContentsMargins(0, 6, 0, 6);
    m_scroll->addWidget(m_body);
}

void LyricsView::setMessage(const QString &text)
{
    if (m_body)
        m_body->setText(text);
}

void LyricsView::clear()
{
    m_track = Track();
    if (m_headerTitle)
        m_headerTitle->setText(QString());
    setMessage(TR("Pilih lagu untuk melihat liriknya."));
}

void LyricsView::showFor(const Track &track)
{
    m_track = track;
    if (m_headerTitle)
        m_headerTitle->setText(QStringLiteral("%1 — %2").arg(track.title, track.artistStr()));
    applyContent();
}

void LyricsView::reload()
{
    if (!m_track.videoId.isEmpty())
        applyContent();
}

void LyricsView::applyContent()
{
    if (m_track.videoId.isEmpty()) {
        setMessage(TR("Pilih lagu untuk melihat liriknya."));
        return;
    }
    setMessage(TR("Memuat lirik…"));
    m_scroll->resetTop();

    const int seq = ++m_seq;
    QJsonObject params;
    params[QStringLiteral("video_id")] = m_track.videoId;
    m_app->bridge()->request(
        QStringLiteral("lyrics"), params,
        [this, seq](bool ok, const QJsonValue &result, const QString &) {
            if (seq != m_seq)
                return;
            if (!ok || !result.isObject()) {
                setMessage(
                    TR("Lirik tidak tersedia untuk lagu ini.\n\nCoba lagu lain, ya."));
                return;
            }
            const QJsonObject o = result.toObject();
            const QString lyrics = o.value(QStringLiteral("lyrics")).toString();
            const QString source = o.value(QStringLiteral("source")).toString();
            if (lyrics.trimmed().isEmpty()) {
                setMessage(TR("Lirik tidak ditemukan untuk lagu ini."));
                return;
            }
            QString text = lyrics;
            if (!source.isEmpty())
                text += QStringLiteral("\n\n— %1").arg(source);
            setMessage(text);
            m_scroll->resetTop();
        }, this);
}

} // namespace bc
