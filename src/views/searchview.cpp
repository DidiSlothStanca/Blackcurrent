#include "i18n.h"
#include "views/searchview.h"

#include "bridge.h"
#include "mainwindow.h"
#include "storage.h"
#include "theme.h"
#include "views/viewutil.h"
#include "widgets.h"

#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace bc {

namespace {
constexpr int kSuggestDelayMs = 250;
constexpr int kSuggestLimit = 8;
}

SearchView::SearchView(MainWindow *app, const QString &initialQuery, QWidget *parent)
    : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_search = new SearchBar(TR("Cari lagu atau album…"), this);
    root->addWidget(m_search);

    m_scroll = new ScrollArea(this);
    root->addWidget(m_scroll, 1);

    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(250);
    connect(m_suggestTimer, &QTimer::timeout, this,
            [this]() { fetchSuggestions(m_search->text().trimmed()); });

    connect(m_search, &SearchBar::textEdited, this, &SearchView::onTextEdited);
    connect(m_search, &SearchBar::submitted, this, [this](const QString &q) {
        m_search->hideSuggestions();
        runSearch(q);
    });
    connect(m_search, &SearchBar::suggestionActivated, this, [this](const QString &q) {
        m_search->hideSuggestions();
        m_search->setText(q);
        runSearch(q);
    });

    if (initialQuery.trimmed().isEmpty()) {
        m_search->focusEdit();
        showHistory();
    } else {
        m_search->setText(initialQuery);
        runSearch(initialQuery);
    }
}

void SearchView::onTextEdited(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_search->hideSuggestions();
        showHistory();
        return;
    }
    m_suggestTimer->start();
}

void SearchView::fetchSuggestions(const QString &query)
{
    if (query.isEmpty())
        return;
    const int seq = ++m_suggestSeq;
    QJsonObject params;
    params[QStringLiteral("query")] = query;
    params[QStringLiteral("limit")] = 8;
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

void SearchView::showHistory()
{
    m_scroll->clear();
    const QStringList history = m_app->storage()->searchHistory();
    if (history.isEmpty()) {
        m_scroll->addWidget(makeSectionHeader(TR("Cari musik"),
                                              TR("Ketik judul lagu, artis, atau album")));
        m_scroll->addWidget(makeEmptyLabel(
            TR("Mulai ketik untuk mencari. Riwayat pencarianmu akan muncul di sini.")));
        return;
    }
    auto *header = makeSectionHeader(TR("Riwayat Pencarian"),
                                     TR("Pencarian terakhirmu"));
    m_scroll->addWidget(header);
    for (const QString &q : history) {
        auto *btn = makeButton(icons::Kind::History, QStringLiteral("  ") + q,
                               QStringLiteral("sideBtn"), icons::Role::Sub);
        btn->setStyleSheet(QStringLiteral("text-align:left; padding:10px 28px;"));
        connect(btn, &QPushButton::clicked, this, [this, q]() {
            m_search->setText(q);
            runSearch(q);
        });
        m_scroll->addWidget(btn);
    }
    auto *clear = makeButton(TR("Bersihkan riwayat"), QStringLiteral("ghostBtn"));
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_app->storage()->clearSearchHistory();
        showHistory();
    });
    auto *wrap = new QWidget;
    auto *wl = new QHBoxLayout(wrap);
    wl->setContentsMargins(28, 10, 28, 10);
    wl->addWidget(clear);
    wl->addStretch(1);
    m_scroll->addWidget(wrap);
}

void SearchView::runSearch(const QString &query)
{
    const QString q = query.trimmed();
    if (q.isEmpty()) {
        showHistory();
        return;
    }
    m_search->hideSuggestions();
    m_app->storage()->addSearchHistory(q);

    m_scroll->clear();
    m_scroll->addWidget(makeSectionHeader(TR("Hasil untuk \"%1\"").arg(q),
                                          TR("Memuat hasil…")));
    m_scroll->addWidget(makeLoadingLabel(TR("Mencari di YouTube Music…")));

    const int seq = ++m_searchSeq;
    QJsonObject params;
    params[QStringLiteral("query")] = q;
    m_app->bridge()->request(
        QStringLiteral("search_all"), params,
        [this, seq, q](bool ok, const QJsonValue &result, const QString &error) {
            if (seq != m_searchSeq)
                return;
            m_scroll->clear();
            if (!ok) {
                m_scroll->addWidget(makeSectionHeader(TR("Hasil untuk \"%1\"").arg(q),
                                                      TR("Gagal memuat")));
                m_scroll->addWidget(makeEmptyLabel(error));
                return;
            }
            const QJsonObject obj = result.toObject();
            const QList<Track> songs = Track::listFromJson(
                obj.value(QStringLiteral("songs")).toArray());
            const QJsonArray albums = obj.value(QStringLiteral("albums")).toArray();

            m_scroll->addWidget(makeSectionHeader(
                TR("Lagu"), TR("%1 hasil ditemukan").arg(songs.size())));
            if (songs.isEmpty()) {
                m_scroll->addWidget(makeEmptyLabel(TR("Tidak ada lagu ditemukan.")));
            } else {
                const QList<SongRow *> rows = makeTrackRows(m_app, songs, true, true, m_scroll->body());
                for (SongRow *r : rows)
                    m_scroll->addWidget(r);
            }

            if (!albums.isEmpty()) {
                m_scroll->addWidget(makeSectionHeader(QStringLiteral("Album"),
                                                      TR("Album terkait")));
                auto *hs = new HScrollArea(shelfHeightFor(kCardWidth), m_scroll->body());
                for (const QJsonValue &av : albums) {
                    const QJsonObject a = av.toObject();
                    const QString bid = a.value(QStringLiteral("browse_id")).toString();
                    QStringList artists;
                    for (const QJsonValue &an : a.value(QStringLiteral("artists")).toArray())
                        if (an.isString())
                            artists << an.toString();
                    const QString title = a.value(QStringLiteral("title")).toString();
                    const QString thumb = a.value(QStringLiteral("thumbnail")).toString();
                    auto *card = new PlaylistCard(
                        title, artists.isEmpty() ? QStringLiteral("Album") : artists.join(", "),
                        thumb, kCardWidth, hs->body());
                    connect(card, &PlaylistCard::activated, this,
                            [this, bid]() { m_app->openAlbum(bid); });
                    hs->contentLayout()->insertWidget(hs->contentLayout()->count() - 1, card);
                }
                m_scroll->addWidget(hs);
            }
        }, this);
}

} // namespace bc
