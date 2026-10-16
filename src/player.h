#pragma once

#include "track.h"

#include <QAudioDevice>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

#include <memory>

class QMediaPlayer;
class QAudioOutput;
class QProcess;

namespace bc {

// Pemutar audio: QMediaPlayer (FFmpeg) + resolusi URL lewat yt-dlp.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    void setVolume(int percent); // 0..100
    int volume() const { return m_volume; }

    void playTrack(const Track &track);
    void toggle();
    void pause();
    void resume();
    void stop();
    void seekTo(qint64 ms);

    bool isPlaying() const;
    bool hasTrack() const { return !m_current.videoId.isEmpty(); }
    Track currentTrack() const { return m_current; }
    qint64 position() const;
    qint64 duration() const;
    bool isResolving() const { return m_resolving; }

    void playTestTone();

    // ----- perangkat keluaran audio -----
    QList<QAudioDevice> outputDevices() const;
    QString currentOutputName() const;
    void setOutputDevice(const QAudioDevice &device);
    bool hasAudioOutput() const;

signals:
    void started(const Track &track);
    void finished();
    void failed(const QString &message);
    void playingChanged(bool playing);
    void positionChanged(qint64 positionMs, qint64 durationMs);
    void resolvingChanged(bool resolving);

private:
    void startResolve(const Track &track);
    void onResolveFinished(int exitCode);
    void beginPlayback(const QString &url, const Track &track);
    void setResolving(bool value);
    QString ytdlpPath() const;
    // ----- pembaruan otomatis yt-dlp (hanya salinan yang dikelola aplikasi) -----
    bool ytdlpSelfUpdatable() const;
    void maybeUpdateYtdlp();
    void runYtdlpUpdate(const std::function<void()> &done);

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QProcess *m_resolver = nullptr;
    QProcess *m_updater = nullptr;
    bool m_updateTried = false; // sudah mencoba memperbarui yt-dlp setelah gagal (per sesi)
    int m_attempt = 0;
    bool m_resolving = false;
    int m_volume = 80;
    Track m_current;
    Track m_pending;
    QAudioDevice m_outputDevice;
    QMediaPlayer *m_tonePlayer = nullptr;
    QAudioOutput *m_toneOutput = nullptr;
};

} // namespace bc
