#pragma once

#include "icons.h"
#include "track.h"

#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QString>
#include <QWidget>

#include <functional>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLineEdit;

namespace bc {

// ------------------------------------------------------- ukuran seragam

// Semua kartu (lagu/album) di rak horizontal memakai lebar & tinggi yang sama
// agar beranda tidak terlihat "ada yang kecil, ada yang besar".
constexpr int kCardWidth = 150;
inline int cardHeightFor(int width) { return width + 72; }
// Tinggi rak horizontal = kartu + margin atas/bawah + ruang scrollbar.
inline int shelfHeightFor(int cardWidth) { return cardHeightFor(cardWidth) + 16 + 14; }

constexpr int kTileHeight = 64;   // tinggi satu sel "terakhir diputar"
constexpr int kGenreTileHeight = 84;

// Potong teks agar muat dalam `maxLines` baris pada lebar tertentu (… di akhir).
QString elideLines(const QString &text, const QFontMetrics &fm, int width, int maxLines);

// ---------------------------------------------------------------- helpers

QLabel *makeLabel(const QString &text, const QString &role = QString(),
                  int pointSize = 0, bool bold = false);
QPushButton *makeButton(const QString &text, const QString &objectName);
QPushButton *makeIconButton(const QString &text, const QString &tooltip = QString());
// Tombol ikon vektor (tanpa glyph/emoji font) — warna ikut tema aktif.
QPushButton *makeIconButton(icons::Kind kind, const QString &tooltip = QString(),
                            icons::Role role = icons::Role::Sub);
// Tombol bertulisan dengan ikon vektor di kiri teks.
QPushButton *makeButton(icons::Kind kind, const QString &text, const QString &objectName,
                        icons::Role role = icons::Role::Text);
// Ikon hati untuk tombol "suka": kosong (abu) / terisi (aksen).
void setLikeIcon(QPushButton *button, bool liked);
// Ganti ikon tombol ikon sambil menandai aktif (aksen) / tidak aktif.
void setActiveIcon(QPushButton *button, icons::Kind kind, bool active);

QWidget *makeSectionHeader(const QString &title, const QString &subtitle = QString(),
                           const QString &actionText = QString(),
                           const std::function<void()> &action = nullptr);

QLabel *makeLoadingLabel(const QString &text);
QLabel *makeEmptyLabel(const QString &text);
QWidget *makeErrorBlock(const QString &title, const QString &subtitle, const QString &error,
                        const std::function<void()> &retry);

// ---------------------------------------------------------------- helpers

void repolish(QWidget *widget);

// Hapus dan hancurkan semua isi layout.
void clearLayout(QLayout *layout);

// -------------------------------------------------------------- thumbnail

class ThumbLabel : public QLabel
{
    Q_OBJECT
public:
    ThumbLabel(int size, QWidget *parent = nullptr);
    void setUrl(const QString &url, bool rounded = true);
    int size() const { return m_size; }
    void setSize(int size);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh();
    int m_size;
    QString m_url;
    bool m_rounded = true;
    bool m_hasPixmap = false; // false -> gambar placeholder (ikon nada) agar tidak "kotak kosong"
};

// ------------------------------------------------------------ slider klik

// QSlider yang melompat ke posisi yang diklik pada alur (groove), lalu tetap bisa
// diseret. QSlider bawaan hanya bergerak per "page step" bila alurnya diklik, dan
// hanya mau diseret bila yang ditarik adalah bulatannya.
class ClickSlider : public QSlider
{
    Q_OBJECT
public:
    explicit ClickSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};

// --------------------------------------------------------- area bergulir

class ScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit ScrollArea(QWidget *parent = nullptr);
    QWidget *body() const { return m_body; }
    QVBoxLayout *contentLayout() const { return m_layout; }
    void addWidget(QWidget *widget);
    void addLayout(QLayout *layout);
    void clear();
    void resetTop();

private:
    QWidget *m_body = nullptr;
    QVBoxLayout *m_layout = nullptr;
};

class HScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit HScrollArea(int height = shelfHeightFor(kCardWidth), QWidget *parent = nullptr);
    QWidget *body() const { return m_body; }
    QHBoxLayout *contentLayout() const { return m_layout; }
    void clear();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QWidget *m_body = nullptr;
    QHBoxLayout *m_layout = nullptr;
};

// ------------------------------------------------------ tombol play bulat

// Tombol bulat yang menggambar sendiri ikon play/pause lewat QPainter sehingga
// segitiganya tepat di tengah lingkaran (glyph teks "▶" selalu bergeser).
class PlayButton : public QPushButton
{
    Q_OBJECT
public:
    explicit PlayButton(int diameter = 42, QWidget *parent = nullptr);
    void setPlaying(bool playing);
    bool playing() const { return m_playing; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_playing = false;
};

// -------------------------------------------------------------- tombol klik

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    ClickableLabel(const QString &text = QString(), QWidget *parent = nullptr);
    void setRole(const QString &role);

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
};

// ------------------------------------------------------------- kartu/baris

class SongCard : public QFrame
{
    Q_OBJECT
public:
    SongCard(const Track &track, int width = 180, QWidget *parent = nullptr);
    Track track() const { return m_track; }

signals:
    void activated(const Track &track);
    void menuRequested(const Track &track, const QPoint &globalPos);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Track m_track;
    int m_width;
    ThumbLabel *m_thumb = nullptr;
    QLabel *m_title = nullptr;
    PlayButton *m_badge = nullptr;
    QString m_titleFull;
};

class PlaylistCard : public QFrame
{
    Q_OBJECT
public:
    PlaylistCard(const QString &title, const QString &subtitle, const QString &thumbnail,
                 int width = 150, QWidget *parent = nullptr);
    void setThumbnail(const QString &url);

signals:
    void activated();
    void menuRequested(const QPoint &globalPos);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int m_width;
    ThumbLabel *m_thumb = nullptr;
    QLabel *m_title = nullptr;
    QString m_titleFull;
};

class GenreTile : public QFrame
{
    Q_OBJECT
public:
    GenreTile(const QString &title, const QString &color, const QString &emoji,
              QWidget *parent = nullptr);

signals:
    void activated();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString m_color;
};

// Sel kompak (cover + judul + artis) untuk grid "Terakhir Diputar".
class SongTile : public QFrame
{
    Q_OBJECT
public:
    explicit SongTile(const Track &track, QWidget *parent = nullptr);
    Track track() const { return m_track; }

signals:
    void activated(const Track &track);
    void menuRequested(const Track &track, const QPoint &globalPos);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyElide();

    Track m_track;
    QLabel *m_title = nullptr;
    QLabel *m_artist = nullptr;
};

// Grid lagu: `rows` baris ke bawah, kolom bertambah ke kanan. `visibleCols`
// kolom pertama langsung terlihat, sisanya digulir ke samping.
class SongGridScroll : public QScrollArea
{
    Q_OBJECT
public:
    explicit SongGridScroll(int rows = 3, int visibleCols = 3, QWidget *parent = nullptr);
    void setTracks(const QList<Track> &tracks);

signals:
    void trackActivated(int index);
    void menuRequested(const Track &track, const QPoint &globalPos);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void relayout();

    int m_rows;
    int m_visibleCols;
    QWidget *m_body = nullptr;
    QGridLayout *m_grid = nullptr;
    QList<SongTile *> m_tiles;
};

class SongRow : public QFrame
{
    Q_OBJECT
public:
    SongRow(const Track &track, int index, bool showAlbum, bool showCover,
            QWidget *parent = nullptr);

    void setActive(bool active);
    void setLiked(bool liked);
    void setIndex(int index);
    void setDurationText(const QString &text);
    Track track() const { return m_track; }
    int playIndex() const { return m_playIndex; }

signals:
    void playRequested(const Track &track, int index);
    void menuRequested(const Track &track, const QPoint &globalPos);
    void likeToggled(const Track &track);
    void artistClicked(const QString &artistId);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyElide();

    Track m_track;
    int m_index;
    int m_playIndex; // indeks berbasis 0 untuk antrean pemutaran
    bool m_showAlbum;
    QLabel *m_indexLabel = nullptr;
    ThumbLabel *m_thumb = nullptr;
    QLabel *m_titleLabel = nullptr;
    ClickableLabel *m_artistLabel = nullptr;
    QLabel *m_albumLabel = nullptr;
    QPushButton *m_likeBtn = nullptr;
    QLabel *m_durationLabel = nullptr;
    bool m_liked = false;
    QString m_titleFull;
    QString m_albumFull;
};

// Kotak pencarian dengan daftar rekomendasi (mendorong konten ke bawah).
class SearchBar : public QWidget
{
    Q_OBJECT
public:
    SearchBar(const QString &placeholder, QWidget *parent = nullptr);
    QString text() const;
    void setText(const QString &text);
    void setSuggestions(const QStringList &items);
    void hideSuggestions();
    void focusEdit();

signals:
    void textEdited(const QString &text);
    void submitted(const QString &text);
    void suggestionActivated(const QString &text);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void highlight(int index);
    QLineEdit *m_edit = nullptr;
    QFrame *m_panel = nullptr;
    QVBoxLayout *m_panelLayout = nullptr;
    QList<QPushButton *> m_buttons;
    int m_highlight = -1;
};

} // namespace bc
