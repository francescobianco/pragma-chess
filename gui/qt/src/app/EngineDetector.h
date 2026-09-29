#pragma once

#include "EngineCatalog.h"

#include <QList>
#include <QString>
#include <QStringList>

/// Finds UCI engines installed on this computer: known engine names in PATH
/// and in the folders where installers and package managers put them. Each
/// candidate is started once and must answer "uci" with "uciok", so a file
/// that only looks like an engine is never added.
///
/// Blocking (a handshake can take a second): run scan() off the GUI thread.
class EngineDetector {
public:
    /// A folder to search and how many levels of subfolders to enter.
    struct SearchDir {
        QString path;
        int depth = 0;
    };

    /// Names (lower case, without extension) that identify a UCI engine; a
    /// file matches if its name starts with one of them ("stockfish_17_x64").
    static const QStringList &knownNames();
    /// Whether a file name looks like one of knownNames(), without running it.
    static bool looksLikeEngine(const QString &fileName);
    /// The folders searched on this system, PATH first. @p home is the user's
    /// home, @p pragmaDir the Pragma folder (its Engines subfolder is searched).
    /// Subfolders are entered where installers put an engine with its files.
    static QList<SearchDir> searchDirs(const QString &home, const QString &pragmaDir);
    /// Engine-looking executables in @p dirs, each file once even if reached
    /// through a symlink or another folder.
    static QStringList candidates(const QList<SearchDir> &dirs);
    /// Runs the UCI handshake; returns the engine's "id name", or empty if
    /// @p path is not a UCI engine or does not answer within @p timeoutMs.
    static QString handshake(const QString &path, int timeoutMs = 3000);
    /// candidates() of searchDirs() that pass the handshake.
    static QList<DetectedEngine> scan(const QString &pragmaDir);
};
