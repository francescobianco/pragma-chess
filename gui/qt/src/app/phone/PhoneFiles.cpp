#include "PhoneFiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {

constexpr char kSuffix[] = ".pdb";

QString sha256Of(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

QJsonObject PhoneFiles::Entry::toJson() const
{
    QJsonObject object{
        {QStringLiteral("name"), name},
        {QStringLiteral("size"), size},
        {QStringLiteral("sha256"), sha256},
        {QStringLiteral("modified"), modified.toUTC().toString(Qt::ISODate)},
        {QStringLiteral("games"), games},
    };
    if (!id.isEmpty())
        object.insert(QStringLiteral("id"), id);
    return object;
}

PhoneFiles::PhoneFiles(QString root)
    : m_root(std::move(root))
{
}

QList<PhoneFiles::Entry> PhoneFiles::list(const Describe &describe)
{
    QList<Entry> entries;
    const QDir root(m_root);
    QDirIterator it(m_root, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        const QString name = root.relativeFilePath(info.absoluteFilePath());
        if (!resolve(m_root, name))
            continue; // Hidden folders and the like.
        Hashed &hashed = m_hashes[name];
        if (hashed.sha256.isEmpty() || hashed.size != info.size() || hashed.modified != info.lastModified()
            || (describe && !hashed.described)) {
            // Describing first: it may give the file its id, which changes it.
            const std::optional<Summary> summary = describe ? describe(info.absoluteFilePath()) : std::nullopt;
            info.refresh();
            hashed = {info.size(), info.lastModified(), sha256Of(info.absoluteFilePath()),
                      summary ? summary->id : QString(), summary ? summary->games : 0, summary.has_value()};
            if (hashed.sha256.isEmpty())
                continue;
        }
        entries.append({name, hashed.size, hashed.sha256, hashed.modified, hashed.id, hashed.games});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    return entries;
}

PhoneFiles::Target PhoneFiles::target(const QList<Entry> &entries, const QString &lineage, const QString &name,
                                      const QString &phoneName)
{
    for (const Entry &entry : entries) {
        if (!lineage.isEmpty() && entry.id == lineage)
            return {entry.name, true};
    }
    const auto taken = [&entries](const QString &candidate) -> const Entry * {
        for (const Entry &entry : entries) {
            if (entry.name.compare(candidate, Qt::CaseInsensitive) == 0)
                return &entry;
        }
        return nullptr;
    };
    const Entry *same = taken(name);
    if (!same)
        return {name, false};
    if (same->id.isEmpty())
        return {same->name, true}; // A file made before ids: it becomes this lineage.

    // Another database already has the name: keep both, telling them apart.
    QString device = phoneName.trimmed();
    for (QChar &c : device) {
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c == QLatin1Char(':') || c.unicode() < 0x20)
            c = QLatin1Char('-');
    }
    while (device.startsWith(QLatin1Char('.')))
        device.remove(0, 1);
    if (device.isEmpty())
        device = QStringLiteral("phone");
    const QString base = name.left(name.size() - int(qstrlen(kSuffix)));
    for (int n = 1;; ++n) {
        const QString candidate = n == 1 ? QStringLiteral("%1 (%2)%3").arg(base, device, QLatin1String(kSuffix))
                                         : QStringLiteral("%1 (%2 %3)%4").arg(base, device).arg(n).arg(QLatin1String(kSuffix));
        if (!taken(candidate))
            return {candidate, false};
    }
}

std::optional<QString> PhoneFiles::resolve(const QString &root, const QString &name)
{
    if (name.isEmpty() || name.size() > 1024 || !name.endsWith(QLatin1String(kSuffix), Qt::CaseInsensitive))
        return std::nullopt;
    if (name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char(':')))
        return std::nullopt;
    for (const QChar c : name) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7f)
            return std::nullopt;
    }
    const QStringList parts = name.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part.startsWith(QLatin1Char('.')) || part.trimmed() != part)
            return std::nullopt;
    }
    const QString base = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    const QString path = QDir::cleanPath(base + QLatin1Char('/') + name);
    if (!path.startsWith(base + QLatin1Char('/')))
        return std::nullopt;
    return path;
}
