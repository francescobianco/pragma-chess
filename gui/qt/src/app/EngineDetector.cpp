#include "EngineDetector.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>

namespace {

// Distinctive names only: a candidate is started to be asked "uci", so a name
// shared with an ordinary program (a "fire" or "obsidian" that is not an
// engine) would start that program.
const QStringList kKnownNames = {
    QStringLiteral("stockfish"), QStringLiteral("lc0"), QStringLiteral("berserk"),
    QStringLiteral("ethereal"), QStringLiteral("rubichess"), QStringLiteral("koivisto"),
    QStringLiteral("caissa"), QStringLiteral("komodo"), QStringLiteral("arasan"),
    QStringLiteral("igel"), QStringLiteral("halogen"), QStringLiteral("minic"),
    QStringLiteral("weiss"), QStringLiteral("xiphos"), QStringLiteral("texel"),
    QStringLiteral("rodent"), QStringLiteral("alexandria"), QStringLiteral("viridithas"),
    QStringLiteral("akimbo"), QStringLiteral("plentychess"), QStringLiteral("stormphrax"),
    QStringLiteral("blackmarlin"), QStringLiteral("zahak"), QStringLiteral("shashchess"),
    QStringLiteral("brainfish"), QStringLiteral("cfish"), QStringLiteral("smallbrain"),
    QStringLiteral("tucano"), QStringLiteral("demolito"), QStringLiteral("defenchess"),
    QStringLiteral("fairy-stockfish"),
};

using SearchDir = EngineDetector::SearchDir;

bool isExecutableFile(const QFileInfo &file)
{
    if (!file.isFile())
        return false;
#ifdef Q_OS_WIN
    return file.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0;
#else
    static const QStringList libraries = {QStringLiteral("so"), QStringLiteral("dylib"), QStringLiteral("a"),
                                          QStringLiteral("txt"), QStringLiteral("nnue"), QStringLiteral("pb")};
    return file.isExecutable() && !libraries.contains(file.suffix().toLower());
#endif
}

void addDir(QList<SearchDir> &dirs, const QString &path, int depth = 0)
{
    if (!path.isEmpty())
        dirs.append({QDir::cleanPath(path), depth});
}

QList<SearchDir> systemSearchDirs(const QString &home, const QString &pragmaDir)
{
    QList<SearchDir> dirs;
    const QStringList pathEntries = qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &entry : pathEntries)
        addDir(dirs, entry);
    if (!pragmaDir.isEmpty())
        addDir(dirs, QDir(pragmaDir).filePath(QStringLiteral("Engines")), 2);
    addDir(dirs, QDir(home).filePath(QStringLiteral("Chess/Engines")), 2);
#if defined(Q_OS_WIN)
    for (const char *variable : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"})
        addDir(dirs, qEnvironmentVariable(variable), 2);
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        addDir(dirs, localAppData + QStringLiteral("/Programs"), 2);
        addDir(dirs, localAppData + QStringLiteral("/Microsoft/WinGet/Links"));
        addDir(dirs, localAppData + QStringLiteral("/Microsoft/WinGet/Packages"), 3);
    }
    addDir(dirs, QDir(home).filePath(QStringLiteral("scoop/shims")));
    addDir(dirs, QStringLiteral("C:/ProgramData/chocolatey/bin"));
#else
    for (const char *path : {"/usr/games", "/usr/local/games", "/usr/local/bin", "/usr/bin", "/snap/bin",
                             "/opt/homebrew/bin", "/opt/local/bin"})
        addDir(dirs, QString::fromLatin1(path));
    addDir(dirs, QStringLiteral("/opt"), 2);
    addDir(dirs, QDir(home).filePath(QStringLiteral(".local/bin")));
    addDir(dirs, QDir(home).filePath(QStringLiteral("bin")));
#ifdef Q_OS_MACOS
    // Chess applications that carry engines of their own.
    for (const QString &applications : {QStringLiteral("/Applications"), QDir(home).filePath(QStringLiteral("Applications"))}) {
        const QFileInfoList apps = QDir(applications).entryInfoList({QStringLiteral("*.app")}, QDir::Dirs);
        for (const QFileInfo &app : apps)
            addDir(dirs, app.filePath() + QStringLiteral("/Contents/Resources"), 1);
    }
#endif
#endif
    return dirs;
}

} // namespace

const QStringList &EngineDetector::knownNames()
{
    return kKnownNames;
}

bool EngineDetector::looksLikeEngine(const QString &fileName)
{
    QString name = QFileInfo(fileName).fileName().toLower();
    if (name.endsWith(QStringLiteral(".exe")))
        name.chop(4);
    for (const QString &known : kKnownNames) {
        if (!name.startsWith(known))
            continue;
        // "stockfish", "stockfish-17", "stockfish_x64" — not "stockfishing".
        if (name.size() == known.size())
            return true;
        const QChar next = name.at(known.size());
        if (next == QLatin1Char('-') || next == QLatin1Char('_') || next == QLatin1Char('.') || next.isDigit())
            return true;
    }
    return false;
}

QList<EngineDetector::SearchDir> EngineDetector::searchDirs(const QString &home, const QString &pragmaDir)
{
    return systemSearchDirs(home, pragmaDir);
}

QStringList EngineDetector::candidates(const QList<SearchDir> &dirs)
{
    QStringList found;
    QSet<QString> seen;
    QSet<QString> visited;
    const auto visit = [&](const auto &self, const QString &path, int depth) -> void {
        const QDir dir(path);
        const QString canonicalDir = QFileInfo(path).canonicalFilePath();
        if (canonicalDir.isEmpty() || visited.contains(canonicalDir))
            return;
        visited.insert(canonicalDir);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (entry.isDir()) {
                if (depth > 0 && !entry.isSymLink())
                    self(self, entry.filePath(), depth - 1);
                continue;
            }
            if (!looksLikeEngine(entry.fileName()) || !isExecutableFile(entry))
                continue;
            const QString canonical = EngineCatalog::canonicalPath(entry.filePath());
            if (seen.contains(canonical))
                continue;
            seen.insert(canonical);
            found << entry.filePath();
        }
    };
    for (const SearchDir &dir : dirs)
        visit(visit, dir.path, dir.depth);
    return found;
}

QString EngineDetector::handshake(const QString &path, int timeoutMs)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(path, {});
    if (!process.waitForStarted(timeoutMs))
        return {};
    process.write("uci\n");

    QString name;
    bool answered = false;
    QByteArray buffer;
    const QDeadlineTimer deadline(timeoutMs);
    while (!answered && !deadline.hasExpired() && process.state() == QProcess::Running) {
        process.waitForReadyRead(int(qMax<qint64>(1, deadline.remainingTime())));
        buffer += process.readAllStandardOutput();
        qsizetype newline;
        while ((newline = buffer.indexOf('\n')) >= 0) {
            const QByteArray line = buffer.left(newline).trimmed();
            buffer.remove(0, newline + 1);
            if (line.startsWith("id name "))
                name = QString::fromUtf8(line.mid(8)).trimmed();
            else if (line == "uciok")
                answered = true;
        }
    }
    process.write("quit\n");
    if (!process.waitForFinished(1000)) {
        process.kill();
        process.waitForFinished(1000);
    }
    if (!answered)
        return {};
    return name.isEmpty() ? QFileInfo(path).completeBaseName() : name;
}

QList<DetectedEngine> EngineDetector::scan(const QString &pragmaDir)
{
    QList<DetectedEngine> engines;
    for (const QString &path : candidates(searchDirs(QDir::homePath(), pragmaDir))) {
        const QString name = handshake(path);
        if (!name.isEmpty())
            engines.append({path, name});
    }
    return engines;
}
