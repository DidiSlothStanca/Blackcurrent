#pragma once

#include "track.h"

#include <QWidget>

namespace bc {

class MainWindow;

class DetailView : public QWidget
{
    Q_OBJECT
public:
    enum class Kind { Generic, Album, LocalPlaylist };

    DetailView(MainWindow *app, const QString &title, const QString &subtitle,
               const QString &cover, const QList<Track> &tracks, Kind kind,
               const QString &refId, QWidget *parent = nullptr);

private:
    MainWindow *m_app;
};

} // namespace bc
