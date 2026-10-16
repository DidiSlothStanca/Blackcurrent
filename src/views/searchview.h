#pragma once

#include "track.h"

#include <QWidget>

class QTimer;

namespace bc {

class MainWindow;
class SearchBar;
class ScrollArea;

class SearchView : public QWidget
{
    Q_OBJECT
public:
    explicit SearchView(MainWindow *app, const QString &initialQuery = QString(),
                        QWidget *parent = nullptr);

private:
    void runSearch(const QString &query);
    void showHistory();
    void onTextEdited(const QString &text);
    void fetchSuggestions(const QString &query);

    MainWindow *m_app;
    SearchBar *m_search = nullptr;
    ScrollArea *m_scroll = nullptr;
    QTimer *m_suggestTimer = nullptr;
    int m_suggestSeq = 0;
    int m_searchSeq = 0;
};

} // namespace bc
