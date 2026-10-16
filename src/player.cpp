#include "i18n.h"
#include "player.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QDir>
#include <QTimer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QProcess>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QUrl>

#include <cmath>

namespace bc {

namespace {
const QStringList kClientFallbacks = {
    QString(), // bawaan yt-dlp
    QStringLiteral("web_safari"),
    QStringLiteral("android"),
    QStringLiteral("ios"),
    QStringLiteral("tv"),
};

// Bangun berkas WAV PCM 16-bit sederhana.
QByteArray makeWav(const QByteArray &pcm, int channels, int sampleRate)
{
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;
    const int dataSize = pcm.size();

    QByteArray wav;
    wav.reserve(44 + dataSize);
    auto put32 = [&wav](quint32 v) {
        wav.append(char(v & 0xff));
        wav.append(char((v >> 8) & 0xff));
        wav.append(char((v >> 16) & 0xff));
        wav.append(char((v >> 24) & 0xff));
    };
    auto put16 = [&wav](quint16 v) {
        wav.append(char(v & 0xff));
        wav.append(char((v >> 8) & 0xff));
    };
    wav.append("RIFF");
    put32(36 + dataSize);
    wav.append("WAVE");
    wav.append("fmt ");
    put32(16);
    put16(1); // PCM
    put16(quint16(channels));
    put32(quint32(sampleRate));
    put32(quint32(byteRate));
    put16(quint16(blockAlign));
    put16(quint16(bitsPerSample));
    wav.append("data");
    put32(quint32(dataSize));
    wav.append(pcm);
    return wav;
}

// Folder salinan yt-dlp yang bisa ditulis: ~/.config/blackcurrant-music-player/bin
QString managedYtdlpDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/blackcurrant-music-player/bin");
}

QString managedYtdlpFile() { return managedYtdlpDir() + QStringLiteral("/yt-dlp"); }

QString bundledYtdlpFile()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/yt-dlp");
}

QString updateStampFile() { return managedYtdlpDir() + QStringLiteral("/.last-update"); }

// Lingkungan proses anak. Di dalam AppImage, jangan wariskan pustaka Qt yang dibundel.
QProcessEnvironment childEnvironment()
{
    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    if (qEnvironmentVariableIsSet("APPDIR")) {
        penv.remove(QStringLiteral("LD_LIBRARY_PATH"));
        penv.remove(QStringLiteral("PYTHONHOME"));
        penv.remove(QStringLiteral("PYTHONPATH"));
    }
    return penv;
}

constexpr qint64 kUpdateIntervalSecs = 24 * 3600; // cek pembaruan paling sering sehari sekali

} // namespace

AudioPlayer::AudioPlayer(QObject *parent) : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(m_volume / 100.0);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus st) {
                if (st == QMediaPlayer::EndOfMedia)
                    emit finished();
            });
    connect(m_player, &QMediaPlayer::playingChanged, this,
            [this]() { emit playingChanged(isPlaying()); });
    connect(m_player, &QMediaPlayer::positionChanged, this,
            [this](qint64 pos) { emit positionChanged(pos, m_player->duration()); });
    connect(m_player, &QMediaPlayer::durationChanged, this,
            [this](qint64 dur) { emit positionChanged(m_player->position(), dur); });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &err) {
                if (!err.isEmpty())
                    emit failed(TR("Gagal memutar: %1").arg(err));
            });

    // Perbarui yt-dlp di latar belakang (tidak menghambat start aplikasi).
    QTimer::singleShot(8000, this, [this]() { maybeUpdateYtdlp(); });
}

AudioPlayer::~AudioPlayer()
{
    if (m_resolver) {
        m_resolver->disconnect(this);
        m_resolver->kill();
        m_resolver->waitForFinished(800);
    }
}

QString AudioPlayer::ytdlpPath() const
{
    const QString custom = QString::fromLocal8Bit(qgetenv("BC_YTDLP")).trimmed();
    if (!custom.isEmpty())
        return custom;
    // yt-dlp yang dibundel di AppImage (<APPDIR>/usr/bin/yt-dlp, sebelah file eksekusi).
    // AppImage hanya-baca, jadi salin ke folder data pengguna agar bisa diperbarui sendiri.
    const QString bundled = bundledYtdlpFile();
    if (QFileInfo::exists(bundled)) {
        const QString managed = managedYtdlpFile();
        const QFileInfo mi(managed);
        const QFileInfo bi(bundled);
        // Salin bila belum ada, atau bila AppImage lebih baru dari salinan (build ulang).
        if (!mi.exists() || bi.lastModified() > mi.lastModified()) {
            QDir().mkpath(managedYtdlpDir());
            QFile::remove(managed);
            if (QFile::copy(bundled, managed)) {
                QFile::setPermissions(managed, QFile::permissions(managed) | QFileDevice::ExeOwner
                                                   | QFileDevice::ExeGroup | QFileDevice::ExeOther);
            }
        }
        if (QFileInfo(managed).isExecutable())
            return managed;
        return bundled; // cadangan: pakai langsung dari dalam AppImage
    }
    const QString found = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!found.isEmpty())
        return found;
    const QString local = QDir::homePath() + QStringLiteral("/.local/bin/yt-dlp");
    if (QFileInfo::exists(local))
        return local;
    return QStringLiteral("yt-dlp");
}

bool AudioPlayer::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 AudioPlayer::position() const
{
    return m_player ? m_player->position() : 0;
}

qint64 AudioPlayer::duration() const
{
    return m_player ? m_player->duration() : 0;
}

void AudioPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
    if (m_audio)
        m_audio->setVolume(m_volume / 100.0);
}

void AudioPlayer::setResolving(bool value)
{
    if (m_resolving == value)
        return;
    m_resolving = value;
    emit resolvingChanged(value);
}

void AudioPlayer::playTrack(const Track &track)
{
    if (track.videoId.isEmpty())
        return;
    m_pending = track;
    m_attempt = 0;
    startResolve(track);
}

void AudioPlayer::startResolve(const Track &track)
{
    if (m_resolver) {
        m_resolver->disconnect(this);
        m_resolver->kill();
        m_resolver->deleteLater();
        m_resolver = nullptr;
    }

    setResolving(true);

    m_resolver = new QProcess(this);
    m_resolver->setProgram(ytdlpPath());
    m_resolver->setProcessEnvironment(childEnvironment());
    const QString url = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(track.videoId);

    QStringList args = {
        QStringLiteral("--no-playlist"),
        QStringLiteral("--no-warnings"),
        QStringLiteral("--quiet"),
        QStringLiteral("--no-progress"),
        QStringLiteral("-f"),
        QStringLiteral("bestaudio/best"),
    };
    if (m_attempt > 0 && m_attempt < kClientFallbacks.size()) {
        args << QStringLiteral("--extractor-args")
             << QStringLiteral("youtube:player_client=%1").arg(kClientFallbacks.at(m_attempt));
    }
    args << QStringLiteral("-g") << url;
    m_resolver->setArguments(args);

    connect(m_resolver, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { onResolveFinished(code); });
    connect(m_resolver, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            setResolving(false);
            emit failed(TR("yt-dlp tidak ditemukan. Pasang yt-dlp atau atur BC_YTDLP."));
        }
    });
    m_resolver->start();
}

void AudioPlayer::onResolveFinished(int exitCode)
{
    if (!m_resolver)
        return;
    const QString out = QString::fromUtf8(m_resolver->readAllStandardOutput()).trimmed();
    m_resolver->deleteLater();
    m_resolver = nullptr;

    if (exitCode == 0 && !out.isEmpty()) {
        const QString url = out.split(QLatin1Char('\n')).first().trimmed();
        if (!url.isEmpty()) {
            setResolving(false);
            beginPlayback(url, m_pending);
            return;
        }
    }

    m_attempt++;
    if (m_attempt < kClientFallbacks.size()) {
        startResolve(m_pending);
        return;
    }

    // Semua klien gagal: kemungkinan yt-dlp sudah usang. Perbarui sekali lalu coba lagi.
    if (!m_updateTried && ytdlpSelfUpdatable()) {
        m_updateTried = true;
        runYtdlpUpdate([this]() {
            m_attempt = 0;
            startResolve(m_pending);
        });
        return; // tetap dalam keadaan "resolving" selama pembaruan
    }

    setResolving(false);
    emit failed(TR("Gagal mengambil stream audio (periksa koneksi internet)."));
}

// Hanya salinan yt-dlp yang dikelola aplikasi yang diperbarui otomatis. yt-dlp dari sistem
// (pip/apt) atau BC_YTDLP dibiarkan, karena pengelolanya bukan aplikasi ini.
bool AudioPlayer::ytdlpSelfUpdatable() const
{
    if (!QString::fromLocal8Bit(qgetenv("BC_YTDLP")).trimmed().isEmpty())
        return false;
    return QFileInfo::exists(bundledYtdlpFile()) && QFileInfo(managedYtdlpFile()).isExecutable();
}

void AudioPlayer::maybeUpdateYtdlp()
{
    if (!ytdlpSelfUpdatable())
        return;
    const QFileInfo stamp(updateStampFile());
    if (stamp.exists() && stamp.lastModified().secsTo(QDateTime::currentDateTime()) < kUpdateIntervalSecs)
        return;
    runYtdlpUpdate(nullptr);
}

void AudioPlayer::runYtdlpUpdate(const std::function<void()> &done)
{
    // Pembaruan sudah berjalan (mis. dari start aplikasi): cukup tunggu selesai.
    if (m_updater && m_updater->state() != QProcess::NotRunning) {
        if (done)
            connect(m_updater, &QProcess::finished, this, [done]() { done(); },
                    Qt::SingleShotConnection);
        return;
    }
    if (m_updater) {
        m_updater->deleteLater();
        m_updater = nullptr;
    }

    m_updater = new QProcess(this);
    m_updater->setProgram(managedYtdlpFile());
    m_updater->setArguments({QStringLiteral("-U")});
    m_updater->setProcessEnvironment(childEnvironment());

    connect(m_updater, &QProcess::finished, this, [this, done](int, QProcess::ExitStatus) {
        // Catat waktu pengecekan (berhasil atau tidak) agar tidak mengecek berulang-ulang.
        QFile stamp(updateStampFile());
        if (stamp.open(QIODevice::WriteOnly))
            stamp.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
        if (done)
            done();
    });
    connect(m_updater, &QProcess::errorOccurred, this, [done](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && done)
            done();
    });
    m_updater->start();
}

void AudioPlayer::beginPlayback(const QString &url, const Track &track)
{
    m_current = track;
    m_player->setSource(QUrl(url));
    m_player->play();
    emit started(track);
    emit playingChanged(true);
}

void AudioPlayer::toggle()
{
    if (!hasTrack())
        return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void AudioPlayer::pause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
}

void AudioPlayer::resume()
{
    if (hasTrack())
        m_player->play();
}

void AudioPlayer::stop()
{
    m_player->stop();
    emit playingChanged(false);
}

void AudioPlayer::seekTo(qint64 ms)
{
    if (hasTrack())
        m_player->setPosition(qMax<qint64>(0, ms));
}

QList<QAudioDevice> AudioPlayer::outputDevices() const
{
    return QMediaDevices::audioOutputs();
}

bool AudioPlayer::hasAudioOutput() const
{
    return !QMediaDevices::defaultAudioOutput().isNull();
}

QString AudioPlayer::currentOutputName() const
{
    if (!m_outputDevice.isNull())
        return m_outputDevice.description();
    const QAudioDevice def = QMediaDevices::defaultAudioOutput();
    return def.isNull() ? QString() : def.description();
}

void AudioPlayer::setOutputDevice(const QAudioDevice &device)
{
    m_outputDevice = device;
    if (m_audio)
        m_audio->setDevice(device);
    if (m_toneOutput)
        m_toneOutput->setDevice(device);
}

void AudioPlayer::playTestTone()
{
    if (!hasAudioOutput()) {
        emit failed(TR("Tidak ada perangkat keluaran audio terdeteksi."));
        return;
    }

    const int sampleRate = 44100;
    const int channels = 1;
    const double seconds = 0.6;
    const int frames = int(sampleRate * seconds);

    // Nada sinus 440 Hz dengan amplop lembut agar tidak "pop".
    QByteArray pcm;
    pcm.resize(frames * channels * sizeof(qint16));
    auto *samples = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / sampleRate;
        const double env = qMax(0.0, qMin(1.0, qMin(t * 20.0, (seconds - t) * 20.0)));
        const qint16 s = qint16(0.35 * 32767.0 * env * std::sin(2.0 * M_PI * 440.0 * t));
        for (int c = 0; c < channels; ++c)
            samples[i * channels + c] = s;
    }

    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                             .filePath(QStringLiteral("blackcurrant_test_tone.wav"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(TR("Gagal menulis nada uji."));
        return;
    }
    file.write(makeWav(pcm, channels, sampleRate));
    file.close();

    if (!m_tonePlayer) {
        m_tonePlayer = new QMediaPlayer(this);
        m_toneOutput = new QAudioOutput(this);
        m_tonePlayer->setAudioOutput(m_toneOutput);
    }
    if (!m_outputDevice.isNull())
        m_toneOutput->setDevice(m_outputDevice);
    m_toneOutput->setVolume(1.0);
    m_tonePlayer->setSource(QUrl::fromLocalFile(path));
    m_tonePlayer->play();
}

} // namespace bc
