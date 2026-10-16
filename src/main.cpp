// Blackcurrant Music — padanan Qt6/C++ dari aplikasi Tkinter.
//
// UI dan audio sepenuhnya C++/Qt; data diambil lewat proses Python
// `bridge/bridge.py` yang memakai lapisan `ytmusicapi` yang sama dengan
// versi Python.

#include "mainwindow.h"
#include "theme.h"
#include "version.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Blackcurrant Music"));
    app.setApplicationDisplayName(QStringLiteral("Blackcurrant Music"));
    app.setApplicationVersion(QString::fromUtf8(bc::appinfo::kVersion));
    app.setOrganizationName(QStringLiteral("Blackcurrant"));
    app.setDesktopFileName(QStringLiteral("blackcurrant"));
    app.setWindowIcon(QIcon(QStringLiteral(":/blackcurrant.png")));

    theme::apply();

    bc::MainWindow window;
    window.show();

    return app.exec();
}
