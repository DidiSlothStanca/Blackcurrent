#pragma once

#include "track.h"

#include <QWidget>

class QVBoxLayout;

namespace bc {

class MainWindow;
class ScrollArea;

class ArtistView : public QWidget
{
    Q_OBJECT
public:
    ArtistView(MainWindow *app, const QString &browseId, QWidget *parent = nullptr);

private:
    void load();
    void loadAllSongs(const QString &browseId);

    MainWindow *m_app;
    QString m_browseId;
    ScrollArea *m_scroll = nullptr;
    QList<Track> m_topSongs;
    QWidget *m_allSongsSection = nullptr;
};

} // namespace bc
