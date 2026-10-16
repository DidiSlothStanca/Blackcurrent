#pragma once

#include "track.h"

#include <QWidget>

class QTimer;
class QVBoxLayout;

namespace bc {

class MainWindow;
class SearchBar;
class ScrollArea;

class HomeView : public QWidget
{
    Q_OBJECT
public:
    explicit HomeView(MainWindow *app, QWidget *parent = nullptr);

    void refresh();
    // Segarkan hanya bagian "Terakhir Diputar" dan "Riwayat Musik" (dipanggil
    // setiap ada lagu baru yang mulai diputar).
    void refreshHistory();

private:
    QWidget *makeSection();
    void onTextEdited(const QString &text);
    void fetchSuggestions(const QString &query);

    void loadQuickPicks(bool personal = true);
    void loadRecommendations();
    void loadRecent();
    void loadShortcuts();
    void loadAlbums();
    void loadGenres();
    void loadHistory();

    void fillTrackHScroll(QWidget *section, const QString &title, const QString &subtitle,
                          const QList<Track> &tracks, const QString &actionText,
                          const std::function<void()> &action);
    void fillTrackGrid(QWidget *section, const QString &title, const QString &subtitle,
                       const QList<Track> &tracks, const QString &actionText,
                       const std::function<void()> &action);
    void fillError(QWidget *section, const QString &title, const QString &subtitle,
                   const QString &message, const std::function<void()> &retry);

    MainWindow *m_app;
    SearchBar *m_search = nullptr;
    ScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;

    QWidget *m_quickSection = nullptr;
    QWidget *m_recoSection = nullptr;
    QWidget *m_recentSection = nullptr;
    QWidget *m_shortcutSection = nullptr;
    QWidget *m_albumSection = nullptr;
    QWidget *m_genreSection = nullptr;
    QWidget *m_historySection = nullptr;

    QTimer *m_suggestTimer = nullptr;
    int m_suggestSeq = 0;
};

} // namespace bc
