#pragma once

#include "i18n.h"

#include "track.h"

#include <QList>
#include <QMainWindow>
#include <QPoint>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QVBoxLayout;
class QStackedWidget;
class QComboBox;
class QPropertyAnimation;
class QEvent;

namespace bc {

class Storage;
class Bridge;
class AudioPlayer;
class ThumbLabel;
class SongRow;
class LyricsView;
class ClickableLabel;
class PlayButton;

struct Page {
    QString key;
    QString title;
    QString param;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // ---- akses layanan ----
    Storage *storage() { return m_storage; }
    Bridge *bridge() { return m_bridge; }
    AudioPlayer *player() { return m_player; }

    // ---- navigasi ----
    void navigate(const QString &key, const QString &title, const QString &param = QString());
    void goBack();
    void showHome() { navigate(QStringLiteral("home"), TR("Beranda")); }
    void showSearch() { navigate(QStringLiteral("search"), TR("Pencarian")); }
    void showLibrary() { navigate(QStringLiteral("library"), TR("Koleksi")); }
    void showLiked() { navigate(QStringLiteral("liked"), TR("Lagu Disukai")); }
    void showLikedAlbums() { navigate(QStringLiteral("liked_albums"), TR("Album Disukai")); }
    void showHistory() { navigate(QStringLiteral("history"), TR("Riwayat Musik")); }
    void showAbout() { navigate(QStringLiteral("about"), TR("Tentang")); }

    void openAlbum(const QString &browseId);
    void openArtist(const QString &browseId);
    void openLocalPlaylist(const QString &id);
    void openGenre(const QString &title, const QString &params);

    // ---- pemutaran ----
    void playContext(const QList<Track> &tracks, int index);
    void playTrack(const Track &track);
    void enqueueNext(const Track &track);
    void togglePlay();
    void nextTrack();
    void previousTrack();
    void setShuffle(bool on);
    void setRepeatOne(bool on);
    bool shuffle() const { return m_shuffle; }
    bool repeatOne() const { return m_repeatOne; }
    void clearQueue();

    // ---- suka ----
    bool isLiked(const QString &videoId);
    bool toggleLike(const Track &track);
    void showTrackMenu(const Track &track, const QPoint &globalPos,
                       const QString &removeFromPlaylistId = QString());

    // ---- playlist ----
    void createPlaylistDialog();
    void addTrackToPlaylistDialog(const Track &track);
    void addTrackToPlaylist(const QString &playlistId, const Track &track);
    void removeFromPlaylist(const QString &playlistId, const Track &track);

    // ---- tema & UI ----
    void setTheme(const QString &name);
    void setLanguage(const QString &code); // "id" atau "en"
    QString themeName() const;
    void applyThemeAndRefresh();
    void toast(const QString &text);
    void refreshPlayerBar();
    void refreshLikeButtons();
    void refreshSidebarPlaylists();

    // dipakai lirik
    QString currentVideoId() const;
    bool isPlaying() const;

    int volume() const;
    void setVolume(int percent);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onPlayerStarted(const Track &track);
    void onPlayerFinished();
    void onPlayerFailed(const QString &message);
    void onPositionChanged(qint64 pos, qint64 dur);
    void onResolvingChanged(bool resolving);
    void onVolumeChanged(int value);
    void onSeekSliderPressed();
    void onSeekSliderReleased();

private:
    void buildUi();
    QWidget *buildSidebar();
    QWidget *buildTopbar();
    QWidget *buildPlayerBar();
    void buildLyricsPanel();
    void renderPage();
    void renderLocalPlaylist(const QString &id);
    void renderAlbum(const QString &browseId);
    void renderGenre(const QString &title, const QString &params);
    void updateNavActive();
    void updateBackVisibility();
    void refreshRowStates();
    void stylePlayerBar();
    void applyResponsiveLayout();
    void rebuildUi();
    void elideNowPlaying();
    void restoreVolume();
    void saveVolume();
    void restoreWindowState();
    void saveWindowState();
    void startCurrent();
    void setNowPlaying(const Track &track);
    void refreshPlayerLike();
    void showToastMessage(const QString &text);
    void applyThemeToApp();
    void refreshThemeCombo();
    void toggleSidebar();
    void openSidebar();
    void closeSidebar();
    void layoutOverlay();
    void recordPlayback(const Track &track);
    void showAudioMenu();
    void testAudio();
    void refreshAudioStatus();

    Storage *m_storage = nullptr;
    Bridge *m_bridge = nullptr;
    AudioPlayer *m_player = nullptr;

    // navigasi
    QList<Page> m_back;
    Page m_current;

    // tata letak
    QWidget *m_central = nullptr;
    QWidget *m_sidebar = nullptr;
    QWidget *m_scrim = nullptr;            // lapisan gelap di belakang sidebar
    QPropertyAnimation *m_sidebarAnim = nullptr;
    bool m_sidebarOpen = false;
    QWidget *m_topbar = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QPushButton *m_backBtn = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QList<QPair<QString, QPushButton *>> m_navButtons;
    QWidget *m_playlistList = nullptr;
    QLabel *m_audioStatus = nullptr;
    QLabel *m_netStatus = nullptr;
    LyricsView *m_lyrics = nullptr;
    bool m_lyricsOpen = false;

    // bar pemutar
    QWidget *m_playerBar = nullptr;
    ThumbLabel *m_npThumb = nullptr;
    QLabel *m_npTitle = nullptr;
    ClickableLabel *m_npArtist = nullptr;
    PlayButton *m_playBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_shuffleBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QPushButton *m_likeBtn = nullptr;
    QPushButton *m_lyricsBtn = nullptr;
    QPushButton *m_deviceBtn = nullptr;
    QPushButton *m_testBtn = nullptr;
    QLabel *m_posLabel = nullptr;
    QLabel *m_durLabel = nullptr;
    QSlider *m_seek = nullptr;
    QSlider *m_volume = nullptr;
    QWidget *m_volIcon = nullptr;
    QString m_npTitleFull;
    QString m_npArtistFull;
    int m_responsiveTier = -1;
    QLabel *m_volumePct = nullptr;
    bool m_seekDragging = false;

    // antrean
    QList<Track> m_queue;
    int m_queueIndex = -1;
    Track m_nowPlaying;
    QString m_contextGenre; // genre dari halaman tempat antrean dimulai (untuk profil selera)
    bool m_shuffle = false;
    bool m_repeatOne = false;
    QList<int> m_shuffleOrder;
    int m_shufflePos = 0;

    QLabel *m_toast = nullptr;
    class QTimer *m_toastTimer = nullptr;
    int m_volumeValue = 80;
};

} // namespace bc
