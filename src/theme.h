#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>

namespace theme {

enum class Id { Blackcurrant, Dark, Light };

struct Palette {
    QString name;
    QString label;
    QColor bg;          // latar konten
    QColor sidebarBg;   // sidebar
    QColor card;        // kartu / panel
    QColor cardHover;   // kartu saat hover
    QColor rowHover;    // baris daftar saat hover
    QColor topbarBg;
    QColor playerBg;
    QColor divider;
    QColor border;
    QColor accent;
    QColor accentHover;
    QColor accentText;  // teks di atas warna aksen
    QColor text;
    QColor subtext;
    QColor subtextDim;
    QColor sliderBg;
    QColor error;
    QColor ok;
};

const Palette &palette();
// Palet tema tertentu (tanpa mengganti tema aktif) — dipakai untuk pratinjau di dropdown.
Palette paletteOf(Id id);
Id current();
void setCurrent(Id id);
QString currentName();
Id idFromName(const QString &name);
QString nameOf(Id id);
QString labelOf(Id id);

// Susun stylesheet QSS global untuk tema aktif.
QString styleSheet();

// Terapkan stylesheet ke QApplication.
void apply();

// Font helper.
QFont font(int pointSize, bool bold = false);

const QStringList &genreEmoji();
const QStringList &genreColors();
const QStringList &genreNames();

} // namespace theme
