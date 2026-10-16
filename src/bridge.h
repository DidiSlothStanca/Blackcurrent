#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QProcess;

namespace bc {

// Klien untuk proses Python `bridge.py` (protokol JSON per baris).
// Setiap permintaan diberi id; respons dicocokkan kembali dan dikirim ke
// callback di thread GUI.
class Bridge : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QJsonValue &result, const QString &error)>;

    explicit Bridge(QObject *parent = nullptr);
    ~Bridge() override;

    bool isRunning() const;
    bool isReady() const { return m_ready; }
    QString lastError() const { return m_lastError; }

    // Kirim permintaan. Bila proses belum hidup, proses akan dijalankan dulu.
    // `context` (biasanya widget pemanggil): bila sudah dihancurkan sebelum
    // respons datang, callback dilewati sehingga tidak ada pointer gantung.
    void request(const QString &method, const QJsonObject &params, Callback cb,
                 QObject *context = nullptr);

    void shutdown();

    static QString resolveScript();
    static QString resolvePython();

signals:
    void readyChanged(bool ready);
    void logMessage(const QString &line);

private slots:
    void onReadyRead();
    void onProcessError();
    void onFinished();

private:
    void ensureStarted();
    void dispatch(const QJsonObject &msg);
    void sendLine(const QByteArray &line);
    void flushPending();
    void failAll(const QString &message);

    QProcess *m_proc = nullptr;
    QByteArray m_buffer;
    bool m_ready = false;
    QString m_lastError;
    QString m_lastStderr;
    struct Pending {
        Callback cb;
        QByteArray payload; // baris yang belum terkirim
        QPointer<QObject> context;
        bool hasContext = false; // QPointer yang sudah null tidak bisa dibedakan dari "tanpa context"

        // false bila pemanggil (widget) sudah dihancurkan -> callback tidak boleh dipanggil.
        bool alive() const { return !hasContext || !context.isNull(); }
    };
    QHash<int, Pending> m_pending;
    int m_nextId = 1;
};

} // namespace bc
