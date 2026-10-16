#pragma once

#include <QWidget>

namespace bc {

class MainWindow;

// Halaman "Tentang" (About): nama aplikasi, versi, pengembang, dan pemelihara.
class AboutView : public QWidget
{
    Q_OBJECT
public:
    explicit AboutView(MainWindow *app, QWidget *parent = nullptr);

private:
    MainWindow *m_app;
};

} // namespace bc
