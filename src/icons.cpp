#include "icons.h"

#include "theme.h"

#include <QIconEngine>
#include <QPaintEvent>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>

#include <initializer_list>

namespace bc {
namespace icons {

QColor colorFor(Role role)
{
    const theme::Palette &p = theme::palette();
    switch (role) {
    case Role::Text: return p.text;
    case Role::Accent: return p.accent;
    case Role::OnAccent: return p.accentText;
    case Role::Dim: return p.subtextDim;
    case Role::White: return QColor(Qt::white);
    case Role::Sub:
    default: return p.subtext;
    }
}

namespace {

QPolygonF poly(std::initializer_list<QPointF> pts)
{
    QPolygonF pg;
    for (const QPointF &pt : pts)
        pg << pt;
    return pg;
}

} // namespace

// Semua koordinat memakai kisi 24x24, lalu diskalakan ke `rect`.
void paint(QPainter *p, const QRectF &rect, Kind kind, const QColor &color)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const qreal side = qMin(rect.width(), rect.height());
    p->translate(rect.center().x() - side / 2.0, rect.center().y() - side / 2.0);
    p->scale(side / 24.0, side / 24.0);

    const QPen strokePen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    auto stroke = [&]() {
        p->setPen(strokePen);
        p->setBrush(Qt::NoBrush);
    };
    auto fill = [&]() {
        p->setPen(Qt::NoPen);
        p->setBrush(color);
    };

    switch (kind) {
    case Kind::Home: {
        stroke();
        p->drawPolyline(poly({{3.5, 11}, {12, 4}, {20.5, 11}}));
        p->drawPolyline(poly({{6, 9.5}, {6, 20}, {10, 20}, {10, 14}, {14, 14}, {14, 20},
                              {18, 20}, {18, 9.5}}));
        break;
    }
    case Kind::Search: {
        stroke();
        p->drawEllipse(QPointF(10.5, 10.5), 6.5, 6.5);
        p->drawLine(QPointF(15.5, 15.5), QPointF(20.5, 20.5));
        break;
    }
    case Kind::Library: {
        stroke();
        p->drawRoundedRect(QRectF(4, 4, 4, 16), 1, 1);
        p->drawRoundedRect(QRectF(10, 4, 4, 16), 1, 1);
        p->drawLine(QPointF(17, 5.5), QPointF(20.5, 19));
        break;
    }
    case Kind::Heart:
    case Kind::HeartFilled: {
        QPainterPath h;
        h.moveTo(12, 21);
        h.cubicTo(5, 15.5, 2, 11.5, 2, 8);
        h.cubicTo(2, 5, 4.3, 3, 7, 3);
        h.cubicTo(9.2, 3, 11, 4.2, 12, 6);
        h.cubicTo(13, 4.2, 14.8, 3, 17, 3);
        h.cubicTo(19.7, 3, 22, 5, 22, 8);
        h.cubicTo(22, 11.5, 19, 15.5, 12, 21);
        h.closeSubpath();
        if (kind == Kind::HeartFilled) {
            p->setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(color);
        } else {
            stroke();
        }
        p->drawPath(h);
        break;
    }
    case Kind::Disc: {
        stroke();
        p->drawEllipse(QPointF(12, 12), 9, 9);
        p->drawEllipse(QPointF(12, 12), 2.5, 2.5);
        break;
    }
    case Kind::Info: {
        stroke();
        p->drawEllipse(QPointF(12, 12), 9, 9);
        p->drawLine(QPointF(12, 11), QPointF(12, 17));
        fill();
        p->drawEllipse(QPointF(12, 7.6), 1.3, 1.3);
        break;
    }
    case Kind::History: {
        stroke();
        p->drawEllipse(QPointF(12, 12), 9, 9);
        p->drawPolyline(poly({{12, 7}, {12, 12}, {15.5, 14}}));
        break;
    }
    case Kind::Shuffle: {
        stroke();
        QPainterPath a;
        a.moveTo(3, 17);
        a.lineTo(6.5, 17);
        a.cubicTo(12, 17, 11, 7, 16.5, 7);
        a.lineTo(20, 7);
        p->drawPath(a);
        p->drawPolyline(poly({{17.5, 4}, {20.5, 7}, {17.5, 10}}));
        QPainterPath b;
        b.moveTo(3, 7);
        b.lineTo(6.5, 7);
        b.cubicTo(8, 7, 9, 7.8, 10, 9);
        p->drawPath(b);
        QPainterPath c;
        c.moveTo(14, 15);
        c.cubicTo(15, 16.2, 16, 17, 17.5, 17);
        c.lineTo(20, 17);
        p->drawPath(c);
        p->drawPolyline(poly({{17.5, 14}, {20.5, 17}, {17.5, 20}}));
        break;
    }
    case Kind::Repeat: {
        stroke();
        QPainterPath a;
        a.moveTo(3.5, 12);
        a.lineTo(3.5, 10);
        a.quadTo(3.5, 6.5, 7, 6.5);
        a.lineTo(19, 6.5);
        p->drawPath(a);
        p->drawPolyline(poly({{16, 3.5}, {19, 6.5}, {16, 9.5}}));
        QPainterPath b;
        b.moveTo(20.5, 12);
        b.lineTo(20.5, 14);
        b.quadTo(20.5, 17.5, 17, 17.5);
        b.lineTo(5, 17.5);
        p->drawPath(b);
        p->drawPolyline(poly({{8, 14.5}, {5, 17.5}, {8, 20.5}}));
        // angka "1" di tengah = ulangi satu lagu
        p->setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPolyline(poly({{10.6, 10.6}, {12.3, 9.4}, {12.3, 14.6}}));
        break;
    }
    case Kind::Prev: {
        fill();
        p->drawRoundedRect(QRectF(5, 5, 2.4, 14), 1, 1);
        p->drawPolygon(poly({{19.5, 5.5}, {19.5, 18.5}, {8.8, 12}}));
        break;
    }
    case Kind::Next: {
        fill();
        p->drawRoundedRect(QRectF(16.6, 5, 2.4, 14), 1, 1);
        p->drawPolygon(poly({{4.5, 5.5}, {4.5, 18.5}, {15.2, 12}}));
        break;
    }
    case Kind::Play: {
        fill();
        p->drawPolygon(poly({{7, 4.5}, {7, 19.5}, {19.5, 12}}));
        break;
    }
    case Kind::Mic: {
        stroke();
        p->drawRoundedRect(QRectF(9, 3, 6, 11), 3, 3);
        QPainterPath m;
        m.moveTo(5.5, 11);
        m.cubicTo(5.5, 17.5, 18.5, 17.5, 18.5, 11);
        p->drawPath(m);
        p->drawLine(QPointF(12, 16.3), QPointF(12, 21));
        p->drawLine(QPointF(8.5, 21), QPointF(15.5, 21));
        break;
    }
    case Kind::Headphones: {
        stroke();
        QPainterPath band;
        band.moveTo(4, 16);
        band.cubicTo(4, 5, 20, 5, 20, 16);
        p->drawPath(band);
        fill();
        p->drawRoundedRect(QRectF(3, 14, 4.5, 7), 1.6, 1.6);
        p->drawRoundedRect(QRectF(16.5, 14, 4.5, 7), 1.6, 1.6);
        break;
    }
    case Kind::Volume: {
        fill();
        p->drawPolygon(poly({{3.5, 9.5}, {8, 9.5}, {13, 5.5}, {13, 18.5}, {8, 14.5},
                             {3.5, 14.5}}));
        stroke();
        p->drawArc(QRectF(8, 7, 10, 10), -45 * 16, 90 * 16);
        p->drawArc(QRectF(5, 4, 16, 16), -45 * 16, 90 * 16);
        break;
    }
    case Kind::Menu: {
        stroke();
        p->drawLine(QPointF(4, 6), QPointF(20, 6));
        p->drawLine(QPointF(4, 12), QPointF(20, 12));
        p->drawLine(QPointF(4, 18), QPointF(20, 18));
        break;
    }
    case Kind::Back: {
        stroke();
        p->drawLine(QPointF(20, 12), QPointF(5, 12));
        p->drawPolyline(poly({{11.5, 5.5}, {5, 12}, {11.5, 18.5}}));
        break;
    }
    case Kind::More: {
        fill();
        p->drawEllipse(QPointF(12, 5), 1.9, 1.9);
        p->drawEllipse(QPointF(12, 12), 1.9, 1.9);
        p->drawEllipse(QPointF(12, 19), 1.9, 1.9);
        break;
    }
    case Kind::Trash: {
        stroke();
        p->drawLine(QPointF(4, 7), QPointF(20, 7));
        p->drawPolyline(poly({{9, 7}, {9, 4}, {15, 4}, {15, 7}}));
        p->drawPolyline(poly({{6, 7}, {7, 20}, {17, 20}, {18, 7}}));
        p->drawLine(QPointF(10.2, 11), QPointF(10.2, 16));
        p->drawLine(QPointF(13.8, 11), QPointF(13.8, 16));
        break;
    }
    case Kind::Note: {
        fill();
        p->drawEllipse(QPointF(8, 17.5), 3.4, 2.8);
        stroke();
        p->drawLine(QPointF(11.2, 17), QPointF(11.2, 5));
        QPainterPath flag;
        flag.moveTo(11.2, 5);
        flag.cubicTo(14.5, 5.5, 17.5, 7, 18.5, 11);
        p->drawPath(flag);
        break;
    }
    case Kind::Warning: {
        stroke();
        p->drawPolygon(poly({{12, 3.5}, {21.5, 20}, {2.5, 20}}));
        p->drawLine(QPointF(12, 9.5), QPointF(12, 14));
        fill();
        p->drawEllipse(QPointF(12, 17.2), 1.2, 1.2);
        break;
    }
    case Kind::QueueNext: {
        stroke();
        p->drawLine(QPointF(4, 6), QPointF(15, 6));
        p->drawLine(QPointF(4, 12), QPointF(11, 12));
        p->drawLine(QPointF(4, 18), QPointF(11, 18));
        fill();
        p->drawPolygon(poly({{14, 12.5}, {14, 21.5}, {21, 17}}));
        break;
    }
    case Kind::Plus: {
        stroke();
        p->drawLine(QPointF(12, 5), QPointF(12, 19));
        p->drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    }
    case Kind::Artist: {
        stroke();
        p->drawEllipse(QPointF(12, 8), 4, 4);
        QPainterPath body;
        body.moveTo(4.5, 20.5);
        body.cubicTo(4.5, 15, 8, 13.5, 12, 13.5);
        body.cubicTo(16, 13.5, 19.5, 15, 19.5, 20.5);
        p->drawPath(body);
        break;
    }
    case Kind::Berry: {
        QColor c = color;
        c.setAlpha(215);
        p->setPen(Qt::NoPen);
        p->setBrush(c);
        p->drawEllipse(QPointF(8.2, 15.2), 5, 5);
        p->drawEllipse(QPointF(15.8, 15.2), 5, 5);
        p->drawEllipse(QPointF(12, 8.6), 5, 5);
        break;
    }
    case Kind::Dot: {
        fill();
        p->drawEllipse(QPointF(12, 12), 4.5, 4.5);
        break;
    }
    case Kind::Check: {
        stroke();
        p->drawPolyline(poly({{5, 12.5}, {10, 17.5}, {19.5, 7}}));
        break;
    }
    }
    p->restore();
}

namespace {

class Engine : public QIconEngine
{
public:
    Engine(Kind kind, Role role) : m_kind(kind), m_role(role) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        QColor c = colorFor(m_role);
        if (mode == QIcon::Disabled)
            c = theme::palette().subtextDim;
        else if (mode == QIcon::Active || mode == QIcon::Selected)
            c = theme::palette().accentText; // mis. item menu yang disorot (latar aksen)
        icons::paint(painter, QRectF(rect), m_kind, c);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pm(size);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pm;
    }

    QIconEngine *clone() const override { return new Engine(m_kind, m_role); }

private:
    Kind m_kind;
    Role m_role;
};

} // namespace

QIcon icon(Kind kind, Role role) { return QIcon(new Engine(kind, role)); }

// ---------------------------------------------------------------- IconWidget

IconWidget::IconWidget(Kind kind, Role role, int size, QWidget *parent)
    : QWidget(parent), m_kind(kind), m_role(role)
{
    setFixedSize(size, size);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void IconWidget::setKind(Kind kind)
{
    m_kind = kind;
    update();
}

void IconWidget::setRole(Role role)
{
    m_role = role;
    update();
}

void IconWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    icons::paint(&p, QRectF(rect()), m_kind, colorFor(m_role));
}

} // namespace icons
} // namespace bc
