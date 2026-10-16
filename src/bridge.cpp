#include "i18n.h"
#include "bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>

namespace bc {

namespace {
QString env(const char *key)
{
    return QString::fromLocal8Bit(qgetenv(key)).trimmed();
}
} // namespace

Bridge::Bridge(QObject *parent) : QObject(parent) {}

Bridge::~Bridge()
{
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(1500);
    }
}

QString Bridge::resolvePython()
{
    const QString custom = env("BC_PYTHON");
    if (!custom.isEmpty())
        return custom;
    // Python yang dibundel di AppImage: <APPDIR>/usr/bin/../python/bin/python3.
    const QString bundled = QCoreApplication::applicationDirPath()
                            + QStringLiteral("/../python/bin/python3");
    if (QFileInfo::exists(bundled))
        return QFileInfo(bundled).absoluteFilePath();
    return QStringLiteral("python3");
}

QString Bridge::resolveScript()
{
    const QString custom = env("BC_BRIDGE_DIR");
    if (!custom.isEmpty()) {
        const QString p = custom + QStringLiteral("/bridge.py");
        if (QFileInfo::exists(p))
            return p;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/bridge/bridge.py"),
        appDir + QStringLiteral("/../share/blackcurrant/bridge/bridge.py"),
        appDir + QStringLiteral("/../bridge/bridge.py"),
#ifdef BC_SOURCE_DIR
        QStringLiteral(BC_SOURCE_DIR) + QStringLiteral("/bridge/bridge.py"),
#endif
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return QFileInfo(c).canonicalFilePath();
    }
    return appDir + QStringLiteral("/bridge/bridge.py");
}

bool Bridge::isRunning() const
{
    return m_proc && m_proc->state() == QProcess::Running;
}

void Bridge::sendLine(const QByteArray &line)
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->write(line);
    }
}

void Bridge::ensureStarted()
{
    if (m_proc)
        return;

    m_proc = new QProcess(this);
    m_proc->setProgram(resolvePython());
    m_proc->setArguments({QStringLiteral("-u"), resolveScript()});
    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    penv.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    penv.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    if (qEnvironmentVariableIsSet("APPDIR")) {
        // Di dalam AppImage: jangan wariskan pustaka Qt yang dibundel ke proses Python.
        penv.remove(QStringLiteral("LD_LIBRARY_PATH"));
        penv.remove(QStringLiteral("PYTHONHOME"));
        penv.remove(QStringLiteral("PYTHONPATH"));
    }
    m_proc->setProcessEnvironment(penv);
    m_proc->setWorkingDirectory(QFileInfo(resolveScript()).absolutePath());

    connect(m_proc, &QProcess::readyReadStandardOutput, this, &Bridge::onReadyRead);
    connect(m_proc, &QProcess::readyReadStandardError, this, [this]() {
        const QString err = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            const QStringList lines = err.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            if (!lines.isEmpty())
                m_lastStderr = lines.last().trimmed();
            emit logMessage(err);
        }
    });
    connect(m_proc, &QProcess::errorOccurred, this, &Bridge::onProcessError);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Bridge::onFinished);

    m_buffer.clear();
    m_lastError.clear();
    m_lastStderr.clear();
    m_proc->start();

    // Uji koneksi dengan ping. Permintaan lain menunggu sampai pong diterima.
    const int pingId = m_nextId++;
    m_pending.insert(pingId, Pending{nullptr, QByteArray(), QPointer<QObject>(), false});
    QJsonObject ping;
    ping[QStringLiteral("id")] = pingId;
    ping[QStringLiteral("method")] = QStringLiteral("ping");
    ping[QStringLiteral("params")] = QJsonObject();
    sendLine(QJsonDocument(ping).toJson(QJsonDocument::Compact) + "\n");
}

void Bridge::flushPending()
{
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (!it->alive()) {
            it = m_pending.erase(it);
            continue;
        }
        if (!it->payload.isEmpty()) {
            sendLine(it->payload);
            it->payload.clear();
        }
        ++it;
    }
}

void Bridge::request(const QString &method, const QJsonObject &params, Callback cb,
                     QObject *context)
{
    if (!m_proc)
        ensureStarted();

    const int id = m_nextId++;
    QJsonObject msg;
    msg[QStringLiteral("id")] = id;
    msg[QStringLiteral("method")] = method;
    msg[QStringLiteral("params")] = params;
    const QByteArray line = QJsonDocument(msg).toJson(QJsonDocument::Compact) + QByteArray("\n");

    if (m_ready) {
        m_pending.insert(id, Pending{std::move(cb), QByteArray(), context, context != nullptr});
        sendLine(line);
    } else {
        m_pending.insert(id, Pending{std::move(cb), line, context, context != nullptr});
    }
}

void Bridge::onReadyRead()
{
    if (!m_proc)
        return;
    m_buffer.append(m_proc->readAllStandardOutput());
    int idx = -1;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        if (line.trimmed().isEmpty())
            continue;
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        dispatch(doc.object());
    }
}

void Bridge::dispatch(const QJsonObject &msg)
{
    if (!msg.contains(QStringLiteral("id")) || msg.value(QStringLiteral("id")).isNull())
        return;
    const int id = msg.value(QStringLiteral("id")).toInt();
    if (!m_pending.contains(id))
        return;
    Pending p = m_pending.take(id);

    if (!p.cb) {
        // Balasan ping: jembatan siap.
        m_ready = true;
        emit readyChanged(true);
        flushPending();
        return;
    }

    // Pemanggilnya sudah dihancurkan (mis. berpindah halaman) -> lewati.
    if (!p.alive())
        return;

    p.cb(msg.value(QStringLiteral("ok")).toBool(), msg.value(QStringLiteral("result")),
         msg.value(QStringLiteral("error")).toString());
}

void Bridge::failAll(const QString &message)
{
    // Salin dulu: callback boleh memanggil request() lagi.
    const QList<int> ids = m_pending.keys();
    QList<Pending> taken;
    for (int id : ids)
        taken.append(m_pending.take(id));
    for (const Pending &p : taken) {
        if (p.cb && p.alive())
            p.cb(false, QJsonValue(), message);
    }
}

void Bridge::onProcessError()
{
    if (!m_proc)
        return;
    m_lastError = m_proc->errorString();
    emit logMessage(QStringLiteral("Bridge error: %1").arg(m_lastError));

    // Bila proses tidak pernah berhasil dijalankan, sinyal finished() tidak akan
    // muncul -> semua permintaan akan menggantung selamanya. Gagalkan sekarang.
    if (m_proc->error() == QProcess::FailedToStart) {
        const QString msg = TR("Tidak bisa menjalankan Python (%1). Pastikan python3 terpasang atau atur BC_PYTHON.")
                                .arg(resolvePython());
        m_ready = false;
        QProcess *dead = m_proc;
        m_proc = nullptr;
        dead->disconnect(this);
        dead->deleteLater();
        emit readyChanged(false);
        failAll(msg);
    }
}

void Bridge::onFinished()
{
    m_ready = false;
    emit readyChanged(false);

    QString err = m_lastStderr.isEmpty() ? m_lastError : m_lastStderr;
    if (err.isEmpty())
        err = TR("Jembatan Python berhenti");
    else
        err = TR("Jembatan Python berhenti: %1").arg(err);

    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
    failAll(err);
}

void Bridge::shutdown()
{
    if (!m_proc)
        return;
    QJsonObject msg;
    msg[QStringLiteral("id")] = m_nextId++;
    msg[QStringLiteral("method")] = QStringLiteral("__quit__");
    msg[QStringLiteral("params")] = QJsonObject();
    sendLine(QJsonDocument(msg).toJson(QJsonDocument::Compact) + QByteArray("\n"));
    m_proc->closeWriteChannel();
    if (!m_proc->waitForFinished(1200))
        m_proc->terminate();
}

} // namespace bc
