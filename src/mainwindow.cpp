#include "i18n.h"
#include "mainwindow.h"

#include "bridge.h"
#include "icons.h"
#include "player.h"
#include "storage.h"
#include "theme.h"
#include "version.h"
#include "thumbnails.h"
#include "widgets.h"
#include "views/aboutview.h"
#include "views/artistview.h"
#include "views/detailview.h"
#include "views/homeview.h"
#include "views/libraryview.h"
#include "views/lyricsview.h"
#include "views/searchview.h"

#include <QApplication>
#include <QAudioDevice>
#include <QCloseEvent>
#include <QComboBox>
#include <QCursor>
#include <QEasingCurve>
#include <QEvent>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QListView>
#include <QPainter>
#include <QPixmap>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace bc {

namespace {
constexpr int kSidebarWidth = 232;
constexpr int kLyricsWidth = 380;

QString fmt(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 h = totalSec / 3600;
    const qint64 m = (totalSec % 3600) / 60;
    const qint64 s = totalSec % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("%1 v%2")
                       .arg(QString::fromUtf8(appinfo::kName),
                            QString::fromUtf8(appinfo::kVersion)));
    resize(1240, 800);
    setMinimumSize(640, 520);

    m_storage = new Storage();
    i18n::setLanguage(i18n::fromCode(
        m_storage->setting(QStringLiteral("language"), QJsonValue(QStringLiteral("en")))
            .toString()));
    m_player = new AudioPlayer(this);
    m_bridge = new Bridge(this);

    ThumbnailLoader::instance().setCacheDir(m_storage->dataDir() + QStringLiteral("/cache"));

    restoreVolume();
    restoreWindowState();

    buildUi();

    connect(m_player, &AudioPlayer::started, this, &MainWindow::onPlayerStarted);
    connect(m_player, &AudioPlayer::finished, this, &MainWindow::onPlayerFinished);
    connect(m_player, &AudioPlayer::failed, this, &MainWindow::onPlayerFailed);
    connect(m_player, &AudioPlayer::playingChanged, this,
            [this](bool) { refreshPlayerBar(); });
    connect(m_player, &AudioPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(m_player, &AudioPlayer::resolvingChanged, this, &MainWindow::onResolvingChanged);
    connect(m_bridge, &Bridge::logMessage, this, [](const QString &line) {
        qWarning().noquote() << "[bridge]" << line;
    });

    const QString savedTheme = m_storage->setting(QStringLiteral("theme"),
                                                  QJsonValue(QStringLiteral("blackcurrant")))
                                   .toString();
    theme::setCurrent(theme::idFromName(savedTheme));
    applyThemeToApp();

    refreshSidebarPlaylists();
    refreshAudioStatus();
    showHome();

    // Tampilkan lagu terakhir yang diputar di bilah pemutar (tanpa langsung memutar).
    const QList<Track> last = m_storage->history(30);
    if (!last.isEmpty()) {
        m_queue = last;
        m_queueIndex = 0;
        setNowPlaying(last.first());
    }
}

MainWindow::~MainWindow() { delete m_storage; }

// ===========================================================================
// Tata letak
// ===========================================================================

void MainWindow::buildUi()
{
    m_central = new QWidget;
    m_central->setObjectName(QStringLiteral("root"));
    setCentralWidget(m_central);

    auto *root = new QHBoxLayout(m_central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);

    right->addWidget(buildTopbar());

    auto *body = new QWidget;
    body->setObjectName(QStringLiteral("viewHost"));
    auto *bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    m_content = new QWidget;
    m_content->setObjectName(QStringLiteral("content"));
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);
    bodyLay->addWidget(m_content, 1);

    buildLyricsPanel();
    bodyLay->addWidget(m_lyrics);

    right->addWidget(body, 1);
    right->addWidget(buildPlayerBar());

    root->addLayout(right, 1);

    // Sidebar = laci yang meluncur di atas konten. Tertutup sampai ☰ ditekan, dan
    // menutup lagi setelah memilih menu atau mengklik area gelap di luarnya.
    m_scrim = new QWidget(m_central);
    m_scrim->setObjectName(QStringLiteral("scrim"));
    m_scrim->setAttribute(Qt::WA_StyledBackground, true);
    m_scrim->setStyleSheet(QStringLiteral("QWidget#scrim { background: rgba(0, 0, 0, 140); }"));
    m_scrim->hide();
    m_scrim->installEventFilter(this);

    QWidget *sidebar = buildSidebar();
    sidebar->setParent(m_central);
    sidebar->setAttribute(Qt::WA_StyledBackground, true);
    sidebar->hide();

    m_sidebarAnim = new QPropertyAnimation(m_sidebar, "pos", this);
    m_sidebarAnim->setDuration(170);
    m_sidebarAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_sidebarAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_sidebarOpen) {
            m_sidebar->hide();
            m_scrim->hide();
        }
    });
    m_central->installEventFilter(this);

    // Toast overlay
    m_toast = new QLabel(m_central);
    m_toast->setObjectName(QStringLiteral("toast"));
    m_toast->setStyleSheet(
        QStringLiteral("QLabel#toast { background: %1; color: %2; border-radius: 10px; "
                       "padding: 10px 18px; border: 1px solid %3; }")
            .arg(theme::palette().card.name(), theme::palette().text.name(),
                 theme::palette().border.name()));
    m_toast->hide();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() { m_toast->hide(); });
}

QWidget *MainWindow::buildSidebar()
{
    m_sidebar = new QWidget;
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(kSidebarWidth);

    auto *lay = new QVBoxLayout(m_sidebar);
    lay->setContentsMargins(16, 22, 16, 18);
    lay->setSpacing(4);

    auto *logoRow = new QHBoxLayout;
    logoRow->setContentsMargins(0, 0, 0, 0);
    logoRow->setSpacing(8);
    {
        // Logo aplikasi (resources/blackcurrant.png), digambar tajam di layar HiDPI.
        auto *logo = new QLabel;
        const qreal dpr = devicePixelRatioF();
        const int logoSize = 30;
        QPixmap pm = QPixmap(QStringLiteral(":/blackcurrant.png"))
                         .scaled(QSize(logoSize, logoSize) * dpr, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        pm.setDevicePixelRatio(dpr);
        logo->setPixmap(pm);
        logo->setFixedSize(logoSize, logoSize);
        logoRow->addWidget(logo);
    }
    logoRow->addWidget(makeLabel(QStringLiteral("Blackcurrant"), QString(), 15, true));
    logoRow->addStretch();
    lay->addLayout(logoRow);
    lay->addWidget(makeLabel(QStringLiteral("Music Player  \u2022  v%1")
                                 .arg(QString::fromUtf8(appinfo::kVersion)),
                             QStringLiteral("dim"), 9));
    lay->addSpacing(14);

    struct NavItem {
        QString key;
        QString label;
        QString title;
        icons::Kind kind;
    };
    const QList<NavItem> items = {
        {QStringLiteral("home"), TR("Beranda"), TR("Beranda"),
         icons::Kind::Home},
        {QStringLiteral("search"), TR("Pencarian"), TR("Pencarian"),
         icons::Kind::Search},
        {QStringLiteral("library"), TR("Koleksi"), TR("Koleksi"),
         icons::Kind::Library},
        {QStringLiteral("liked"), TR("Lagu Disukai"), TR("Lagu Disukai"),
         icons::Kind::Heart},
        {QStringLiteral("liked_albums"), TR("Album Disukai"),
         TR("Album Disukai"), icons::Kind::Disc},
        {QStringLiteral("history"), TR("Riwayat Musik"),
         TR("Riwayat Musik"), icons::Kind::History},
        {QStringLiteral("about"), TR("Tentang"), TR("Tentang"),
         icons::Kind::Info},
    };

    for (const NavItem &item : items) {
        auto *btn = new QPushButton(QStringLiteral("  ") + item.label);
        btn->setIcon(icons::icon(item.kind, icons::Role::Sub));
        btn->setIconSize(QSize(18, 18));
        btn->setProperty("iconKind", static_cast<int>(item.kind));
        btn->setObjectName(QStringLiteral("navItem"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFlat(true);
        btn->setProperty("navkey", item.key);
        const QString key = item.key;
        const QString title = item.title;
        connect(btn, &QPushButton::clicked, this, [this, key, title]() { navigate(key, title); });
        lay->addWidget(btn);
        m_navButtons.append(qMakePair(item.key, btn));
    }

    lay->addStretch();

    auto *divider = new QFrame;
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    lay->addWidget(divider);
    lay->addSpacing(6);

    auto *plHead = new QHBoxLayout;
    plHead->setContentsMargins(4, 0, 0, 0);
    plHead->addWidget(makeLabel(TR("PLAYLIST"), QStringLiteral("dim"), 8, true));
    plHead->addStretch();
    auto *plusBtn = makeIconButton(icons::Kind::Plus, TR("Buat playlist"));
    plusBtn->setFixedSize(24, 24);
    connect(plusBtn, &QPushButton::clicked, this, &MainWindow::createPlaylistDialog);
    plHead->addWidget(plusBtn);
    lay->addLayout(plHead);

    m_playlistList = new QWidget;
    auto *plList = new QVBoxLayout(m_playlistList);
    plList->setContentsMargins(0, 2, 0, 2);
    plList->setSpacing(1);
    lay->addWidget(m_playlistList);

    lay->addStretch();

    m_audioStatus = makeLabel(TR("Memeriksa audio…"), QStringLiteral("dim"), 8);
    m_audioStatus->setWordWrap(false);
    lay->addWidget(m_audioStatus);

    m_netStatus = makeLabel(TR("Online — butuh internet"), QStringLiteral("accent"),
                            8);
    lay->addWidget(m_netStatus);

    lay->addSpacing(8);
    lay->addWidget(makeLabel(TR("Tema"), QStringLiteral("dim"), 9));
    m_themeCombo = new QComboBox;
    // QListView biasa agar item popup bisa diberi gaya hover/terpilih lewat QSS
    // (tanpa ini popup combo tidak menyorot item yang ditunjuk).
    m_themeCombo->setView(new QListView(m_themeCombo));
    m_themeCombo->setIconSize(QSize(34, 20));
    m_themeCombo->setMaxVisibleItems(6);
    m_themeCombo->addItem(QString(), QStringLiteral("blackcurrant"));
    m_themeCombo->addItem(QString(), QStringLiteral("dark"));
    m_themeCombo->addItem(QString(), QStringLiteral("light"));
    refreshThemeCombo();
    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        setTheme(m_themeCombo->currentData().toString());
    });
    lay->addWidget(m_themeCombo);

    lay->addSpacing(6);
    lay->addWidget(makeLabel(TR("Bahasa"), QStringLiteral("dim"), 9));
    m_langCombo = new QComboBox;
    m_langCombo->setView(new QListView(m_langCombo));
    m_langCombo->addItem(QStringLiteral("Bahasa Indonesia"), QStringLiteral("id"));
    m_langCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_langCombo->setCurrentIndex(qMax(0, m_langCombo->findData(i18n::currentCode())));
    connect(m_langCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { setLanguage(m_langCombo->currentData().toString()); });
    lay->addWidget(m_langCombo);

    return m_sidebar;
}

QWidget *MainWindow::buildTopbar()
{
    m_topbar = new QWidget;
    m_topbar->setObjectName(QStringLiteral("topbar"));
    m_topbar->setFixedHeight(66);

    auto *lay = new QHBoxLayout(m_topbar);
    lay->setContentsMargins(20, 10, 24, 10);
    lay->setSpacing(12);

    auto *hamburger = makeIconButton(icons::Kind::Menu, TR("Buka/tutup sidebar"));
    hamburger->setFixedSize(36, 36);
    connect(hamburger, &QPushButton::clicked, this, &MainWindow::toggleSidebar);
    lay->addWidget(hamburger);

    m_backBtn = makeIconButton(icons::Kind::Back, TR("Kembali"));
    m_backBtn->setFixedSize(36, 36);
    connect(m_backBtn, &QPushButton::clicked, this, &MainWindow::goBack);
    lay->addWidget(m_backBtn);

    m_titleLabel = makeLabel(TR("Beranda"), QString(), 16, true);
    lay->addWidget(m_titleLabel);

    lay->addStretch();

    m_statusLabel = makeLabel(QString(), QStringLiteral("sub"), 10);
    lay->addWidget(m_statusLabel);

    return m_topbar;
}

QWidget *MainWindow::buildPlayerBar()
{
    m_playerBar = new QWidget;
    m_playerBar->setObjectName(QStringLiteral("playerBar"));
    m_playerBar->setFixedHeight(96);

    auto *lay = new QHBoxLayout(m_playerBar);
    lay->setContentsMargins(20, 10, 20, 10);
    lay->setSpacing(18);

    // --- kiri: info lagu ---
    auto *left = new QWidget;
    auto *ll = new QHBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(12);
    m_npThumb = new ThumbLabel(56, left);
    ll->addWidget(m_npThumb);
    auto *info = new QVBoxLayout;
    info->setContentsMargins(0, 0, 0, 0);
    info->setSpacing(1);
    m_npTitle = makeLabel(TR("Belum ada lagu"), QStringLiteral("title"), 11, true);
    m_npArtist = new ClickableLabel(TR("Pilih lagu untuk mulai"));
    m_npArtist->setRole(QStringLiteral("sub"));
    m_npArtist->setFont(theme::font(9));
    m_npArtist->setCursor(Qt::PointingHandCursor);
    connect(m_npArtist, &ClickableLabel::clicked, this, [this]() {
        if (!m_nowPlaying.artistIds.isEmpty())
            openArtist(m_nowPlaying.artistIds.first());
    });
    // Label boleh menyusut mengikuti ruang yang ada; teks dipotong dengan "…".
    m_npTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_npArtist->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_npTitle->setMinimumWidth(0);
    m_npArtist->setMinimumWidth(0);
    m_npTitle->installEventFilter(this);
    m_npArtist->installEventFilter(this);
    m_npTitleFull = m_npTitle->text();
    m_npArtistFull = m_npArtist->text();
    info->addWidget(m_npTitle);
    info->addWidget(m_npArtist);
    ll->addLayout(info, 1);
    left->setMinimumWidth(110);
    left->setMaximumWidth(420);
    lay->addWidget(left, 3);

    // --- tengah: kontrol + seek ---
    auto *center = new QWidget;
    auto *cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);

    auto *controls = new QHBoxLayout;
    controls->setSpacing(6);
    controls->addStretch();
    m_shuffleBtn = makeIconButton(icons::Kind::Shuffle, TR("Acak"));
    m_prevBtn = makeIconButton(icons::Kind::Prev, TR("Sebelumnya"), icons::Role::Text);
    m_playBtn = new PlayButton(44);
    m_nextBtn = makeIconButton(icons::Kind::Next, TR("Berikutnya"), icons::Role::Text);
    m_repeatBtn = makeIconButton(icons::Kind::Repeat, TR("Ulangi satu lagu"));
    controls->addWidget(m_shuffleBtn);
    controls->addWidget(m_prevBtn);
    controls->addWidget(m_playBtn, 0, Qt::AlignVCenter);
    controls->addWidget(m_nextBtn);
    controls->addWidget(m_repeatBtn);
    controls->addStretch();
    cl->addLayout(controls);

    auto *seekRow = new QHBoxLayout;
    seekRow->setSpacing(8);
    m_posLabel = makeLabel(QStringLiteral("0:00"), QStringLiteral("dim"), 9);
    m_posLabel->setFixedWidth(48);
    m_posLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_seek = new ClickSlider(Qt::Horizontal); // klik di mana saja pada bar -> langsung lompat
    m_seek->setRange(0, 1000);
    m_seek->setCursor(Qt::PointingHandCursor);
    m_durLabel = makeLabel(QStringLiteral("0:00"), QStringLiteral("dim"), 9);
    m_durLabel->setFixedWidth(48);
    seekRow->addWidget(m_posLabel);
    seekRow->addWidget(m_seek, 1);
    seekRow->addWidget(m_durLabel);
    cl->addLayout(seekRow);

    lay->addWidget(center, 4);

    // --- kanan: like, lirik, perangkat, volume ---
    m_likeBtn = makeIconButton(icons::Kind::Heart, TR("Suka"));
    m_lyricsBtn = makeIconButton(icons::Kind::Mic, TR("Lirik"));
    m_lyricsBtn->setToolTip(TR("Lirik"));
    m_deviceBtn = makeIconButton(icons::Kind::Headphones, TR("Pilih perangkat audio"));
    m_testBtn = makeButton(TR("Uji"), QStringLiteral("ghostBtn"));
    m_testBtn->setToolTip(TR("Putar nada uji"));
    m_volume = new ClickSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(110);
    m_volume->setValue(m_volumeValue);
    m_volume->setToolTip(QStringLiteral("%1%").arg(m_volumeValue));
    m_volumePct = makeLabel(QStringLiteral("%1%").arg(m_volumeValue), QStringLiteral("dim"), 9);
    // Lebar label persen = lebar teks terpanjang ("100%") agar tidak pernah menumpuk slider.
    m_volumePct->setFixedWidth(m_volumePct->fontMetrics().horizontalAdvance(QStringLiteral("100%")) + 8);
    m_volumePct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *rightRow = new QHBoxLayout;
    rightRow->setContentsMargins(0, 0, 0, 0);
    rightRow->setSpacing(4);
    rightRow->addStretch();
    rightRow->addWidget(m_likeBtn);
    rightRow->addWidget(m_lyricsBtn);
    rightRow->addWidget(m_testBtn);
    rightRow->addWidget(m_deviceBtn);
    m_volIcon = new icons::IconWidget(icons::Kind::Volume, icons::Role::Sub, 18);
    rightRow->addWidget(m_volIcon);
    rightRow->addWidget(m_volume);
    rightRow->addWidget(m_volumePct);
    auto *rightWrap = new QWidget;
    rightWrap->setLayout(rightRow);
    lay->addWidget(rightWrap, 2);

    connect(m_deviceBtn, &QPushButton::clicked, this, &MainWindow::showAudioMenu);
    connect(m_testBtn, &QPushButton::clicked, this, &MainWindow::testAudio);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(m_prevBtn, &QPushButton::clicked, this, &MainWindow::previousTrack);
    connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::nextTrack);
    connect(m_shuffleBtn, &QPushButton::clicked, this, [this]() { setShuffle(!m_shuffle); });
    connect(m_repeatBtn, &QPushButton::clicked, this, [this]() { setRepeatOne(!m_repeatOne); });
    connect(m_likeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_nowPlaying.videoId.isEmpty())
            toggleLike(m_nowPlaying);
    });
    connect(m_lyricsBtn, &QPushButton::clicked, this, [this]() {
        m_lyricsOpen = !m_lyricsOpen;
        m_lyrics->setVisible(m_lyricsOpen);
        setActiveIcon(m_lyricsBtn, icons::Kind::Mic, m_lyricsOpen);
        m_lyricsBtn->setProperty("active", m_lyricsOpen);
        repolish(m_lyricsBtn);
        if (m_lyricsOpen) {
            if (!m_nowPlaying.videoId.isEmpty())
                m_lyrics->showFor(m_nowPlaying);
            else
                m_lyrics->clear();
        }
    });
    connect(m_volume, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    connect(m_seek, &QSlider::sliderPressed, this, &MainWindow::onSeekSliderPressed);
    connect(m_seek, &QSlider::sliderReleased, this, &MainWindow::onSeekSliderReleased);
    connect(m_seek, &QSlider::sliderMoved, this, [this](int v) {
        m_posLabel->setText(fmt(static_cast<qint64>(v)));
    });

    stylePlayerBar();
    return m_playerBar;
}

void MainWindow::stylePlayerBar()
{
    if (!m_playerBar)
        return;
    // QSS global sudah menata bar; hanya badge/aksen yang perlu disegarkan.
}

void MainWindow::buildLyricsPanel()
{
    m_lyrics = new LyricsView(this);
    m_lyrics->setFixedWidth(kLyricsWidth);
    m_lyrics->setVisible(false);
}

void MainWindow::applyThemeToApp()
{
    theme::apply();
    if (m_toast) {
        m_toast->setStyleSheet(
            QStringLiteral("QLabel#toast { background: %1; color: %2; border-radius: 10px; "
                           "padding: 10px 18px; border: 1px solid %3; }")
                .arg(theme::palette().card.name(), theme::palette().text.name(),
                     theme::palette().border.name()));
    }
    refreshThemeCombo();
}

// Isi ulang dropdown tema: tiap item diberi pratinjau warna (latar + aksen) dan item
// tema yang sedang dipakai diberi tanda centang, supaya jelas mana yang aktif.
void MainWindow::refreshThemeCombo()
{
    if (!m_themeCombo)
        return;
    QSignalBlocker block(m_themeCombo);
    const theme::Id ids[] = {theme::Id::Blackcurrant, theme::Id::Dark, theme::Id::Light};
    for (int i = 0; i < 3; ++i) {
        const theme::Id id = ids[i];
        const theme::Palette pal = theme::paletteOf(id);
        const bool isCurrent = (id == theme::current());

        QPixmap pm(34, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box(0.5, 0.5, 33, 19);
        p.setPen(QPen(isCurrent ? pal.accent : pal.border.lighter(160), isCurrent ? 2.0 : 1.0));
        p.setBrush(pal.bg);
        p.drawRoundedRect(box, 5, 5);
        p.setPen(Qt::NoPen);
        p.setBrush(pal.card);
        p.drawRoundedRect(QRectF(4, 4, 12, 12), 3, 3);
        p.setBrush(pal.accent);
        p.drawEllipse(QPointF(25, 10), 5, 5);
        p.end();

        m_themeCombo->setItemIcon(i, QIcon(pm));
        m_themeCombo->setItemText(i, isCurrent ? theme::labelOf(id) + TR("  (aktif)")
                                               : theme::labelOf(id));
    }
    const int idx = m_themeCombo->findData(theme::currentName());
    if (idx >= 0)
        m_themeCombo->setCurrentIndex(idx);
}

// ===========================================================================
// Navigasi
// ===========================================================================

void MainWindow::navigate(const QString &key, const QString &title, const QString &param)
{
    closeSidebar();
    if (!m_current.key.isEmpty())
        m_back.append(m_current);
    m_current.key = key;
    m_current.title = title;
    m_current.param = param;
    renderPage();
}

void MainWindow::goBack()
{
    closeSidebar();
    if (m_back.isEmpty())
        return;
    m_current = m_back.takeLast();
    renderPage();
}

void MainWindow::openAlbum(const QString &browseId)
{
    navigate(QStringLiteral("album"), QStringLiteral("Album"), browseId);
}

void MainWindow::openArtist(const QString &browseId)
{
    navigate(QStringLiteral("artist"), TR("Artis"), browseId);
}

void MainWindow::openLocalPlaylist(const QString &id)
{
    Playlist pl;
    const QString title = m_storage->getPlaylist(id, &pl) ? pl.name : QStringLiteral("Playlist");
    navigate(QStringLiteral("playlist"), title, id);
}

void MainWindow::openGenre(const QString &title, const QString &params)
{
    m_storage->noteGenre(title, params);
    navigate(QStringLiteral("genre"), title, params);
}

void MainWindow::renderPage()
{
    clearLayout(m_contentLayout);
    m_current.title = m_current.title.isEmpty() ? QString::fromUtf8(appinfo::kName)
                                                : m_current.title;
    m_titleLabel->setText(m_current.title);

    const QString &key = m_current.key;

    if (key == QLatin1String("home")) {
        auto *view = new HomeView(this, m_content);
        m_contentLayout->addWidget(view, 1);
    } else if (key == QLatin1String("search")) {
        auto *view = new SearchView(this, m_current.param, m_content);
        m_contentLayout->addWidget(view, 1);
    } else if (key == QLatin1String("library")) {
        m_contentLayout->addWidget(new LibraryView(this, m_content), 1);
    } else if (key == QLatin1String("liked")) {
        m_contentLayout->addWidget(new LikedView(this, m_content), 1);
    } else if (key == QLatin1String("liked_albums")) {
        m_contentLayout->addWidget(new LikedAlbumsView(this, m_content), 1);
    } else if (key == QLatin1String("history")) {
        m_contentLayout->addWidget(new HistoryView(this, m_content), 1);
    } else if (key == QLatin1String("about")) {
        m_contentLayout->addWidget(new AboutView(this, m_content), 1);
    } else if (key == QLatin1String("artist")) {
        m_contentLayout->addWidget(new ArtistView(this, m_current.param, m_content), 1);
    } else if (key == QLatin1String("playlist")) {
        renderLocalPlaylist(m_current.param);
    } else if (key == QLatin1String("album")) {
        renderAlbum(m_current.param);
    } else if (key == QLatin1String("genre")) {
        renderGenre(m_current.title, m_current.param);
    } else {
        m_contentLayout->addWidget(new HomeView(this, m_content), 1);
    }

    updateBackVisibility();
    updateNavActive();
    refreshRowStates();
    if (m_lyricsOpen && !m_nowPlaying.videoId.isEmpty())
        m_lyrics->showFor(m_nowPlaying);
}

void MainWindow::renderLocalPlaylist(const QString &id)
{
    Playlist pl;
    if (!m_storage->getPlaylist(id, &pl)) {
        m_contentLayout->addWidget(
            makeEmptyLabel(TR("Playlist tidak ditemukan.")), 1);
        return;
    }
    m_titleLabel->setText(pl.name);
    const QString cover = pl.tracks.isEmpty() ? QString() : pl.tracks.first().bigThumbnail();
    auto *view = new DetailView(this, pl.name,
                                TR("%1 lagu").arg(pl.tracks.size()), cover, pl.tracks,
                                DetailView::Kind::LocalPlaylist, id, m_content);
    m_contentLayout->addWidget(view, 1);
}

void MainWindow::renderAlbum(const QString &browseId)
{
    auto *loading = makeLoadingLabel(TR("Memuat album…"));
    m_contentLayout->addWidget(loading, 1);

    QJsonObject params;
    params[QStringLiteral("browse_id")] = browseId;
    m_bridge->request(
        QStringLiteral("album_tracks"), params,
        [this, browseId](bool ok, const QJsonValue &result, const QString &error) {
            if (m_current.key != QLatin1String("album") || m_current.param != browseId)
                return;
            clearLayout(m_contentLayout);
            if (!ok || !result.isObject()) {
                m_contentLayout->addWidget(
                    makeEmptyLabel(TR("Gagal memuat album: ") + error), 1);
                return;
            }
            const QJsonObject o = result.toObject();
            const QString title = o.value(QStringLiteral("title")).toString();
            QStringList artists;
            for (const QJsonValue &a : o.value(QStringLiteral("artists")).toArray())
                if (a.isString())
                    artists << a.toString();
            const QString cover = o.value(QStringLiteral("thumbnail")).toString();
            const QList<Track> tracks = Track::listFromJson(o.value(QStringLiteral("tracks")).toArray());

            m_titleLabel->setText(title.isEmpty() ? QStringLiteral("Album") : title);
            const QString subtitle =
                artists.isEmpty() ? QStringLiteral("Album") : artists.join(QStringLiteral(", "));
            auto *view = new DetailView(this, title.isEmpty() ? QStringLiteral("Album") : title,
                                        subtitle, cover, tracks, DetailView::Kind::Album, browseId,
                                        m_content);
            m_contentLayout->addWidget(view, 1);
            refreshRowStates();
        }, this);
}

void MainWindow::renderGenre(const QString &title, const QString &params)
{
    auto *loading = makeLoadingLabel(TR("Memuat lagu genre…"));
    m_contentLayout->addWidget(loading, 1);

    QJsonObject p;
    p[QStringLiteral("params")] = params;
    p[QStringLiteral("fallback_query")] = title;
    m_bridge->request(
        QStringLiteral("genre_tracks"), p,
        [this, title, params](bool ok, const QJsonValue &result, const QString &error) {
            if (m_current.key != QLatin1String("genre") || m_current.param != params
                || m_current.title != title)
                return;
            clearLayout(m_contentLayout);
            if (!ok) {
                m_contentLayout->addWidget(
                    makeEmptyLabel(TR("Gagal memuat genre: ") + error), 1);
                return;
            }
            const QList<Track> tracks = Track::listFromJson(result.toArray());
            m_storage->noteGenreTracks(title, tracks); // pelajari artis -> genre
            const QString cover = tracks.isEmpty() ? QString() : tracks.first().bigThumbnail();
            auto *view = new DetailView(this, title, TR("%1 lagu").arg(tracks.size()),
                                        cover, tracks, DetailView::Kind::Generic, QString(),
                                        m_content);
            m_contentLayout->addWidget(view, 1);
            refreshRowStates();
        }, this);
}

void MainWindow::updateNavActive()
{
    for (const auto &pair : m_navButtons) {
        QPushButton *btn = pair.second;
        const bool active = (pair.first == m_current.key);
        btn->setProperty("active", active);
        const QVariant kind = btn->property("iconKind");
        if (kind.isValid())
            btn->setIcon(icons::icon(static_cast<icons::Kind>(kind.toInt()),
                                     active ? icons::Role::Text : icons::Role::Sub));
        repolish(btn);
    }
}

void MainWindow::updateBackVisibility()
{
    const bool show = m_current.key != QLatin1String("home");
    m_backBtn->setVisible(show);
}

// ===========================================================================
// Pemutaran
// ===========================================================================

void MainWindow::playContext(const QList<Track> &tracks, int index)
{
    if (tracks.isEmpty())
        return;
    m_queue = tracks;
    m_queueIndex = qBound(0, index, tracks.size() - 1);
    m_contextGenre = (m_current.key == QLatin1String("genre")) ? m_current.title : QString();

    if (m_shuffle) {
        m_shuffleOrder.clear();
        for (int i = 0; i < tracks.size(); ++i)
            if (i != m_queueIndex)
                m_shuffleOrder << i;
        // Acak sederhana.
        for (int i = m_shuffleOrder.size() - 1; i > 0; --i)
            std::swap(m_shuffleOrder[i], m_shuffleOrder[QRandomGenerator::global()->bounded(i + 1)]);
        m_shuffleOrder.prepend(m_queueIndex);
        m_shufflePos = 0;
    }

    startCurrent();
}

void MainWindow::startCurrent()
{
    if (m_queueIndex < 0 || m_queueIndex >= m_queue.size())
        return;
    const Track track = m_queue.at(m_queueIndex);
    setNowPlaying(track);
    m_playBtn->setPlaying(true);
    m_player->playTrack(track);
}

void MainWindow::setNowPlaying(const Track &track)
{
    m_nowPlaying = track;
    m_npTitleFull = track.title;
    m_npArtistFull = track.artistStr();
    m_npTitle->setToolTip(track.title);
    elideNowPlaying();
    m_npThumb->setUrl(track.bigThumbnail(), true);
    refreshPlayerLike();
    if (m_lyricsOpen)
        m_lyrics->showFor(track);
    refreshRowStates();
}

void MainWindow::playTrack(const Track &track)
{
    playContext(QList<Track>{track}, 0);
}

void MainWindow::enqueueNext(const Track &track)
{
    const int insertAt = (m_queueIndex >= 0 && m_queueIndex + 1 <= m_queue.size())
                             ? m_queueIndex + 1
                             : m_queue.size();
    m_queue.insert(insertAt, track);
    toast(TR("Ditambahkan ke antrean"));
}

void MainWindow::clearQueue()
{
    m_queue.clear();
    m_queueIndex = -1;
}

void MainWindow::togglePlay()
{
    if (!m_player->hasTrack()) {
        if (!m_queue.isEmpty())
            startCurrent();
        return;
    }
    m_player->toggle();
}

void MainWindow::nextTrack()
{
    if (m_queue.isEmpty())
        return;
    if (m_shuffle) {
        if (m_shufflePos + 1 < m_shuffleOrder.size()) {
            ++m_shufflePos;
            m_queueIndex = m_shuffleOrder.at(m_shufflePos);
            startCurrent();
            return;
        }
        // Bangun ulang urutan acak bila sudah habis.
        m_shuffleOrder.clear();
        for (int i = 0; i < m_queue.size(); ++i)
            if (i != m_queueIndex)
                m_shuffleOrder << i;
        for (int i = m_shuffleOrder.size() - 1; i > 0; --i)
            std::swap(m_shuffleOrder[i], m_shuffleOrder[QRandomGenerator::global()->bounded(i + 1)]);
        m_shuffleOrder.prepend(m_queueIndex);
        m_shufflePos = 0;
    }
    if (m_queueIndex + 1 < m_queue.size()) {
        ++m_queueIndex;
        startCurrent();
    } else {
        m_player->stop();
        m_queueIndex = -1;
        refreshPlayerBar();
    }
}

void MainWindow::previousTrack()
{
    if (m_queue.isEmpty())
        return;
    if (m_player->position() > 4000) {
        m_player->seekTo(0);
        return;
    }
    if (m_queueIndex > 0) {
        --m_queueIndex;
        startCurrent();
    }
}

void MainWindow::setShuffle(bool on)
{
    m_shuffle = on;
    setActiveIcon(m_shuffleBtn, icons::Kind::Shuffle, on);
    m_shuffleBtn->setProperty("active", on);
    repolish(m_shuffleBtn);
    toast(on ? TR("Mode acak aktif") : TR("Mode acak nonaktif"));
}

void MainWindow::setRepeatOne(bool on)
{
    m_repeatOne = on;
    setActiveIcon(m_repeatBtn, icons::Kind::Repeat, on);
    m_repeatBtn->setProperty("active", on);
    repolish(m_repeatBtn);
    toast(on ? TR("Ulangi satu lagu aktif") : TR("Ulangi satu lagu mati"));
}

void MainWindow::onPlayerStarted(const Track &track)
{
    recordPlayback(track);
    refreshPlayerBar();
    refreshRowStates();
}

// Catat lagu yang benar-benar mulai diputar: riwayat, hitungan putar, serta profil
// selera (artis & genre) yang dipakai beranda untuk pilihan cepat dan rekomendasi.
void MainWindow::recordPlayback(const Track &track)
{
    if (!track.isValid())
        return;
    m_storage->addHistory(track);
    m_storage->incrementPlaycount(track.videoId);
    m_storage->recordListen(track, m_contextGenre);

    // Bila beranda sedang terbuka, perbarui daftar "Terakhir Diputar" saat itu juga.
    if (m_current.key == QLatin1String("home") && m_contentLayout->count() > 0) {
        if (QLayoutItem *item = m_contentLayout->itemAt(0)) {
            if (auto *home = qobject_cast<HomeView *>(item->widget()))
                home->refreshHistory();
        }
    }
}

void MainWindow::onPlayerFinished()
{
    if (m_repeatOne) {
        startCurrent();
        return;
    }
    nextTrack();
}

void MainWindow::onPlayerFailed(const QString &message)
{
    m_statusLabel->setText(QString());
    refreshPlayerBar();
    toast(message);
}

void MainWindow::onPositionChanged(qint64 pos, qint64 dur)
{
    if (!m_seekDragging) {
        if (dur > 0 && m_seek->maximum() != static_cast<int>(dur)) {
            m_seek->setRange(0, static_cast<int>(dur));
        }
        m_seek->setValue(static_cast<int>(pos));
        m_posLabel->setText(fmt(pos));
    }
    if (dur > 0)
        m_durLabel->setText(fmt(dur));
}

void MainWindow::onResolvingChanged(bool resolving)
{
    m_statusLabel->setText(resolving ? TR("Memuat audio…") : QString());
    if (resolving)
        refreshPlayerBar();
}

void MainWindow::onSeekSliderPressed() { m_seekDragging = true; }

void MainWindow::onSeekSliderReleased()
{
    m_seekDragging = false;
    m_player->seekTo(m_seek->value());
}

void MainWindow::refreshPlayerBar()
{
    const bool playing = m_player->isPlaying() || m_player->isResolving();
    m_playBtn->setPlaying(playing);
    if (m_repeatOne) {
        setActiveIcon(m_repeatBtn, icons::Kind::Repeat, true);
        m_repeatBtn->setProperty("active", true);
        repolish(m_repeatBtn);
    }
    if (m_shuffle) {
        setActiveIcon(m_shuffleBtn, icons::Kind::Shuffle, true);
        m_shuffleBtn->setProperty("active", true);
        repolish(m_shuffleBtn);
    }
}

void MainWindow::refreshPlayerLike()
{
    const bool liked = !m_nowPlaying.videoId.isEmpty() && m_storage->isLiked(m_nowPlaying.videoId);
    setLikeIcon(m_likeBtn, liked);
    m_likeBtn->setProperty("active", liked);
    repolish(m_likeBtn);
}

void MainWindow::refreshRowStates()
{
    if (!m_content)
        return;
    for (SongRow *row : m_content->findChildren<SongRow *>()) {
        row->setActive(!m_nowPlaying.videoId.isEmpty()
                       && row->track().videoId == m_nowPlaying.videoId);
        row->setLiked(m_storage->isLiked(row->track().videoId));
    }
}

void MainWindow::refreshLikeButtons() { refreshRowStates(); refreshPlayerLike(); }

// ===========================================================================
// Suara
// ===========================================================================

void MainWindow::onVolumeChanged(int value)
{
    m_volumeValue = value;
    m_volumePct->setText(QStringLiteral("%1%").arg(value));
    m_volume->setToolTip(QStringLiteral("%1%").arg(value));
    m_player->setVolume(value);
    saveVolume();
}

void MainWindow::restoreVolume()
{
    const int vol = m_storage->setting(QStringLiteral("volume"), QJsonValue(80)).toInt(80);
    m_volumeValue = qBound(0, vol, 100);
    m_player->setVolume(m_volumeValue);
}

void MainWindow::saveVolume()
{
    m_storage->setSetting(QStringLiteral("volume"), m_volumeValue);
}

void MainWindow::restoreWindowState()
{
    const QString geo = m_storage->setting(QStringLiteral("window_geometry")).toString();
    bool restored = false;
    if (!geo.isEmpty()) {
        static const QRegularExpression re(
            QStringLiteral("^(\\d+)x(\\d+)(?:([+-]\\d+)([+-]\\d+))?$"));
        const QRegularExpressionMatch m = re.match(geo);
        if (m.hasMatch()) {
            const int w = m.captured(1).toInt();
            const int h = m.captured(2).toInt();
            if (w > 400 && h > 300) {
                resize(w, h);
                if (!m.captured(3).isEmpty())
                    move(m.captured(3).toInt(), m.captured(4).toInt());
                restored = true;
            }
        }
    }
    if (!restored)
        resize(1240, 800);
    if (m_storage->setting(QStringLiteral("window_maximized"), QJsonValue(false)).toBool())
        showMaximized();
}

void MainWindow::saveWindowState()
{
    const bool maximized = isMaximized();
    m_storage->setSetting(QStringLiteral("window_maximized"), maximized);
    if (!maximized) {
        m_storage->setSetting(QStringLiteral("window_geometry"),
                              QStringLiteral("%1x%2+%3+%4")
                                  .arg(width())
                                  .arg(height())
                                  .arg(x())
                                  .arg(y()));
    }
}

int MainWindow::volume() const { return m_volumeValue; }

void MainWindow::setVolume(int percent)
{
    const int v = qBound(0, percent, 100);
    if (m_volume)
        m_volume->setValue(v);
    else
        onVolumeChanged(v);
}

// ===========================================================================
// Suka & menu
// ===========================================================================

bool MainWindow::isLiked(const QString &videoId) { return m_storage->isLiked(videoId); }

bool MainWindow::toggleLike(const Track &track)
{
    const bool liked = m_storage->toggleLike(track);
    refreshLikeButtons();
    toast(liked ? TR("Ditambahkan ke Lagu Disukai")
                : TR("Dihapus dari Lagu Disukai"));
    return liked;
}

void MainWindow::showTrackMenu(const Track &track, const QPoint &globalPos,
                               const QString &removeFromPlaylistId)
{
    if (track.videoId.isEmpty())
        return;

    QMenu menu;
    const bool liked = m_storage->isLiked(track.videoId);

    QAction *playAct = menu.addAction(icons::icon(icons::Kind::Play, icons::Role::Text), TR("Putar sekarang"));
    QAction *queueAct = menu.addAction(icons::icon(icons::Kind::QueueNext, icons::Role::Text), TR("Putar berikutnya"));
    menu.addSeparator();
    QAction *likeAct = menu.addAction(
        icons::icon(liked ? icons::Kind::HeartFilled : icons::Kind::Heart, icons::Role::Text),
        liked ? TR("Hapus dari Lagu Disukai")
              : TR("Tambahkan ke Lagu Disukai"));

    QMenu *plMenu = menu.addMenu(icons::icon(icons::Kind::Library, icons::Role::Text), TR("Tambahkan ke playlist"));
    const QList<Playlist> playlists = m_storage->playlists();
    for (const Playlist &p : playlists) {
        QAction *act = plMenu->addAction(p.name);
        const QString pid = p.id;
        connect(act, &QAction::triggered, this,
                [this, pid, track]() { addTrackToPlaylist(pid, track); });
    }
    if (!playlists.isEmpty())
        plMenu->addSeparator();
    QAction *newPlAct = plMenu->addAction(icons::icon(icons::Kind::Plus, icons::Role::Text), TR("Playlist baru…"));
    connect(newPlAct, &QAction::triggered, this, [this, track]() {
        addTrackToPlaylistDialog(track);
    });

    if (!track.artistIds.isEmpty()) {
        menu.addSeparator();
        QAction *artistAct = menu.addAction(icons::icon(icons::Kind::Artist, icons::Role::Text), TR("Buka artis"));
        const QString aid = track.artistIds.first();
        connect(artistAct, &QAction::triggered, this, [this, aid]() { openArtist(aid); });
    }

    if (!removeFromPlaylistId.isEmpty()) {
        menu.addSeparator();
        QAction *removeAct = menu.addAction(icons::icon(icons::Kind::Trash, icons::Role::Text), TR("Hapus dari playlist ini"));
        const QString pid = removeFromPlaylistId;
        connect(removeAct, &QAction::triggered, this,
                [this, pid, track]() { removeFromPlaylist(pid, track); });
    }

    QAction *chosen = menu.exec(globalPos);
    if (chosen == playAct) {
        playTrack(track);
    } else if (chosen == queueAct) {
        enqueueNext(track);
    } else if (chosen == likeAct) {
        toggleLike(track);
    }
}

// ===========================================================================
// Playlist
// ===========================================================================

void MainWindow::createPlaylistDialog()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, TR("Playlist baru"),
                                               TR("Nama playlist:"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    const Playlist pl = m_storage->createPlaylist(name);
    toast(TR("Playlist \"%1\" dibuat").arg(pl.name));
    refreshSidebarPlaylists();
    if (m_current.key == QLatin1String("library"))
        renderPage();
    else
        openLocalPlaylist(pl.id);
}

void MainWindow::addTrackToPlaylistDialog(const Track &track)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, TR("Playlist baru"),
                                               TR("Nama playlist:"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    const Playlist pl = m_storage->createPlaylist(name);
    refreshSidebarPlaylists();
    if (m_storage->addToPlaylist(pl.id, track))
        toast(TR("Ditambahkan ke \"%1\"").arg(pl.name));
}

void MainWindow::addTrackToPlaylist(const QString &playlistId, const Track &track)
{
    Playlist pl;
    if (!m_storage->getPlaylist(playlistId, &pl))
        return;
    if (m_storage->addToPlaylist(playlistId, track))
        toast(TR("Ditambahkan ke \"%1\"").arg(pl.name));
    else
        toast(TR("Lagu sudah ada di \"%1\"").arg(pl.name));
}

void MainWindow::removeFromPlaylist(const QString &playlistId, const Track &track)
{
    if (m_storage->removeFromPlaylist(playlistId, track.videoId)) {
        toast(TR("Dihapus dari playlist"));
        if (m_current.key == QLatin1String("playlist") && m_current.param == playlistId)
            renderPage();
    }
}

// ===========================================================================
// Tema & toast
// ===========================================================================

void MainWindow::setTheme(const QString &name)
{
    if (theme::currentName() == name)
        return;
    theme::setCurrent(theme::idFromName(name));
    m_storage->setSetting(QStringLiteral("theme"), theme::currentName());
    applyThemeToApp();
    renderPage();
    toast(TR("Tema: %1").arg(theme::labelOf(theme::current())));
}

// Ganti bahasa UI. Judul halaman yang tersimpan diterjemahkan ulang, lalu seluruh UI
// dibangun ulang (ditunda ke putaran event berikutnya karena pemicunya berada di
// dalam widget yang akan diganti).
void MainWindow::setLanguage(const QString &code)
{
    const i18n::Lang lang = i18n::fromCode(code);
    if (lang == i18n::current())
        return;
    i18n::setLanguage(lang);
    m_storage->setSetting(QStringLiteral("language"), i18n::currentCode());

    auto retitle = [](Page &p) {
        const QString &k = p.key;
        if (k == QLatin1String("home"))
            p.title = TR("Beranda");
        else if (k == QLatin1String("search"))
            p.title = TR("Pencarian");
        else if (k == QLatin1String("library"))
            p.title = TR("Koleksi");
        else if (k == QLatin1String("liked"))
            p.title = TR("Lagu Disukai");
        else if (k == QLatin1String("liked_albums"))
            p.title = TR("Album Disukai");
        else if (k == QLatin1String("history"))
            p.title = TR("Riwayat Musik");
        else if (k == QLatin1String("about"))
            p.title = TR("Tentang");
        else if (k == QLatin1String("artist"))
            p.title = TR("Artis");
        // playlist & genre: judulnya nama dari pengguna/YouTube Music, biarkan.
    };
    retitle(m_current);
    for (Page &p : m_back)
        retitle(p);

    QTimer::singleShot(0, this, &MainWindow::rebuildUi);
}

// Bangun ulang seluruh antarmuka (dipakai saat bahasa berganti). Pemutaran audio,
// antrean, dan riwayat navigasi tidak tersentuh; hanya widget yang dibuat ulang.
void MainWindow::rebuildUi()
{
    // Objek anak lama yang menunjuk ke widget lama.
    if (m_sidebarAnim) {
        m_sidebarAnim->stop();
        delete m_sidebarAnim;
        m_sidebarAnim = nullptr;
    }
    if (m_toastTimer) {
        delete m_toastTimer;
        m_toastTimer = nullptr;
    }
    m_navButtons.clear();
    m_toast = nullptr;
    m_sidebarOpen = false;
    m_responsiveTier = -1;

    buildUi(); // membuat central widget baru; yang lama dihapus oleh QMainWindow

    applyThemeToApp();
    refreshSidebarPlaylists();
    refreshAudioStatus();

    if (m_lyricsOpen) {
        m_lyrics->setVisible(true);
        setActiveIcon(m_lyricsBtn, icons::Kind::Mic, true);
        m_lyricsBtn->setProperty("active", true);
        repolish(m_lyricsBtn);
    }

    renderPage();

    if (!m_nowPlaying.videoId.isEmpty())
        setNowPlaying(m_nowPlaying);
    refreshPlayerBar();
    onPositionChanged(m_player->position(), m_player->duration());
    onResolvingChanged(m_player->isResolving());
    applyResponsiveLayout();
}

QString MainWindow::themeName() const { return theme::currentName(); }

void MainWindow::applyThemeAndRefresh()
{
    applyThemeToApp();
    renderPage();
}

void MainWindow::toast(const QString &text) { showToastMessage(text); }

void MainWindow::showToastMessage(const QString &text)
{
    if (!m_toast)
        return;
    m_toast->setText(text);
    m_toast->adjustSize();
    const int x = qMax(0, (m_central->width() - m_toast->width()) / 2);
    const int y = qMax(0, m_central->height() - m_toast->height() - 120);
    m_toast->move(x, y);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start(2600);
}

QString MainWindow::currentVideoId() const { return m_nowPlaying.videoId; }

bool MainWindow::isPlaying() const { return m_player->isPlaying(); }

// ===========================================================================
// Sidebar & perangkat audio
// ===========================================================================

void MainWindow::toggleSidebar()
{
    if (m_sidebarOpen)
        closeSidebar();
    else
        openSidebar();
}

void MainWindow::layoutOverlay()
{
    if (!m_central || !m_sidebar || !m_scrim)
        return;
    m_scrim->setGeometry(0, 0, m_central->width(), m_central->height());
    if (m_sidebarAnim && m_sidebarAnim->state() == QAbstractAnimation::Running)
        m_sidebar->resize(kSidebarWidth, m_central->height());
    else
        m_sidebar->setGeometry(m_sidebarOpen ? 0 : -kSidebarWidth, 0, kSidebarWidth,
                               m_central->height());
}

void MainWindow::openSidebar()
{
    if (m_sidebarOpen)
        return;
    m_sidebarOpen = true;
    m_sidebarAnim->stop();
    m_scrim->setGeometry(0, 0, m_central->width(), m_central->height());
    m_scrim->show();
    m_scrim->raise();
    m_sidebar->setGeometry(-kSidebarWidth, 0, kSidebarWidth, m_central->height());
    m_sidebar->show();
    m_sidebar->raise();
    m_sidebarAnim->setStartValue(QPoint(-kSidebarWidth, 0));
    m_sidebarAnim->setEndValue(QPoint(0, 0));
    m_sidebarAnim->start();
}

void MainWindow::closeSidebar()
{
    if (!m_sidebarOpen)
        return;
    m_sidebarOpen = false;
    m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebar->pos());
    m_sidebarAnim->setEndValue(QPoint(-kSidebarWidth, 0));
    m_sidebarAnim->start();
}

void MainWindow::elideNowPlaying()
{
    if (!m_npTitle || !m_npArtist)
        return;
    const int tw = qMax(20, m_npTitle->width());
    const int aw = qMax(20, m_npArtist->width());
    m_npTitle->setText(m_npTitle->fontMetrics().elidedText(m_npTitleFull, Qt::ElideRight, tw));
    m_npArtist->setText(m_npArtist->fontMetrics().elidedText(m_npArtistFull, Qt::ElideRight, aw));
    m_npArtist->setToolTip(m_npArtistFull);
}

// Bilah pemutar & panel lirik menyesuaikan bentuk/ukuran ikon dengan lebar window.
// Empat tingkat: penuh, sedang, ringkas, mungil. Elemen yang tidak muat disembunyikan
// (bukan ditumpuk); persen volume tetap terlihat lewat tooltip slider.
void MainWindow::applyResponsiveLayout()
{
    if (!m_central || !m_playerBar || !m_playBtn || !m_volume)
        return;
    const int w = m_central->width();

    // Panel lirik: proporsional terhadap lebar window.
    if (m_lyrics) {
        const int lw = qBound(240, w * 3 / 10, kLyricsWidth);
        if (m_lyrics->width() != lw || m_lyrics->minimumWidth() != lw)
            m_lyrics->setFixedWidth(lw);
    }

    const int tier = w >= 1100 ? 0 : (w >= 860 ? 1 : (w >= 700 ? 2 : 3));
    if (tier == m_responsiveTier)
        return;
    m_responsiveTier = tier;

    struct Tier {
        int barH, thumb, play, icon, vol;
        bool test, pct, device, shuffleRepeat, volIcon;
    };
    static const Tier tiers[4] = {
        {96, 56, 44, 20, 110, true,  true,  true,  true,  true},
        {88, 50, 40, 18,  96, false, true,  true,  true,  true},
        {80, 44, 36, 18,  80, false, false, true,  true,  true},
        {72, 40, 34, 16,  56, false, false, false, false, false},
    };
    const Tier &t = tiers[tier];

    m_playerBar->setFixedHeight(t.barH);
    m_npThumb->setSize(t.thumb);
    m_playBtn->setFixedSize(t.play, t.play);
    const QSize iconSize(t.icon, t.icon);
    for (QPushButton *b : {m_shuffleBtn, m_prevBtn, m_nextBtn, m_repeatBtn, m_likeBtn, m_lyricsBtn,
                           m_deviceBtn})
        b->setIconSize(iconSize);
    m_testBtn->setVisible(t.test);
    m_deviceBtn->setVisible(t.device);
    m_shuffleBtn->setVisible(t.shuffleRepeat);
    m_repeatBtn->setVisible(t.shuffleRepeat);
    m_volIcon->setVisible(t.volIcon);
    m_volumePct->setVisible(t.pct);
    m_volume->setFixedWidth(t.vol);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_central && event->type() == QEvent::Resize) {
        layoutOverlay();
        applyResponsiveLayout();
    } else if ((watched == m_npTitle || watched == m_npArtist) && event->type() == QEvent::Resize) {
        elideNowPlaying();
    } else if (watched == m_scrim && event->type() == QEvent::MouseButtonPress) {
        closeSidebar();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::refreshSidebarPlaylists()
{
    if (!m_playlistList)
        return;
    auto *lay = qobject_cast<QVBoxLayout *>(m_playlistList->layout());
    if (!lay)
        return;
    clearLayout(lay);

    const QList<Playlist> lists = m_storage->playlists();
    if (lists.isEmpty()) {
        lay->addWidget(makeLabel(TR("Belum ada playlist"), QStringLiteral("dim"), 9));
        return;
    }
    for (const Playlist &p : lists) {
        auto *btn = new QPushButton(QStringLiteral("  %1").arg(p.name));
        btn->setIcon(icons::icon(icons::Kind::Note, icons::Role::Sub));
        btn->setIconSize(QSize(16, 16));
        btn->setObjectName(QStringLiteral("navItem"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFlat(true);
        btn->setToolTip(p.name);
        const QString id = p.id;
        connect(btn, &QPushButton::clicked, this, [this, id]() { openLocalPlaylist(id); });
        lay->addWidget(btn);
    }
}

void MainWindow::refreshAudioStatus()
{
    if (!m_audioStatus)
        return;
    if (!m_player->hasAudioOutput()) {
        m_audioStatus->setText(TR("Tidak ada perangkat audio"));
        m_audioStatus->setProperty("role", QStringLiteral("accent"));
        repolish(m_audioStatus);
        return;
    }
    m_audioStatus->setText(QStringLiteral("Audio: %1").arg(m_player->currentOutputName()));
    m_audioStatus->setProperty("role", QStringLiteral("accent"));
    repolish(m_audioStatus);
}

void MainWindow::testAudio()
{
    if (!m_player->hasAudioOutput()) {
        toast(TR("Tidak ada perangkat keluaran audio terdeteksi"));
        return;
    }
    m_player->playTestTone();
    toast(TR("Memutar nada uji…"));
}

void MainWindow::showAudioMenu()
{
    QMenu menu;
    const QList<QAudioDevice> devices = m_player->outputDevices();
    if (devices.isEmpty()) {
        QAction *none = menu.addAction(TR("Tidak ada perangkat ditemukan"));
        none->setEnabled(false);
    }
    const QString current = m_player->currentOutputName();
    for (const QAudioDevice &dev : devices) {
        const bool isCurrent = (dev.description() == current);
        QAction *act = menu.addAction(
            isCurrent ? icons::icon(icons::Kind::Check, icons::Role::Accent) : QIcon(),
            dev.description());
        connect(act, &QAction::triggered, this, [this, dev]() {
            m_player->setOutputDevice(dev);
            refreshAudioStatus();
            toast(TR("Perangkat audio: %1").arg(dev.description()));
        });
    }
    menu.addSeparator();
    QAction *testAct = menu.addAction(icons::icon(icons::Kind::Volume, icons::Role::Text), TR("Uji suara"));
    connect(testAct, &QAction::triggered, this, &MainWindow::testAudio);
    menu.exec(QCursor::pos());
}

// ===========================================================================
// Peristiwa
// ===========================================================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveVolume();
    m_storage->setSetting(QStringLiteral("theme"), theme::currentName());
    saveWindowState();
    if (m_player)
        m_player->stop();
    if (m_bridge)
        m_bridge->shutdown();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_sidebarOpen) {
        closeSidebar();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        // Abaikan bila fokus sedang di kotak teks.
        QWidget *focus = QApplication::focusWidget();
        const QString cls = focus ? QString::fromLatin1(focus->metaObject()->className()) : QString();
        if (!cls.contains(QLatin1String("QLineEdit"))) {
            togglePlay();
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape && m_backBtn && m_backBtn->isVisible()) {
        goBack();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace bc
