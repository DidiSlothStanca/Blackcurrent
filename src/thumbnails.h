#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;

namespace bc {

// Pemuat gambar asinkron dengan cache memori + disk.
class ThumbnailLoader : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const QPixmap &)>;

    static ThumbnailLoader &instance();

    void setCacheDir(const QString &dir);

    // Skala & (opsional) bulatkan thumbnail. Callback dijalankan di thread GUI.
    void load(const QString &url, int size, bool rounded, Callback cb);

    // Ambil versi tersimpan (tanpa memicu unduhan). Null bila belum ada.
    QPixmap cached(const QString &url, int size, bool rounded) const;

    static QPixmap roundPixmap(const QPixmap &src, int radius);

private:
    explicit ThumbnailLoader(QObject *parent = nullptr);

    QString key(const QString &url, int size, bool rounded) const;
    QString diskPath(const QString &url, int size, bool rounded) const;
    void fetch(const QString &url, const QString &key, const QString &disk, int size, bool rounded);
    void flush(const QString &key);

    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, QPixmap> m_cache;
    QHash<QString, QList<Callback>> m_waiting;
    QHash<QString, int> m_active; // key -> size (info tambahan)
    QString m_dir;
};

} // namespace bc
