#pragma once

#include <QWidget>

namespace bc {

class MainWindow;

class LibraryView : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryView(MainWindow *app, QWidget *parent = nullptr);
    void refresh();

private:
    MainWindow *m_app;
    class ScrollArea *m_scroll = nullptr;
};

class LikedView : public QWidget
{
    Q_OBJECT
public:
    explicit LikedView(MainWindow *app, QWidget *parent = nullptr);

private:
    MainWindow *m_app;
};

class LikedAlbumsView : public QWidget
{
    Q_OBJECT
public:
    explicit LikedAlbumsView(MainWindow *app, QWidget *parent = nullptr);

private:
    MainWindow *m_app;
};

class HistoryView : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryView(MainWindow *app, QWidget *parent = nullptr);

private:
    MainWindow *m_app;
};

} // namespace bc
