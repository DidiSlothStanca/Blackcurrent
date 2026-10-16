#include "i18n.h"
#include "views/aboutview.h"

#include "mainwindow.h"
#include "theme.h"
#include "version.h"
#include "widgets.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace bc {

AboutView::AboutView(MainWindow *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new ScrollArea(this);
    root->addWidget(scroll);

    auto *wrap = new QWidget;
    auto *wl = new QVBoxLayout(wrap);
    wl->setContentsMargins(28, 24, 28, 24);
    wl->setSpacing(18);

    // ---- Header: logo + nama + versi ----
    auto *head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(20);

    auto *logo = new QLabel;
    const qreal dpr = devicePixelRatioF();
    const int logoSize = 96;
    QPixmap pm = QPixmap(QStringLiteral(":/blackcurrant.png"))
                     .scaled(QSize(logoSize, logoSize) * dpr, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    pm.setDevicePixelRatio(dpr);
    logo->setPixmap(pm);
    logo->setFixedSize(logoSize, logoSize);
    head->addWidget(logo, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout;
    col->setContentsMargins(0, 6, 0, 0);
    col->setSpacing(6);
    col->addWidget(makeLabel(TR("TENTANG APLIKASI"), QStringLiteral("sub"), 9, true));
    auto *name = makeLabel(QString::fromUtf8(appinfo::kName), QString(), 24, true);
    name->setWordWrap(true);
    col->addWidget(name);
    col->addWidget(makeLabel(TR("Versi") + QStringLiteral(" ") + QString::fromUtf8(appinfo::kVersion),
                             QStringLiteral("accent"), 11, true));
    col->addStretch(1);
    head->addLayout(col, 1);
    wl->addLayout(head);

    // ---- Kartu info ----
    auto *panel = new QFrame;
    panel->setObjectName(QStringLiteral("panel"));
    auto *grid = new QGridLayout(panel);
    grid->setContentsMargins(22, 18, 22, 18);
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(1, 1);

    struct Row {
        QString key;
        QString value;
    };
    const QList<Row> rows = {
        {TR("Pengembang"), QString::fromUtf8(appinfo::kDeveloper)},
        {TR("Pemelihara"), QString::fromUtf8(appinfo::kMaintainer)},
        {TR("Nama Aplikasi"), QString::fromUtf8(appinfo::kName)},
        {TR("Versi"), QString::fromUtf8(appinfo::kVersion)},
    };
    int r = 0;
    for (const Row &row : rows) {
        grid->addWidget(makeLabel(row.key, QStringLiteral("sub"), 10, true), r, 0,
                        Qt::AlignTop);
        auto *val = makeLabel(row.value, QString(), 11);
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(val, r, 1);
        ++r;
    }
    wl->addWidget(panel);

    auto *desc = makeLabel(
        TR("Blackcurrant Music Player adalah pemutar musik desktop sumber terbuka "
           "berbasis Qt/C++ dengan sumber data YouTube Music."),
        QStringLiteral("sub"), 10);
    desc->setWordWrap(true);
    wl->addWidget(desc);
    wl->addStretch(1);

    scroll->addWidget(wrap);
}

} // namespace bc
