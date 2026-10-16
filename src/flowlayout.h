#pragma once

// FlowLayout — susunan yang membungkus ke baris berikutnya sesuai lebar yang
// tersedia. Item tetap berukuran aslinya dan rapat (jarak tetap), sehingga
// kartu tidak "melebar" mengikuti jendela, tetapi jumlah kolom menyesuaikan
// lebar layar. Dipakai untuk kisi album di halaman artis.

#include <QLayout>
#include <QStyle>
#include <QWidget>
#include <QWidgetItem>

#include <algorithm>

namespace bc {

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int hSpacing = 12, int vSpacing = 12)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
    }

    ~FlowLayout() override
    {
        while (QLayoutItem *item = takeAt(0))
            delete item;
    }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem *takeAt(int index) override
    {
        return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
    }

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }
    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override
    {
        QSize size;
        for (const QLayoutItem *item : m_items)
            size = size.expandedTo(item->minimumSize());
        const QMargins m = contentsMargins();
        return size + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        const QMargins m = contentsMargins();
        const QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = eff.x();
        int y = eff.y();
        int lineH = 0;
        for (QLayoutItem *item : m_items) {
            const QSize hint = item->sizeHint();
            int nextX = x + hint.width() + m_hSpace;
            if (nextX - m_hSpace > eff.right() + 1 && lineH > 0) {
                x = eff.x();
                y += lineH + m_vSpace;
                nextX = x + hint.width() + m_hSpace;
                lineH = 0;
            }
            if (!testOnly)
                item->setGeometry(QRect(QPoint(x, y), hint));
            x = nextX;
            lineH = std::max(lineH, hint.height());
        }
        return y + lineH - rect.y() + m.bottom();
    }

    QList<QLayoutItem *> m_items;
    int m_hSpace;
    int m_vSpace;
};

} // namespace bc
