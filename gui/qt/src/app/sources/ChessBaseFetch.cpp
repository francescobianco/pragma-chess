#include "ChessBaseFetch.h"

#include "app/chessbase/ChessBaseDatabase.h"

#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace {

/// Games handed over at a time: the database stores them and moves the
/// cursor between batches, so a long import survives being stopped.
constexpr int kBatch = 200;

} // namespace

ChessBaseFetch::ChessBaseFetch(const GameSource &source, QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_next(source.state.value(QLatin1String(ChessBaseSettings::read)).toInt())
{
}

QString ChessBaseFetch::path(const GameSource &source)
{
    return source.settings.value(QLatin1String(ChessBaseSettings::path)).toString();
}

bool ChessBaseFetch::isAvailable(const GameSource &source)
{
    const QString file = path(source);
    return !file.isEmpty() && QFileInfo::exists(file);
}

void ChessBaseFetch::start()
{
    if (!isAvailable(m_source)) {
        Q_EMIT finished(tr("The ChessBase database was not found at %1.").arg(path(m_source)));
        return;
    }
    QTimer::singleShot(0, this, &ChessBaseFetch::readBatch);
}

void ChessBaseFetch::abort()
{
    m_aborted = true;
}

void ChessBaseFetch::readBatch()
{
    if (m_aborted)
        return;
    QString error;
    const std::unique_ptr<ChessBaseDatabase> database = ChessBaseDatabase::open(path(m_source), &error);
    if (!database) {
        Q_EMIT finished(error);
        return;
    }
    QList<ImportedGame> games;
    const int end = qMin(database->count(), m_next + kBatch);
    for (; m_next < end; ++m_next) {
        const ChessBaseDatabase::Entry entry = database->entry(m_next);
        if (!entry.isGame || entry.deleted)
            continue;
        QString ignored; // A game whose moves cannot be read keeps what was read.
        games << ImportedGame{QString::number(m_next), database->game(m_next, &ignored)};
    }
    QJsonObject state;
    state.insert(QLatin1String(ChessBaseSettings::read), m_next);
    if (!games.isEmpty() || m_next > m_source.state.value(QLatin1String(ChessBaseSettings::read)).toInt())
        Q_EMIT gamesFetched(games, state);
    if (m_next >= database->count()) {
        Q_EMIT finished(QString());
        return;
    }
    // Between batches the application breathes.
    QTimer::singleShot(0, this, &ChessBaseFetch::readBatch);
}
