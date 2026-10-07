#include "EngineCatalog.h"

#include "UciEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSet>
#include <QUuid>

#ifndef PRAGMA_BUNDLED_ENGINE_NAME
#define PRAGMA_BUNDLED_ENGINE_NAME "Stockfish"
#endif

const QString EngineCatalog::kBundledId = QStringLiteral("bundled");

namespace {

QString executableName(const QString &base)
{
#ifdef Q_OS_WIN
    return base + QStringLiteral(".exe");
#else
    return base;
#endif
}

EngineProfile bundledProfile()
{
    EngineProfile profile;
    profile.id = EngineCatalog::kBundledId;
    profile.name = EngineCatalog::bundledEngineName();
    profile.bundled = true;
    return profile;
}

} // namespace

EngineCatalog::EngineCatalog()
{
    m_engines.append(bundledProfile());
}

EngineCatalog EngineCatalog::load(QSettings &settings)
{
    EngineCatalog catalog;
    const int count = settings.beginReadArray(QStringLiteral("engines/profiles"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        EngineProfile profile;
        profile.id = settings.value(QStringLiteral("id")).toString();
        profile.name = settings.value(QStringLiteral("name")).toString();
        profile.path = settings.value(QStringLiteral("path")).toString();
        profile.threads = qMax(0, settings.value(QStringLiteral("threads")).toInt());
        profile.hashMb = qMax(0, settings.value(QStringLiteral("hash")).toInt());
        profile.power = qBound(int(EnginePower::Minimum),
                               settings.value(QStringLiteral("power"), EnginePower::kDefault).toInt(),
                               int(EnginePower::Full));
        if (profile.id == kBundledId) {
            // Only the parameters are the user's: name and path belong to this build.
            catalog.m_engines.first().threads = profile.threads;
            catalog.m_engines.first().hashMb = profile.hashMb;
            catalog.m_engines.first().power = profile.power;
        } else if (!profile.id.isEmpty() && !catalog.find(profile.id)) {
            catalog.m_engines.append(profile);
        }
    }
    settings.endArray();
    return catalog;
}

void EngineCatalog::save(QSettings &settings) const
{
    settings.remove(QStringLiteral("engines/profiles"));
    settings.beginWriteArray(QStringLiteral("engines/profiles"), int(m_engines.size()));
    for (int i = 0; i < m_engines.size(); ++i) {
        const EngineProfile &profile = m_engines.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), profile.id);
        if (!profile.bundled) {
            settings.setValue(QStringLiteral("name"), profile.name);
            settings.setValue(QStringLiteral("path"), profile.path);
        }
        settings.setValue(QStringLiteral("threads"), profile.threads);
        settings.setValue(QStringLiteral("hash"), profile.hashMb);
        settings.setValue(QStringLiteral("power"), profile.power);
    }
    settings.endArray();
}

const EngineProfile *EngineCatalog::find(const QString &id) const
{
    for (const EngineProfile &profile : m_engines) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

const EngineProfile &EngineCatalog::resolve(const QString &id, const QString &name) const
{
    if (const EngineProfile *profile = find(id))
        return *profile;
    if (!name.isEmpty()) {
        for (const EngineProfile &profile : m_engines) {
            if (profile.name.compare(name, Qt::CaseInsensitive) == 0)
                return profile;
        }
        // Older projects stored a command or path ("stockfish", "/usr/bin/lc0").
        const QString wanted = QFileInfo(name).completeBaseName();
        for (const EngineProfile &profile : m_engines) {
            if (!profile.bundled && !profile.path.isEmpty()
                && QFileInfo(profile.path).completeBaseName().compare(wanted, Qt::CaseInsensitive) == 0)
                return profile;
        }
    }
    return m_engines.first();
}

QString EngineCatalog::add(EngineProfile profile)
{
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.bundled = false;
    m_engines.append(profile);
    return profile.id;
}

void EngineCatalog::update(const EngineProfile &profile)
{
    for (EngineProfile &existing : m_engines) {
        if (existing.id != profile.id)
            continue;
        if (existing.bundled) {
            existing.threads = profile.threads;
            existing.hashMb = profile.hashMb;
            existing.power = profile.power;
        } else {
            existing = profile;
            existing.bundled = false;
        }
        return;
    }
}

bool EngineCatalog::remove(const QString &id)
{
    if (id == kBundledId)
        return false;
    return m_engines.removeIf([&](const EngineProfile &profile) { return profile.id == id; }) > 0;
}

int EngineCatalog::addDetected(const QList<DetectedEngine> &found, const QString &bundledPath)
{
    QSet<QString> known;
    if (!bundledPath.isEmpty())
        known.insert(canonicalPath(bundledPath));
    for (const EngineProfile &profile : std::as_const(m_engines)) {
        if (!profile.path.isEmpty())
            known.insert(canonicalPath(profile.path));
    }
    int added = 0;
    for (const DetectedEngine &engine : found) {
        const QString path = canonicalPath(engine.path);
        if (path.isEmpty() || known.contains(path))
            continue;
        known.insert(path);
        EngineProfile profile;
        profile.name = engine.name.isEmpty() ? QFileInfo(engine.path).completeBaseName() : engine.name;
        profile.path = engine.path;
        add(profile);
        ++added;
    }
    return added;
}

QString EngineCatalog::canonicalPath(const QString &path)
{
    if (path.isEmpty())
        return {};
    const QFileInfo file(path);
    const QString canonical = file.canonicalFilePath();
    const QString result = canonical.isEmpty() ? QDir::cleanPath(file.absoluteFilePath()) : canonical;
#ifdef Q_OS_WIN
    return result.toLower();
#else
    return result;
#endif
}

QString EngineCatalog::bundledEngineName()
{
    return QStringLiteral(PRAGMA_BUNDLED_ENGINE_NAME);
}

QStringList EngineCatalog::bundledEngineDirs(const QString &applicationDir)
{
    QStringList dirs;
    const QString overridden = qEnvironmentVariable("PRAGMA_ENGINES_DIR");
    if (!overridden.isEmpty())
        dirs << overridden;
    const QDir app(applicationDir);
    // Windows and development builds: engines/ next to the executable.
    dirs << app.filePath(QStringLiteral("engines"));
    // Linux packages: <prefix>/bin/pragma-chess and <prefix>/lib/pragma-chess/engines.
    dirs << QDir::cleanPath(app.filePath(QStringLiteral("../lib/pragma-chess/engines")));
#ifdef Q_OS_MACOS
    // macOS: code lives in Contents/MacOS, where the signature expects it.
    dirs << applicationDir;
    // A development build: engines/ next to the bundle (`make stockfish`).
    dirs << QDir::cleanPath(app.filePath(QStringLiteral("../../../engines")));
#endif
    return dirs;
}

QString EngineCatalog::bundledEnginePath()
{
    const QString applicationDir = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();
    const QString name = executableName(QStringLiteral("stockfish"));
    for (const QString &dir : bundledEngineDirs(applicationDir)) {
        const QFileInfo file(QDir(dir).filePath(name));
        if (file.isFile() && file.isExecutable())
            return file.absoluteFilePath();
    }
    return {};
}

QString EngineCatalog::executableFor(const EngineProfile &profile)
{
    if (!profile.bundled)
        return UciEngine::findExecutable(profile.path);
    const QString bundled = bundledEnginePath();
    return bundled.isEmpty() ? UciEngine::findExecutable(QStringLiteral("stockfish")) : bundled;
}
