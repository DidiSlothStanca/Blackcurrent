#include "i18n.h"
#include "theme.h"

#include <QApplication>

namespace theme {
namespace {

Palette makeBlackcurrant()
{
    Palette p;
    p.name = QStringLiteral("blackcurrant");
    p.label = QStringLiteral("Blackcurrant");
    p.bg = QColor("#140f1c");
    p.sidebarBg = QColor("#0b0712");
    p.card = QColor("#1e1730");
    p.cardHover = QColor("#2b2144");
    p.rowHover = QColor("#251c38");
    p.topbarBg = QColor("#140f1c");
    p.playerBg = QColor("#100b1a");
    p.divider = QColor("#2c2342");
    p.border = QColor("#2f2547");
    p.accent = QColor("#a855f7");
    p.accentHover = QColor("#bb7bff");
    p.accentText = QColor("#ffffff");
    p.text = QColor("#f4eefb");
    p.subtext = QColor("#b3a6cf");
    p.subtextDim = QColor("#746a89");
    p.sliderBg = QColor("#453a5e");
    p.error = QColor("#ff6b8a");
    p.ok = QColor("#c084fc");
    return p;
}

Palette makeDark()
{
    Palette p;
    p.name = QStringLiteral("dark");
    p.label = QStringLiteral("Gelap");
    p.bg = QColor("#121212");
    p.sidebarBg = QColor("#000000");
    p.card = QColor("#1e1e1e");
    p.cardHover = QColor("#2a2a2a");
    p.rowHover = QColor("#242424");
    p.topbarBg = QColor("#121212");
    p.playerBg = QColor("#181818");
    p.divider = QColor("#2a2a2a");
    p.border = QColor("#2a2a2a");
    p.accent = QColor("#ff7a59");
    p.accentHover = QColor("#ff9377");
    p.accentText = QColor("#20140b");
    p.text = QColor("#ffffff");
    p.subtext = QColor("#b3b3b3");
    p.subtextDim = QColor("#7f7f7f");
    p.sliderBg = QColor("#4d4d4d");
    p.error = QColor("#f15e6c");
    p.ok = QColor("#ffb28f");
    return p;
}

Palette makeLight()
{
    Palette p;
    p.name = QStringLiteral("light");
    p.label = QStringLiteral("Terang");
    p.bg = QColor("#f5f3f8");
    p.sidebarBg = QColor("#ffffff");
    p.card = QColor("#ffffff");
    p.cardHover = QColor("#ece7f3");
    p.rowHover = QColor("#ece7f3");
    p.topbarBg = QColor("#ffffff");
    p.playerBg = QColor("#ffffff");
    p.divider = QColor("#e3deec");
    p.border = QColor("#e3deec");
    p.accent = QColor("#7c3aed");
    p.accentHover = QColor("#9257f0");
    p.accentText = QColor("#ffffff");
    p.text = QColor("#1c1626");
    p.subtext = QColor("#5d5470");
    p.subtextDim = QColor("#8d849c");
    p.sliderBg = QColor("#d3ccdf");
    p.error = QColor("#d6336c");
    p.ok = QColor("#7c3aed");
    return p;
}

Palette g_pal = makeBlackcurrant();
Id g_id = Id::Blackcurrant;

const QStringList kGenreEmoji = {
    QStringLiteral("🎸"), QStringLiteral("🎧"), QStringLiteral("💜"), QStringLiteral("🔥"),
    QStringLiteral("🌊"), QStringLiteral("✨"), QStringLiteral("🌙"), QStringLiteral("🚗"),
    QStringLiteral("💪"), QStringLiteral("☕"), QStringLiteral("💔"), QStringLiteral("🎉"),
    QStringLiteral("🌍"), QStringLiteral("🎺"), QStringLiteral("🎻"), QStringLiteral("📀"),
    QStringLiteral("🤍"), QStringLiteral("⚡")
};

const QStringList kGenreColors = {
    QStringLiteral("#5b2a86"), QStringLiteral("#8e3b9e"), QStringLiteral("#b13bb8"),
    QStringLiteral("#6a3fa0"), QStringLiteral("#a03b6b"), QStringLiteral("#c2557a"),
    QStringLiteral("#7a3cc4"), QStringLiteral("#9b51e0"), QStringLiteral("#6d4c9f"),
    QStringLiteral("#4a2c63"), QStringLiteral("#b5651d"), QStringLiteral("#c97b2e"),
    QStringLiteral("#8e4a2f"), QStringLiteral("#7a3b5d"), QStringLiteral("#5a2e7a"),
    QStringLiteral("#93489e")
};

const QStringList kGenreNames = {
    QStringLiteral("Pop"), QStringLiteral("Hip-Hop"), QStringLiteral("Rock"),
    QStringLiteral("Jazz"), QStringLiteral("Klasik"), QStringLiteral("Dangdut"),
    QStringLiteral("EDM"), QStringLiteral("Indie")
};

QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

void rx(QString &s, const QString &token, const QString &value)
{
    s.replace(token, value);
}

} // namespace

const Palette &palette() { return g_pal; }

Palette paletteOf(Id id)
{
    switch (id) {
    case Id::Dark: return makeDark();
    case Id::Light: return makeLight();
    default: return makeBlackcurrant();
    }
}

Id current() { return g_id; }

QString currentName() { return g_pal.name; }

Id idFromName(const QString &name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("dark") || n == QLatin1String("gelap"))
        return Id::Dark;
    if (n == QLatin1String("light") || n == QLatin1String("terang"))
        return Id::Light;
    return Id::Blackcurrant;
}

QString nameOf(Id id)
{
    switch (id) {
    case Id::Dark: return QStringLiteral("dark");
    case Id::Light: return QStringLiteral("light");
    default: return QStringLiteral("blackcurrant");
    }
}

QString labelOf(Id id)
{
    switch (id) {
    case Id::Dark: return TR("Gelap");
    case Id::Light: return TR("Terang");
    default: return QStringLiteral("Blackcurrant");
    }
}

void setCurrent(Id id)
{
    g_id = id;
    switch (id) {
    case Id::Dark: g_pal = makeDark(); break;
    case Id::Light: g_pal = makeLight(); break;
    default: g_pal = makeBlackcurrant(); break;
    }
}

QFont font(int pointSize, bool bold)
{
    QFont f(QStringLiteral("Noto Sans"));
    f.setStyleHint(QFont::SansSerif);
    f.setPointSize(pointSize);
    f.setBold(bold);
    return f;
}

const QStringList &genreEmoji() { return kGenreEmoji; }
const QStringList &genreColors() { return kGenreColors; }
const QStringList &genreNames() { return kGenreNames; }

QString styleSheet()
{
    const Palette &p = g_pal;
    QString css = QStringLiteral(R"QSS(
* { outline: none; }
QMainWindow, QDialog { background: @bg@; }
QWidget#root, QWidget#content, QWidget#viewHost, QWidget#scrollBody,
QWidget#cardBody, QWidget#hbody { background: @bg@; }
QWidget#sidebar { background: @sidebar@; }
QWidget#topbar, QWidget#topbarInner { background: @topbar@; }
QWidget#playerBar { background: @player@; }
QWidget#lyricsPanel { background: @card@; }
QWidget#suggestPanel { background: @card@; border: 1px solid @border@; border-radius: 10px; }
QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QAbstractScrollArea::corner { background: transparent; }
QLabel { color: @text@; background: transparent; }
QLabel[role="sub"] { color: @sub@; }
QLabel[role="dim"] { color: @dim@; }
QLabel[role="accent"] { color: @accent@; }
QLabel[role="title"] { color: @text@; font-weight: bold; }
QToolTip { background: @card@; color: @text@; border: 1px solid @border@; padding: 4px; }
QFrame#divider { background: @border@; border: none; }

QPushButton { background: transparent; color: @text@; border: none; padding: 6px 10px; border-radius: 8px; }
QPushButton:hover { background: @cardhover@; }
QPushButton#navItem { color: @sub@; text-align: left; padding: 9px 12px; font-weight: bold; }
QPushButton#navItem:hover { color: @text@; background: @rowhover@; }
QPushButton#navItem[active="true"] { color: @text@; background: @rowhover@; }
QPushButton#iconBtn { color: @sub@; padding: 4px 9px; font-size: 14pt; }
QPushButton#iconBtn:hover { color: @text@; background: @rowhover@; }
QPushButton#iconBtn[active="true"] { color: @accent@; }
QPushButton#primaryBtn { background: @accent@; color: @accenttext@; font-weight: bold; padding: 9px 20px; border-radius: 18px; }
QPushButton#primaryBtn:hover { background: @accenthover@; }
QPushButton#ghostBtn { border: 1px solid @border@; color: @text@; padding: 8px 16px; border-radius: 18px; }
QPushButton#ghostBtn:hover { border-color: @accent@; background: @rowhover@; }
QPushButton#linkBtn { color: @sub@; padding: 2px 4px; }
QPushButton#linkBtn:hover { color: @text@; background: transparent; }
QPushButton#sideBtn { color: @sub@; text-align: left; padding: 7px 12px; }
QPushButton#sideBtn:hover { color: @text@; background: @rowhover@; }

QFrame#songCard, QFrame#playlistCard { background: @card@; border-radius: 12px; }
QFrame#songCard:hover, QFrame#playlistCard:hover { background: @cardhover@; }
QFrame#songTile { background: @card@; border-radius: 10px; }
QFrame#songTile:hover { background: @cardhover@; }
QFrame#songRow { background: transparent; border-radius: 8px; }
QFrame#songRow:hover { background: @rowhover@; }
QFrame#genreTile { border-radius: 12px; }
QFrame#panel { background: @card@; border-radius: 12px; }

QLineEdit { background: @card@; color: @text@; border: 1px solid @border@; border-radius: 20px; padding: 9px 16px; selection-background-color: @accent@; }
QLineEdit:focus { border: 1px solid @accent@; }
QLineEdit#flatInput { border: none; background: transparent; padding: 6px 8px; }

QComboBox { background: @card@; color: @text@; border: 1px solid @border@; border-radius: 10px; padding: 6px 12px; min-height: 22px; }
QComboBox:hover { border-color: @accent@; }
QComboBox:focus, QComboBox:on { border: 1px solid @accent@; }
QComboBox QAbstractItemView { background: @card@; color: @text@; border: 1px solid @accent@; border-radius: 8px; padding: 4px; outline: none; selection-background-color: @accent@; selection-color: @accenttext@; }
QComboBox QAbstractItemView::item { min-height: 30px; padding: 4px 8px; border-radius: 6px; color: @text@; background: transparent; }
QComboBox QAbstractItemView::item:hover { background: @accent@; color: @accenttext@; }
QComboBox QAbstractItemView::item:selected { background: @accent@; color: @accenttext@; }

QSlider { min-height: 22px; }
QSlider::groove:horizontal { height: 5px; background: @slider@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::handle:horizontal { background: @text@; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }
QSlider::handle:horizontal:hover { background: @accent@; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @slider@; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @accent@; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: @slider@; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: @accent@; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QMenu { background: @card@; color: @text@; border: 1px solid @border@; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 7px 22px; border-radius: 6px; }
QMenu::item:selected { background: @accent@; color: @accenttext@; }
QMenu::separator { height: 1px; background: @border@; margin: 5px 8px; }
)QSS");

    rx(css, "@bg@", hex(p.bg));
    rx(css, "@sidebar@", hex(p.sidebarBg));
    rx(css, "@card@", hex(p.card));
    rx(css, "@cardhover@", hex(p.cardHover));
    rx(css, "@rowhover@", hex(p.rowHover));
    rx(css, "@topbar@", hex(p.topbarBg));
    rx(css, "@player@", hex(p.playerBg));
    rx(css, "@border@", hex(p.border));
    rx(css, "@accent@", hex(p.accent));
    rx(css, "@accenthover@", hex(p.accentHover));
    rx(css, "@accenttext@", hex(p.accentText));
    rx(css, "@text@", hex(p.text));
    rx(css, "@sub@", hex(p.subtext));
    rx(css, "@dim@", hex(p.subtextDim));
    rx(css, "@slider@", hex(p.sliderBg));
    return css;
}

void apply()
{
    if (qApp)
        qApp->setStyleSheet(styleSheet());
}

} // namespace theme
