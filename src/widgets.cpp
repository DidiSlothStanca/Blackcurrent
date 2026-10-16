#include "i18n.h"
#include "widgets.h"

#include "theme.h"
#include "thumbnails.h"

#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPointer>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <QVBoxLayout>

namespace bc {

namespace {

void setRole(QWidget *w, const QString &role)
{
    if (role.isEmpty())
        return;
    w->setProperty("role", role);
}

QString placeholderColor(const QString &url)
{
    if (url.isEmpty())
        return QStringLiteral("#282828");
    const uint h = qHash(url);
    const QColor c = QColor::fromHsv(static_cast<int>(h % 360), 60, 70);
    return c.name();
}

} // namespace

void repolish(QWidget *w)
{
    if (!w)
        return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

void clearLayout(QLayout *layout)
{
    if (!layout)
        return;
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            // Jangan setParent(nullptr): di Linux (X11/Wayland) widget yang dilepas dari
            // induknya sempat menjadi jendela top-level. Cukup sembunyikan lalu hancurkan
            // dengan aman di putaran event berikutnya (tombol yang sedang diklik pun aman).
            w->hide();
            w->deleteLater();
        } else if (QLayout *child = item->layout()) {
            clearLayout(child); // sub-layout: hancurkan juga widget di dalamnya
        }
        delete item;
    }
}

QString elideLines(const QString &text, const QFontMetrics &fm, int width, int maxLines)
{
    if (maxLines <= 1)
        return fm.elidedText(text, Qt::ElideRight, width);
    const QRect limit(0, 0, qMax(10, width), fm.lineSpacing() * maxLines);
    auto fits = [&](const QString &t) {
        return fm.boundingRect(limit, Qt::TextWordWrap, t).height() <= limit.height();
    };
    if (fits(text))
        return text;
    int lo = 0, hi = text.size();
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (fits(text.left(mid).trimmed() + QChar(0x2026)))
            lo = mid;
        else
            hi = mid - 1;
    }
    return text.left(lo).trimmed() + QChar(0x2026);
}

QLabel *makeLabel(const QString &text, const QString &role, int pointSize, bool bold)
{
    auto *l = new QLabel(text);
    setRole(l, role);
    l->setFont(theme::font(pointSize > 0 ? pointSize : 11, bold));
    return l;
}

QPushButton *makeButton(const QString &text, const QString &objectName)
{
    auto *b = new QPushButton(text);
    b->setObjectName(objectName);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QPushButton *makeIconButton(const QString &text, const QString &tooltip)
{
    auto *b = new QPushButton(text);
    b->setObjectName(QStringLiteral("iconBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFlat(true);
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    return b;
}

QPushButton *makeIconButton(icons::Kind kind, const QString &tooltip, icons::Role role)
{
    auto *b = new QPushButton;
    b->setObjectName(QStringLiteral("iconBtn"));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFlat(true);
    b->setIcon(icons::icon(kind, role));
    b->setIconSize(QSize(20, 20));
    if (!tooltip.isEmpty())
        b->setToolTip(tooltip);
    return b;
}

QPushButton *makeButton(icons::Kind kind, const QString &text, const QString &objectName,
                        icons::Role role)
{
    QPushButton *b = makeButton(text, objectName);
    b->setIcon(icons::icon(kind, role));
    b->setIconSize(QSize(16, 16));
    return b;
}

void setLikeIcon(QPushButton *button, bool liked)
{
    if (!button)
        return;
    button->setIcon(liked ? icons::icon(icons::Kind::HeartFilled, icons::Role::Accent)
                          : icons::icon(icons::Kind::Heart, icons::Role::Sub));
}

void setActiveIcon(QPushButton *button, icons::Kind kind, bool active)
{
    if (!button)
        return;
    button->setIcon(icons::icon(kind, active ? icons::Role::Accent : icons::Role::Sub));
}

QWidget *makeSectionHeader(const QString &title, const QString &subtitle,
                           const QString &actionText, const std::function<void()> &action)
{
    auto *w = new QWidget;
    auto *row = new QHBoxLayout(w);
    row->setContentsMargins(28, 22, 28, 8);
    row->setSpacing(10);

    auto *left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(2);
    auto *t = makeLabel(title, QStringLiteral("title"), 14, true);
    left->addWidget(t);
    if (!subtitle.isEmpty())
        left->addWidget(makeLabel(subtitle, QStringLiteral("sub"), 10));
    row->addLayout(left);
    row->addStretch();

    if (!actionText.isEmpty() && action) {
        auto *btn = makeButton(actionText, QStringLiteral("linkBtn"));
        QObject::connect(btn, &QPushButton::clicked, w, [action]() { action(); });
        row->addWidget(btn);
    }
    return w;
}

QLabel *makeLoadingLabel(const QString &text)
{
    auto *l = makeLabel(text, QStringLiteral("sub"), 11);
    l->setContentsMargins(32, 4, 32, 12);
    return l;
}

QLabel *makeEmptyLabel(const QString &text)
{
    auto *l = makeLabel(text, QStringLiteral("sub"), 11);
    l->setWordWrap(true);
    l->setContentsMargins(32, 4, 32, 12);
    return l;
}

// ------------------------------------------------------------ ClickableLabel

ClickableLabel::ClickableLabel(const QString &text, QWidget *parent) : QLabel(text, parent)
{
    setCursor(Qt::PointingHandCursor);
}

void ClickableLabel::setRole(const QString &role) { setProperty("role", role); }

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QLabel::mouseReleaseEvent(event);
}

// --------------------------------------------------------------- ThumbLabel

ThumbLabel::ThumbLabel(int size, QWidget *parent) : QLabel(parent), m_size(size)
{
    setFixedSize(size, size);
    setScaledContents(false);
    setAlignment(Qt::AlignCenter);
}

void ThumbLabel::setUrl(const QString &url, bool rounded)
{
    m_url = url;
    m_rounded = rounded;
    refresh();
}

void ThumbLabel::setSize(int size)
{
    if (size == m_size)
        return;
    m_size = size;
    setFixedSize(size, size);
    refresh();
}

void ThumbLabel::refresh()
{
    m_hasPixmap = false;
    if (m_url.isEmpty()) {
        setPixmap(QPixmap());
        update();
        return;
    }
    const QPixmap cachedPm = ThumbnailLoader::instance().cached(m_url, m_size, m_rounded);
    if (!cachedPm.isNull()) {
        m_hasPixmap = true;
        setPixmap(cachedPm);
        return;
    }
    update(); // tampilkan placeholder selagi gambar diunduh
    QPointer<ThumbLabel> self(this);
    const QString url = m_url;
    ThumbnailLoader::instance().load(m_url, m_size, m_rounded, [self, url](const QPixmap &pm) {
        // Abaikan hasil basi: label bisa saja sudah diberi URL lain / ukuran lain.
        if (!self || pm.isNull() || self->m_url != url || pm.width() != self->m_size)
            return;
        self->m_hasPixmap = true;
        self->setPixmap(pm);
    });
}

void ThumbLabel::paintEvent(QPaintEvent *event)
{
    if (m_hasPixmap) {
        QLabel::paintEvent(event);
        return;
    }
    // Belum ada gambar (sedang diunduh / gagal): gambar kotak membulat + ikon nada
    // supaya terlihat disengaja, bukan "bug" thumbnail hilang.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const theme::Palette &pal = theme::palette();
    p.setPen(Qt::NoPen);
    p.setBrush(pal.cardHover);
    p.drawRoundedRect(QRectF(rect()), 10, 10);
    const qreal s = qMin(width(), height()) * 0.42;
    icons::paint(&p, QRectF((width() - s) / 2.0, (height() - s) / 2.0, s, s),
                 icons::Kind::Note, pal.subtextDim);
}

// ---------------------------------------------------------------- ClickSlider

ClickSlider::ClickSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
    setMinimumHeight(22); // area klik yang nyaman, bukan hanya 5px alur
}

void ClickSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && orientation() == Qt::Horizontal) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                     QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                     QStyle::SC_SliderHandle, this);
        // Klik tepat di bulatan -> perilaku bawaan (langsung mulai diseret).
        if (!handle.contains(event->position().toPoint())) {
            const int span = qMax(1, groove.width() - handle.width());
            const int pos = qBound(0, int(event->position().x()) - groove.x() - handle.width() / 2,
                                   span);
            const int value = QStyle::sliderValueFromPosition(minimum(), maximum(), pos, span,
                                                              opt.upsideDown);
            setValue(value);
            // Bulatan kini berada tepat di bawah kursor, jadi QSlider::mousePressEvent
            // di bawah memperlakukannya sebagai awal seretan: sliderPressed() terpancar,
            // lalu sliderReleased() saat tombol dilepas (di situlah lagu di-seek).
        }
    }
    QSlider::mousePressEvent(event);
}

// ---------------------------------------------------------------- ScrollArea

ScrollArea::ScrollArea(QWidget *parent) : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAlignment(Qt::AlignTop);
    setObjectName(QStringLiteral("scrollArea"));

    m_body = new QWidget;
    m_body->setObjectName(QStringLiteral("scrollBody"));
    m_layout = new QVBoxLayout(m_body);
    m_layout->setContentsMargins(0, 0, 0, 28);
    m_layout->setSpacing(2);
    setWidget(m_body);
}

void ScrollArea::addWidget(QWidget *widget)
{
    if (m_layout->count() > 0) {
        QLayoutItem *last = m_layout->itemAt(m_layout->count() - 1);
        if (last && last->spacerItem())
            m_layout->removeItem(last);
    }
    m_layout->addWidget(widget);
    m_layout->addStretch(1);
}

void ScrollArea::addLayout(QLayout *layout)
{
    if (m_layout->count() > 0) {
        QLayoutItem *last = m_layout->itemAt(m_layout->count() - 1);
        if (last && last->spacerItem())
            m_layout->removeItem(last);
    }
    m_layout->addLayout(layout);
    m_layout->addStretch(1);
}

void ScrollArea::clear()
{
    clearLayout(m_layout);
    m_layout->addStretch(1);
}

void ScrollArea::resetTop() { verticalScrollBar()->setValue(0); }

// --------------------------------------------------------------- HScrollArea

HScrollArea::HScrollArea(int height, QWidget *parent) : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFixedHeight(height);
    setObjectName(QStringLiteral("hscrollArea"));

    m_body = new QWidget;
    m_body->setObjectName(QStringLiteral("hbody"));
    m_layout = new QHBoxLayout(m_body);
    m_layout->setContentsMargins(12, 8, 12, 8);
    m_layout->setSpacing(12);
    m_layout->addStretch(1);
    setWidget(m_body);
}

void HScrollArea::clear()
{
    clearLayout(m_layout);
    m_layout->addStretch(1);
}

namespace {
// Roda mouse vertikal tetap menggulir halaman; Shift+roda / touchpad menggulir ke samping.
void scrollHorizontally(QScrollArea *area, QWheelEvent *event)
{
    QScrollBar *bar = area->horizontalScrollBar();
    int delta = event->angleDelta().x();
    if (delta == 0 && (event->modifiers() & Qt::ShiftModifier))
        delta = event->angleDelta().y();
    if (!bar || bar->maximum() == 0 || delta == 0) {
        event->ignore(); // teruskan ke area gulir halaman
        return;
    }
    bar->setValue(bar->value() - delta);
    event->accept();
}
} // namespace

void HScrollArea::wheelEvent(QWheelEvent *event) { scrollHorizontally(this, event); }

// ---------------------------------------------------------------- PlayButton

PlayButton::PlayButton(int diameter, QWidget *parent) : QPushButton(parent)
{
    setObjectName(QStringLiteral("playBtn"));
    setFixedSize(diameter, diameter);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
}

void PlayButton::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    update();
}

void PlayButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const theme::Palette &pal = theme::palette();

    QColor bg = underMouse() ? pal.accentHover : pal.accent;
    if (isDown())
        bg = bg.darker(115);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(r);

    p.setBrush(pal.accentText);
    const QPointF c = r.center();
    const qreal s = r.width();
    if (m_playing) {
        const qreal bw = s * 0.12;
        const qreal bh = s * 0.36;
        const qreal gap = s * 0.10;
        p.drawRoundedRect(QRectF(c.x() - gap / 2 - bw, c.y() - bh / 2, bw, bh), bw / 3, bw / 3);
        p.drawRoundedRect(QRectF(c.x() + gap / 2, c.y() - bh / 2, bw, bh), bw / 3, bw / 3);
    } else {
        // Titik berat segitiga diletakkan tepat di pusat lingkaran -> tampak di tengah.
        const qreal tw = s * 0.32;
        const qreal th = s * 0.38;
        QPolygonF tri;
        tri << QPointF(c.x() - tw / 3.0, c.y() - th / 2.0)
            << QPointF(c.x() - tw / 3.0, c.y() + th / 2.0)
            << QPointF(c.x() + 2.0 * tw / 3.0, c.y());
        p.drawPolygon(tri);
    }
}

// ----------------------------------------------------------------- SongCard

SongCard::SongCard(const Track &track, int width, QWidget *parent)
    : QFrame(parent), m_track(track), m_width(width)
{
    setObjectName(QStringLiteral("songCard"));
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(width, cardHeightFor(width));

    const int inner = width - 20;
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(6);

    m_thumb = new ThumbLabel(inner, this);
    m_thumb->setObjectName(QStringLiteral("cardThumb"));
    lay->addWidget(m_thumb, 0, Qt::AlignHCenter);

    m_titleFull = track.title;
    m_title = makeLabel(QString(), QStringLiteral("title"), 10, true);
    m_title->setWordWrap(true);
    m_title->setFixedSize(inner, 38); // selalu 2 baris
    m_title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_title->setText(elideLines(track.title, m_title->fontMetrics(), inner - 2, 2));
    m_title->setToolTip(track.title);
    lay->addWidget(m_title);

    auto *sub = makeLabel(QString(), QStringLiteral("sub"), 9);
    sub->setFixedWidth(inner);
    sub->setText(sub->fontMetrics().elidedText(
        QStringLiteral("%1 • %2").arg(track.artistStr(), track.displayDuration()),
        Qt::ElideRight, inner));
    lay->addWidget(sub);

    lay->addStretch(1);

    m_badge = new PlayButton(40, this);
    m_badge->hide();
    connect(m_badge, &QPushButton::clicked, this, [this]() { emit activated(m_track); });

    if (!track.thumbnail.isEmpty())
        m_thumb->setUrl(track.bigThumbnail(), true);
}

void SongCard::enterEvent(QEnterEvent *event)
{
    m_badge->show();
    m_badge->raise();
    QFrame::enterEvent(event);
}

void SongCard::leaveEvent(QEvent *event)
{
    m_badge->hide();
    QFrame::leaveEvent(event);
}

void SongCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_badge->geometry().contains(event->pos()))
        emit activated(m_track);
    QFrame::mouseReleaseEvent(event);
}

void SongCard::contextMenuEvent(QContextMenuEvent *event)
{
    emit menuRequested(m_track, event->globalPos());
    event->accept();
}

void SongCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (m_badge) {
        const int thumbSize = m_width - 20;
        m_badge->move(10 + thumbSize - m_badge->width() - 6, 10 + thumbSize - m_badge->height() - 6);
    }
}

// ------------------------------------------------------------- PlaylistCard

PlaylistCard::PlaylistCard(const QString &title, const QString &subtitle, const QString &thumbnail,
                           int width, QWidget *parent)
    : QFrame(parent), m_width(width)
{
    setObjectName(QStringLiteral("playlistCard"));
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(width, cardHeightFor(width));

    const int inner = width - 20;
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(6);

    m_thumb = new ThumbLabel(inner, this);
    lay->addWidget(m_thumb, 0, Qt::AlignHCenter);

    m_titleFull = title;
    m_title = makeLabel(QString(), QStringLiteral("title"), 10, true);
    m_title->setWordWrap(true);
    m_title->setFixedSize(inner, 38);
    m_title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_title->setText(elideLines(title, m_title->fontMetrics(), inner - 2, 2));
    m_title->setToolTip(title);
    lay->addWidget(m_title);

    auto *sub = makeLabel(QString(), QStringLiteral("sub"), 9);
    sub->setFixedWidth(inner);
    sub->setText(sub->fontMetrics().elidedText(subtitle, Qt::ElideRight, inner));
    lay->addWidget(sub);
    lay->addStretch(1);

    if (!thumbnail.isEmpty())
        m_thumb->setUrl(thumbnail, true);
}

void PlaylistCard::setThumbnail(const QString &url)
{
    if (m_thumb)
        m_thumb->setUrl(url, true);
}

void PlaylistCard::enterEvent(QEnterEvent *event) { QFrame::enterEvent(event); }
void PlaylistCard::leaveEvent(QEvent *event) { QFrame::leaveEvent(event); }

void PlaylistCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit activated();
    QFrame::mouseReleaseEvent(event);
}

void PlaylistCard::contextMenuEvent(QContextMenuEvent *event)
{
    emit menuRequested(event->globalPos());
    event->accept();
}

void PlaylistCard::resizeEvent(QResizeEvent *event) { QFrame::resizeEvent(event); }

// ---------------------------------------------------------------- GenreTile

GenreTile::GenreTile(const QString &title, const QString &color, const QString &emoji,
                     QWidget *parent)
    : QFrame(parent), m_color(color)
{
    setObjectName(QStringLiteral("genreTile"));
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    // Tinggi tetap + lebar mengembang rata -> semua ubin sama besar.
    setFixedHeight(kGenreTileHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(QStringLiteral("QFrame#genreTile { background: %1; border-radius: 12px; }")
                      .arg(color));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 8, 12, 8);
    lay->setSpacing(6);

    auto *text = new QLabel(this);
    text->setWordWrap(true);
    text->setFont(theme::font(11, true));
    text->setStyleSheet(QStringLiteral("color: #ffffff; background: transparent;"));
    text->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    text->setText(title);
    text->setToolTip(title);
    lay->addWidget(text, 1);

    // Parameter `emoji` sengaja diabaikan: emoji butuh font khusus dan tampil sebagai
    // kotak kosong di banyak instalasi Linux. Ikon nada digambar sendiri.
    Q_UNUSED(emoji);
    auto *icon = new icons::IconWidget(icons::Kind::Note, icons::Role::White, 28, this);
    lay->addWidget(icon);
}

void GenreTile::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit activated();
    QFrame::mouseReleaseEvent(event);
}

// ------------------------------------------------------------------ SongTile

namespace {
constexpr int kTileThumb = 48;
constexpr int kTileMarginL = 8;
constexpr int kTileMarginR = 12;
constexpr int kTileSpacing = 10;
} // namespace

SongTile::SongTile(const Track &track, QWidget *parent) : QFrame(parent), m_track(track)
{
    setObjectName(QStringLiteral("songTile"));
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(kTileHeight);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(kTileMarginL, 6, kTileMarginR, 6);
    lay->setSpacing(kTileSpacing);

    auto *thumb = new ThumbLabel(kTileThumb, this);
    if (!track.thumbnail.isEmpty())
        thumb->setUrl(track.thumbnail, true);
    lay->addWidget(thumb);

    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(2);
    m_title = makeLabel(track.title, QStringLiteral("title"), 10, true);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_artist = makeLabel(track.artistStr(), QStringLiteral("sub"), 9);
    m_artist->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    col->addStretch(1);
    col->addWidget(m_title);
    col->addWidget(m_artist);
    col->addStretch(1);
    lay->addLayout(col, 1);

    setToolTip(QStringLiteral("%1 — %2").arg(track.title, track.artistStr()));
    applyElide();
}

void SongTile::applyElide()
{
    const int avail = qMax(40, width() - kTileMarginL - kTileMarginR - kTileThumb - kTileSpacing);
    m_title->setText(m_title->fontMetrics().elidedText(m_track.title, Qt::ElideRight, avail));
    m_artist->setText(m_artist->fontMetrics().elidedText(m_track.artistStr(), Qt::ElideRight, avail));
}

void SongTile::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    applyElide();
}

void SongTile::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
        emit activated(m_track);
    QFrame::mouseReleaseEvent(event);
}

void SongTile::contextMenuEvent(QContextMenuEvent *event)
{
    emit menuRequested(m_track, event->globalPos());
    event->accept();
}

// ------------------------------------------------------------- SongGridScroll

namespace {
constexpr int kGridSideMargin = 28;
constexpr int kGridSpacing = 10;
constexpr int kGridMinColWidth = 220;
constexpr int kScrollbarExtent = 16;
} // namespace

SongGridScroll::SongGridScroll(int rows, int visibleCols, QWidget *parent)
    : QScrollArea(parent), m_rows(rows), m_visibleCols(visibleCols)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setObjectName(QStringLiteral("hscrollArea"));

    m_body = new QWidget;
    m_body->setObjectName(QStringLiteral("hbody"));
    m_grid = new QGridLayout(m_body);
    m_grid->setContentsMargins(kGridSideMargin, 4, kGridSideMargin, 4);
    m_grid->setHorizontalSpacing(kGridSpacing);
    m_grid->setVerticalSpacing(kGridSpacing);
    m_grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setWidget(m_body);

    const int contentH = m_rows * kTileHeight + (m_rows - 1) * kGridSpacing + 8;
    setFixedHeight(contentH);
}

void SongGridScroll::setTracks(const QList<Track> &tracks)
{
    clearLayout(m_grid);
    m_tiles.clear();

    for (int i = 0; i < tracks.size(); ++i) {
        auto *tile = new SongTile(tracks.at(i), m_body);
        connect(tile, &SongTile::activated, this, [this, i](const Track &) { emit trackActivated(i); });
        connect(tile, &SongTile::menuRequested, this,
                [this](const Track &t, const QPoint &pos) { emit menuRequested(t, pos); });
        // Isi per kolom: 3 lagu ke bawah, lalu geser ke kolom berikutnya.
        m_grid->addWidget(tile, i % m_rows, i / m_rows);
        m_tiles << tile;
    }

    // Sisihkan ruang scrollbar hanya bila memang ada kolom di luar layar.
    const int contentH = m_rows * kTileHeight + (m_rows - 1) * kGridSpacing + 8;
    const bool needScroll = tracks.size() > m_rows * m_visibleCols;
    setFixedHeight(contentH + (needScroll ? kScrollbarExtent : 0));
    setHorizontalScrollBarPolicy(needScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    relayout();
}

void SongGridScroll::relayout()
{
    const int avail = viewport()->width() - 2 * kGridSideMargin - (m_visibleCols - 1) * kGridSpacing;
    const int colW = qMax(kGridMinColWidth, avail / qMax(1, m_visibleCols));
    for (SongTile *t : std::as_const(m_tiles))
        t->setFixedWidth(colW);
}

void SongGridScroll::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    relayout();
}

void SongGridScroll::wheelEvent(QWheelEvent *event) { scrollHorizontally(this, event); }

// ------------------------------------------------------------------ SongRow

SongRow::SongRow(const Track &track, int index, bool showAlbum, bool showCover, QWidget *parent)
    : QFrame(parent), m_track(track), m_index(index), m_playIndex(index - 1),
      m_showAlbum(showAlbum)
{
    setObjectName(QStringLiteral("songRow"));
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(58);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 4, 14, 4);
    lay->setSpacing(12);

    m_indexLabel = makeLabel(index > 0 ? QString::number(index) : QStringLiteral("•"),
                             QStringLiteral("dim"), 10);
    m_indexLabel->setFixedWidth(24);
    m_indexLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_indexLabel);

    if (showCover) {
        m_thumb = new ThumbLabel(44, this);
        if (!track.thumbnail.isEmpty())
            m_thumb->setUrl(track.thumbnail, true);
        lay->addWidget(m_thumb);
    }

    auto *mid = new QVBoxLayout;
    mid->setContentsMargins(0, 0, 0, 0);
    mid->setSpacing(1);
    m_titleFull = track.title;
    m_titleLabel = makeLabel(track.title, QStringLiteral("title"), 11, true);
    mid->addWidget(m_titleLabel);

    m_artistLabel = new ClickableLabel(track.artistStr());
    m_artistLabel->setRole(QStringLiteral("sub"));
    m_artistLabel->setFont(theme::font(9));
    connect(m_artistLabel, &ClickableLabel::clicked, this, [this]() {
        if (!m_track.artistIds.isEmpty())
            emit artistClicked(m_track.artistIds.first());
    });
    mid->addWidget(m_artistLabel);
    lay->addLayout(mid, 1);

    if (showAlbum) {
        m_albumFull = track.album;
        m_albumLabel = makeLabel(track.album, QStringLiteral("sub"), 10);
        m_albumLabel->setFixedWidth(190);
        lay->addWidget(m_albumLabel);
    }

    m_likeBtn = makeIconButton(icons::Kind::Heart, TR("Suka"));
    m_likeBtn->setFixedWidth(34);
    connect(m_likeBtn, &QPushButton::clicked, this, [this]() {
        m_liked = !m_liked;
        setLiked(m_liked);
        emit likeToggled(m_track);
    });
    lay->addWidget(m_likeBtn);

    m_durationLabel = makeLabel(track.displayDuration(), QStringLiteral("dim"), 10);
    m_durationLabel->setFixedWidth(56);
    m_durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_durationLabel);

    auto *menuBtn = makeIconButton(icons::Kind::More, QStringLiteral("Menu"));
    menuBtn->setFixedWidth(30);
    connect(menuBtn, &QPushButton::clicked, this, [this, menuBtn]() {
        emit menuRequested(m_track, menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });
    lay->addWidget(menuBtn);
}

void SongRow::setActive(bool active)
{
    setProperty("active", active);
    repolish(this);
    if (m_indexLabel)
        m_indexLabel->setVisible(!active);
}

void SongRow::setLiked(bool liked)
{
    m_liked = liked;
    if (!m_likeBtn)
        return;
    setLikeIcon(m_likeBtn, liked);
    m_likeBtn->setProperty("active", liked);
    repolish(m_likeBtn);
}

void SongRow::setIndex(int index)
{
    m_index = index;
    m_playIndex = index - 1;
    if (m_indexLabel)
        m_indexLabel->setText(QString::number(index));
}

void SongRow::setDurationText(const QString &text)
{
    if (m_durationLabel)
        m_durationLabel->setText(text);
}

void SongRow::applyElide()
{
    if (m_titleLabel) {
        const int avail = qMax(60, m_titleLabel->width());
        m_titleLabel->setText(
            m_titleLabel->fontMetrics().elidedText(m_titleFull, Qt::ElideRight, avail));
    }
    if (m_albumLabel && m_showAlbum) {
        const int avail = qMax(40, m_albumLabel->width());
        m_albumLabel->setText(
            m_albumLabel->fontMetrics().elidedText(m_albumFull, Qt::ElideRight, avail));
    }
}

void SongRow::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    // Baris sempit: kolom album disembunyikan agar judul & tombol tidak berdesakan.
    if (m_albumLabel && m_showAlbum) {
        const bool wide = width() >= 600;
        if (m_albumLabel->isHidden() == wide)
            m_albumLabel->setVisible(wide);
    }
    applyElide();
}

void SongRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit playRequested(m_track, m_playIndex);
    QFrame::mouseReleaseEvent(event);
}

void SongRow::contextMenuEvent(QContextMenuEvent *event)
{
    emit menuRequested(m_track, event->globalPos());
    event->accept();
}

// ---------------------------------------------------------------- SearchBar

SearchBar::SearchBar(const QString &placeholder, QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(28, 8, 28, 4);
    lay->setSpacing(6);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(placeholder);
    m_edit->setClearButtonEnabled(true);
    m_edit->addAction(icons::icon(icons::Kind::Search, icons::Role::Dim), QLineEdit::LeadingPosition);
    m_edit->installEventFilter(this);
    lay->addWidget(m_edit);

    m_panel = new QFrame(this);
    m_panel->setObjectName(QStringLiteral("suggestPanel"));
    m_panelLayout = new QVBoxLayout(m_panel);
    m_panelLayout->setContentsMargins(6, 6, 6, 6);
    m_panelLayout->setSpacing(0);
    m_panel->hide();
    lay->addWidget(m_panel);

    connect(m_edit, &QLineEdit::textEdited, this,
            [this](const QString &t) { emit textEdited(t); });
}

QString SearchBar::text() const { return m_edit->text(); }

void SearchBar::setText(const QString &text) { m_edit->setText(text); }

void SearchBar::focusEdit() { m_edit->setFocus(); }

void SearchBar::hideSuggestions()
{
    m_highlight = -1;
    m_buttons.clear();
    while (QLayoutItem *item = m_panelLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_panel->hide();
}

void SearchBar::setSuggestions(const QStringList &items)
{
    hideSuggestions();
    if (items.isEmpty())
        return;
    for (const QString &item : items) {
        auto *btn = new QPushButton(item, m_panel);
        btn->setIcon(icons::icon(icons::Kind::Search, icons::Role::Sub));
        btn->setIconSize(QSize(14, 14));
        btn->setObjectName(QStringLiteral("sideBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        connect(btn, &QPushButton::clicked, this, [this, item]() {
            m_edit->setText(item);
            hideSuggestions();
            emit suggestionActivated(item);
        });
        m_panelLayout->addWidget(btn);
        m_buttons << btn;
    }
    m_panel->show();
}

void SearchBar::highlight(int index)
{
    if (m_buttons.isEmpty()) {
        m_highlight = -1;
        return;
    }
    if (index < 0)
        index = m_buttons.size() - 1;
    if (index >= m_buttons.size())
        index = 0;
    m_highlight = index;
    for (int i = 0; i < m_buttons.size(); ++i) {
        m_buttons[i]->setProperty("active", i == index);
        repolish(m_buttons[i]);
    }
}

bool SearchBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_edit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Down:
            if (!m_buttons.isEmpty()) {
                highlight(m_highlight + 1);
                return true;
            }
            break;
        case Qt::Key_Up:
            if (!m_buttons.isEmpty()) {
                highlight(m_highlight - 1);
                return true;
            }
            break;
        case Qt::Key_Escape:
            if (m_panel->isVisible()) {
                hideSuggestions();
                return true;
            }
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_highlight >= 0 && m_highlight < m_buttons.size()) {
                const QString item = m_buttons[m_highlight]->text().trimmed();
                m_edit->setText(item);
                hideSuggestions();
                emit submitted(item);
                return true;
            }
            hideSuggestions();
            emit submitted(m_edit->text().trimmed());
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace bc

