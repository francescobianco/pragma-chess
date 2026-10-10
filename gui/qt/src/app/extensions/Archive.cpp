#include "Archive.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <miniz.h>

namespace Archive {

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(Archive)
};

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

/// Where an entry goes inside `folder`, or empty for one that would leave it.
QString target(const QString &folder, QString name)
{
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (name.startsWith(QLatin1Char('/')) || QDir::isAbsolutePath(name))
        return {};
    const QString cleaned = QDir::cleanPath(name);
    if (cleaned == QLatin1String("..") || cleaned.startsWith(QLatin1String("../")) || cleaned.isEmpty())
        return {};
    return QDir(folder).filePath(cleaned);
}

bool extractZip(const QString &file, const QString &folder, QString *error)
{
    mz_zip_archive zip{};
    const QByteArray path = QFile::encodeName(file);
    if (!mz_zip_reader_init_file(&zip, path.constData(), 0)) {
        setError(error, Text::tr("The archive cannot be read."));
        return false;
    }
    bool ok = true;
    for (mz_uint i = 0; ok && i < mz_zip_reader_get_num_files(&zip); ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            ok = false;
            break;
        }
        const QString out = target(folder, QString::fromUtf8(stat.m_filename));
        if (out.isEmpty()) {
            setError(error, Text::tr("The archive holds a file outside its folder: %1").arg(QString::fromUtf8(stat.m_filename)));
            ok = false;
            break;
        }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            QDir().mkpath(out);
            continue;
        }
        QDir().mkpath(QFileInfo(out).absolutePath());
        ok = mz_zip_reader_extract_to_file(&zip, i, QFile::encodeName(out).constData(), 0);
        if (!ok)
            setError(error, Text::tr("Could not extract %1.").arg(QString::fromUtf8(stat.m_filename)));
    }
    mz_zip_reader_end(&zip);
    return ok;
}

/// Inflates the gzip file `in` into `out`.
bool gunzip(QFile &in, QFile &out, QString *error)
{
    const QByteArray head = in.read(10);
    if (head.size() < 10 || uchar(head[0]) != 0x1f || uchar(head[1]) != 0x8b || head[2] != 8) {
        setError(error, Text::tr("The archive is not gzip."));
        return false;
    }
    const uchar flags = uchar(head[3]);
    if (flags & 4) { // FEXTRA
        const QByteArray length = in.read(2);
        if (length.size() == 2)
            in.read(uchar(length[0]) | uchar(length[1]) << 8);
    }
    for (const uchar flag : {uchar(8), uchar(16)}) { // FNAME, FCOMMENT: ended by a zero
        if (flags & flag) {
            char c = 1;
            while (in.getChar(&c) && c != 0) {
            }
        }
    }
    if (flags & 2) // FHCRC
        in.read(2);

    mz_stream stream{};
    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
        return false;
    QByteArray input;
    QByteArray output(1 << 20, Qt::Uninitialized);
    int status = MZ_OK;
    while (status != MZ_STREAM_END) {
        if (stream.avail_in == 0) {
            input = in.read(1 << 20);
            if (input.isEmpty())
                break;
            stream.next_in = reinterpret_cast<const unsigned char *>(input.constData());
            stream.avail_in = mz_uint32(input.size());
        }
        stream.next_out = reinterpret_cast<unsigned char *>(output.data());
        stream.avail_out = mz_uint32(output.size());
        status = mz_inflate(&stream, MZ_NO_FLUSH);
        if (status != MZ_OK && status != MZ_STREAM_END) {
            mz_inflateEnd(&stream);
            setError(error, Text::tr("The archive is damaged."));
            return false;
        }
        out.write(output.constData(), output.size() - stream.avail_out);
    }
    mz_inflateEnd(&stream);
    if (status != MZ_STREAM_END) {
        setError(error, Text::tr("The archive is cut short."));
        return false;
    }
    return true;
}

qint64 octal(const char *field, int size)
{
    qint64 value = 0;
    for (int i = 0; i < size && field[i]; ++i) {
        if (field[i] >= '0' && field[i] <= '7')
            value = value * 8 + (field[i] - '0');
    }
    return value;
}

bool extractTar(QFile &tar, const QString &folder, QString *error)
{
    QString longName; // GNU's ././@LongLink: the next entry's name.
    for (;;) {
        const QByteArray header = tar.read(512);
        if (header.size() < 512 || header.count('\0') == 512)
            return true; // The end: two empty blocks, or the file's end.
        const char *h = header.constData();
        QString name = longName.isEmpty() ? QString::fromUtf8(h, int(qstrnlen(h, 100))) : longName;
        longName.clear();
        if (QByteArray(h + 257, 5) == "ustar" && h[345]) {
            const QString prefix = QString::fromUtf8(h + 345, int(qstrnlen(h + 345, 155)));
            name = prefix + QLatin1Char('/') + name;
        }
        const qint64 size = octal(h + 124, 12);
        const char type = h[156];
        const qint64 padded = (size + 511) / 512 * 512;
        if (type == 'L') {
            longName = QString::fromUtf8(tar.read(padded)).left(int(size)).section(QLatin1Char('\0'), 0, 0);
            continue;
        }
        if (type != '0' && type != '\0' && type != '5') { // Links and the rest are not needed.
            tar.skip(padded);
            continue;
        }
        const QString out = target(folder, name);
        if (out.isEmpty()) {
            setError(error, Text::tr("The archive holds a file outside its folder: %1").arg(name));
            return false;
        }
        if (type == '5') {
            QDir().mkpath(out);
            continue;
        }
        QDir().mkpath(QFileInfo(out).absolutePath());
        QFile file(out);
        if (!file.open(QIODevice::WriteOnly)) {
            setError(error, Text::tr("Could not write %1.").arg(out));
            return false;
        }
        for (qint64 left = size; left > 0;) {
            const QByteArray chunk = tar.read(qMin<qint64>(left, 1 << 20));
            if (chunk.isEmpty()) {
                setError(error, Text::tr("The archive is cut short."));
                return false;
            }
            file.write(chunk);
            left -= chunk.size();
        }
        tar.skip(padded - size);
        // Executable in the archive, executable here (an engine).
        if (octal(h + 100, 8) & 0111)
            file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    }
}

} // namespace

Kind kindOf(const QString &name)
{
    const QString path = name.section(QLatin1Char('?'), 0, 0).section(QLatin1Char('#'), 0, 0).toLower();
    if (path.endsWith(QLatin1String(".tar.gz")) || path.endsWith(QLatin1String(".tgz")))
        return Kind::TarGz;
    if (path.endsWith(QLatin1String(".zip")))
        return Kind::Zip;
    if (path.endsWith(QLatin1String(".tar")))
        return Kind::Tar;
    return Kind::None;
}

bool extract(const QString &file, Kind kind, const QString &folder, QString *error, const QString &plainName)
{
    if (!QDir().mkpath(folder)) {
        setError(error, Text::tr("Could not make the folder %1.").arg(folder));
        return false;
    }
    switch (kind) {
    case Kind::Zip:
        return extractZip(file, folder, error);
    case Kind::Tar: {
        QFile tar(file);
        if (!tar.open(QIODevice::ReadOnly)) {
            setError(error, tar.errorString());
            return false;
        }
        return extractTar(tar, folder, error);
    }
    case Kind::TarGz: {
        QFile in(file);
        QTemporaryFile tar;
        if (!in.open(QIODevice::ReadOnly) || !tar.open()) {
            setError(error, in.errorString());
            return false;
        }
        if (!gunzip(in, tar, error))
            return false;
        tar.seek(0);
        return extractTar(tar, folder, error);
    }
    case Kind::None: {
        const QString out = target(folder, plainName.isEmpty() ? QFileInfo(file).fileName() : plainName);
        QFile::remove(out);
        if (out.isEmpty() || !QFile::copy(file, out)) {
            setError(error, Text::tr("Could not write %1.").arg(out));
            return false;
        }
        return true;
    }
    }
    return false;
}

} // namespace Archive
