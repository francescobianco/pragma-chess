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
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("size"), size},
        {QStringLiteral("sha256"), sha256},
        {QStringLiteral("modified"), modified.toUTC().toString(Qt::ISODate)},
    };
}

PhoneFiles::PhoneFiles(QString root)
    : m_root(std::move(root))
{
}

QList<PhoneFiles::Entry> PhoneFiles::list()
{
    QList<Entry> entries;
    const QDir root(m_root);
    QDirIterator it(m_root, {QStringLiteral("*") + QLatin1String(kSuffix)}, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        const QString name = root.relativeFilePath(info.absoluteFilePath());
        if (!resolve(m_root, name))
            continue; // Hidden folders and the like.
        Hashed &hashed = m_hashes[name];
        if (hashed.sha256.isEmpty() || hashed.size != info.size() || hashed.modified != info.lastModified()) {
            hashed = {info.size(), info.lastModified(), sha256Of(info.absoluteFilePath())};
            if (hashed.sha256.isEmpty())
                continue;
        }
        entries.append({name, hashed.size, hashed.sha256, hashed.modified});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    return entries;
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
