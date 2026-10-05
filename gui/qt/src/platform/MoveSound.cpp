#include "MoveSound.h"

#include <QDir>
#include <QFile>
#include <QTemporaryFile>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#include <mmsystem.h>
#elif defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#else
#include <QProcess>
#include <QStandardPaths>
#endif

namespace {

const QString kResource = QStringLiteral(":/resources/sounds/move.wav");

#if !defined(Q_OS_WIN)
/// The sound as a file of its own, for the players that want a path: in
/// the system's temporary folder, removed when the application ends.
QString soundFile()
{
    static QTemporaryFile file(QDir::temp().filePath(QStringLiteral("pragma-chess-move-XXXXXX.wav")));
    if (file.fileName().isEmpty()) {
        QFile resource(kResource);
        if (resource.open(QIODevice::ReadOnly) && file.open()) {
            file.write(resource.readAll());
            file.flush();
        }
    }
    return file.fileName();
}
#endif

} // namespace

namespace MoveSound {

void play()
{
#if defined(Q_OS_WIN)
    // PlaySound reads the WAV from memory, which has to outlive the sound.
    static const QByteArray data = [] {
        QFile resource(kResource);
        return resource.open(QIODevice::ReadOnly) ? resource.readAll() : QByteArray();
    }();
    if (!data.isEmpty())
        PlaySoundW(reinterpret_cast<LPCWSTR>(data.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#elif defined(Q_OS_MACOS)
    static const SystemSoundID sound = [] {
        SystemSoundID id = 0;
        const QByteArray path = QFile::encodeName(soundFile());
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(path.constData()),
                                                               path.size(), false);
        if (url) {
            AudioServicesCreateSystemSoundID(url, &id);
            CFRelease(url);
        }
        return id;
    }();
    if (sound)
        AudioServicesPlaySystemSound(sound);
#else
    // PipeWire's player first (current desktops), then PulseAudio's, then ALSA's.
    static const QStringList command = [] {
        for (const QStringList &candidate : {QStringList{QStringLiteral("pw-play")},
                                             QStringList{QStringLiteral("paplay")},
                                             QStringList{QStringLiteral("aplay"), QStringLiteral("-q")}}) {
            const QString program = QStandardPaths::findExecutable(candidate.first());
            if (!program.isEmpty())
                return QStringList{program} + candidate.mid(1);
        }
        return QStringList();
    }();
    if (!command.isEmpty())
        QProcess::startDetached(command.first(), command.mid(1) << soundFile());
#endif
}

} // namespace MoveSound
