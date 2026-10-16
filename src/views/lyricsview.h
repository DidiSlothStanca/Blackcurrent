#pragma once

#include "track.h"

#include <QWidget>

class QLabel;

namespace bc {

class MainWindow;
class ScrollArea;

// Panel lirik yang muncul dari sisi kanan.
class LyricsView : public QWidget
{
    Q_OBJECT
public:
    explicit LyricsView(MainWindow *app, QWidget *parent = nullptr);

    void showFor(const Track &track);
    void clear();
    void reload();
    bool hasTrack() const { return !m_track.videoId.isEmpty(); }

private:
    void setMessage(const QString &text);
    void applyContent();

    MainWindow *m_app;
    Track m_track;
    QLabel *m_headerTitle = nullptr;
    QLabel *m_headerSub = nullptr;
    ScrollArea *m_scroll = nullptr;
    QLabel *m_body = nullptr;
    int m_seq = 0;
};

} // namespace bc
