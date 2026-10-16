#pragma once

#include <QIcon>
#include <QPainter>
#include <QRectF>
#include <QWidget>

namespace bc {
namespace icons {

// Semua ikon digambar sendiri lewat QPainter (vektor) sehingga TIDAK bergantung
// pada font emoji/simbol sistem. Di Linux tanpa Noto Color Emoji / Symbola,
// glyph seperti ⏮ ⏭ 🎤 🔀 tampil sebagai kotak kosong.
enum class Kind {
    Home, Search, Library, Heart, HeartFilled, Disc, History, Shuffle, Repeat,
    Prev, Next, Play, Mic, Headphones, Volume, Menu, Back, More, Trash, Note,
    Warning, QueueNext, Plus, Artist, Berry, Dot, Check, Info
};

// Warna ikon diambil dari tema AKTIF saat digambar, jadi otomatis ikut berganti
// ketika tema diganti (tanpa perlu membuat ulang tombolnya).
enum class Role { Sub, Text, Accent, OnAccent, Dim, White };

QColor colorFor(Role role);

// Gambar ikon `kind` pada kotak `rect` dengan warna `color`.
void paint(QPainter *p, const QRectF &rect, Kind kind, const QColor &color);

QIcon icon(Kind kind, Role role = Role::Sub);

// Widget kecil yang menggambar satu ikon (pengganti QLabel berisi emoji).
class IconWidget : public QWidget
{
public:
    IconWidget(Kind kind, Role role, int size, QWidget *parent = nullptr);
    void setKind(Kind kind);
    void setRole(Role role);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Kind m_kind;
    Role m_role;
};

} // namespace icons
} // namespace bc
