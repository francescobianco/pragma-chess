#include "TorneiOnlineFetch.h"

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace {

const QString kSite = QStringLiteral("torneionline.com");
const QString kBaseUrl = QStringLiteral("https://www.torneionline.com/");
/// Pause between two pages, to be gentle with a site that has no API.
constexpr int kPauseMs = 1000;
/// Results of a tournament can still change for a while after it ends.
constexpr int kSettleDays = 7;

using Options = QRegularExpression::PatternOptions;
constexpr Options kHtml = QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption;

QString decodeEntities(const QString &text)
{
    static const QRegularExpression entity(QStringLiteral("&(#x[0-9a-f]+|#[0-9]+|[a-z0-9]+);?"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString decoded;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = entity.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1).toLower();
        QString replacement;
        if (name.startsWith(QLatin1String("#x")))
            replacement = QChar(char16_t(name.mid(2).toUInt(nullptr, 16)));
        else if (name.startsWith(QLatin1Char('#')))
            replacement = QChar(char16_t(name.mid(1).toUInt()));
        else if (name == QLatin1String("nbsp"))
            replacement = QStringLiteral(" ");
        else if (name == QLatin1String("amp"))
            replacement = QStringLiteral("&");
        else if (name == QLatin1String("quot"))
            replacement = QStringLiteral("\"");
        else if (name == QLatin1String("apos"))
            replacement = QStringLiteral("'");
        else if (name == QLatin1String("lt"))
            replacement = QStringLiteral("<");
        else if (name == QLatin1String("gt"))
            replacement = QStringLiteral(">");
        else if (name == QLatin1String("frac12"))
            replacement = QStringLiteral("½");
        else
            continue;
        decoded += QStringView(text).mid(last, match.capturedStart() - last);
        decoded += replacement;
        last = match.capturedEnd();
    }
    decoded += QStringView(text).mid(last);
    return decoded;
}

/// The text of an HTML fragment, on one line.
QString plainText(const QString &html)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    QString text = html;
    text.replace(tag, QStringLiteral(" "));
    return decodeEntities(text).simplified();
}

/// The table rows of a page. Rows are not always closed, so a row runs to the next.
QStringList rows(const QString &page)
{
    static const QRegularExpression rowStart(QStringLiteral("<tr[\\s>]"), QRegularExpression::CaseInsensitiveOption);
    QStringList result = page.split(rowStart);
    if (!result.isEmpty())
        result.removeFirst();
    return result;
}

QStringList cells(const QString &row)
{
    static const QRegularExpression cell(QStringLiteral("<td[^>]*>(.*?)</td>"), kHtml);
    QStringList result;
    QRegularExpressionMatchIterator it = cell.globalMatch(row);
    while (it.hasNext())
        result << plainText(it.next().captured(1));
    return result;
}

QDate siteDate(const QString &text)
{
    return QDate::fromString(text, QStringLiteral("dd-MM-yyyy"));
}

bool isNumber(const QString &text)
{
    static const QRegularExpression digits(QStringLiteral("^\\d+$"));
    return digits.match(text).hasMatch();
}

/// The player's score in a game: 1, 0.5 or 0; nothing when not played yet.
std::optional<double> score(const QString &text)
{
    if (text == QLatin1String("1"))
        return 1.0;
    if (text == QLatin1String("0"))
        return 0.0;
    if (text == QStringLiteral("½") || text == QLatin1String("0.5") || text == QLatin1String("="))
        return 0.5;
    return std::nullopt;
}

} // namespace

TorneiOnlineFetch::TorneiOnlineFetch(const GameSource &source, QNetworkAccessManager *network, QObject *parent)
    : SourceFetch(parent)
    , m_source(source)
    , m_network(network)
    , m_pause(new QTimer(this))
    , m_state(source.state)
{
    m_pause->setSingleShot(true);
    m_pause->setInterval(kPauseMs);
    connect(m_pause, &QTimer::timeout, this, [this] {
        if (const std::function<void()> send = std::exchange(m_pending, {}))
            send();
    });
}

QUrl TorneiOnlineFetch::searchUrl(const QString &idType, const QString &id)
{
    const QString field = idType == QLatin1String("fsi") ? QStringLiteral("ifsi") : QStringLiteral("ifid");
    return QUrl(kBaseUrl + QStringLiteral("giocatori.php?tipo=1&%1=%2")
                               .arg(field, QString::fromLatin1(QUrl::toPercentEncoding(id.trimmed()))));
}

QString TorneiOnlineFetch::decodePage(const QByteArray &page)
{
    QString text;
    text.reserve(page.size());
    const auto *bytes = reinterpret_cast<const unsigned char *>(page.constData());
    const qsizetype size = page.size();
    for (qsizetype i = 0; i < size;) {
        const unsigned char lead = bytes[i];
        const int length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
        bool utf8 = length > 1 && i + length <= size;
        for (int k = 1; utf8 && k < length; ++k)
            utf8 = (bytes[i + k] & 0xC0) == 0x80;
        if (length == 1) {
            text += QChar(lead);
            ++i;
        } else if (utf8) {
            text += QString::fromUtf8(page.constData() + i, length);
            i += length;
        } else {
            text += QChar(lead); // Latin-1
            ++i;
        }
    }
    return text;
}

std::optional<TorneiOnlineFetch::Player> TorneiOnlineFetch::parsePlayerSearch(const QString &page, const QString &id)
{
    static const QRegularExpression link(QStringLiteral("giocatori_d\\.php\\?progre=(\\d+)&tipo=a>(.*?)</a>"), kHtml);
    const QString wanted = id.trimmed();
    for (const QString &row : rows(page)) {
        const QRegularExpressionMatch match = link.match(row);
        if (!match.hasMatch() || !cells(row).contains(wanted))
            continue;
        return Player{match.captured(1), plainText(match.captured(2))};
    }
    return std::nullopt;
}

QList<TorneiOnlineFetch::Tournament> TorneiOnlineFetch::parseTournaments(const QString &page)
{
    static const QRegularExpression link(QStringLiteral("tornei_d\\.php\\?codice=(\\w+)&tipo=p"),
                                         QRegularExpression::CaseInsensitiveOption);
    QList<Tournament> tournaments;
    for (const QString &row : rows(page)) {
        const QRegularExpressionMatch match = link.match(row);
        const QStringList values = cells(row);
        if (!match.hasMatch() || values.size() < 5)
            continue;
        Tournament tournament;
        tournament.code = match.captured(1);
        tournament.name = values.at(1);
        tournament.province = values.at(2);
        tournament.start = siteDate(values.at(3));
        tournament.end = siteDate(values.at(4));
        if (tournament.start.isValid())
            tournaments << tournament;
    }
    return tournaments;
}

QString TorneiOnlineFetch::parseParticipantNumber(const QString &page, const QString &progre)
{
    static const QRegularExpression gix(QStringLiteral("gix=(\\d+)"));
    const QString player = QStringLiteral("progre=%1&").arg(progre);
    for (const QString &row : rows(page)) {
        if (row.contains(player))
            return gix.match(row).captured(1);
    }
    return QString();
}

QList<ImportedGame> TorneiOnlineFetch::parseScoreCard(const QString &page, const Tournament &tournament)
{
    static const QRegularExpression heading(QStringLiteral("<b>\\s*\\d+\\s*-\\s*(.*?)(?:&nbsp;|<)"), kHtml);
    static const QRegularExpression rating(QStringLiteral("Elo (?:FIDE|Italia):\\s*<b>\\s*(\\d+)"), kHtml);

    qsizetype cardStart = page.indexOf(QLatin1String("Cartellini giocatori"), 0, Qt::CaseInsensitive);
    const QString card = page.mid(qMax<qsizetype>(cardStart, 0));
    const QString player = plainText(heading.match(card).captured(1));
    if (player.isEmpty())
        return {};
    const int playerElo = rating.match(card).captured(1).toInt();

    QList<ImportedGame> games;
    for (const QString &row : rows(card)) {
        const QStringList values = cells(row);
        // T, C, Num, flag, Cat, opponent, Fed, Elo FIDE, Elo Italia, Diff, Exp, Ris, F, …
        if (values.size() < 14 || !isNumber(values.at(1)))
            continue;
        const QString opponent = values.at(6);
        const bool forfeit = !values.at(13).isEmpty();
        const std::optional<double> points = score(values.at(12));
        if (opponent.isEmpty() || forfeit || !points)
            continue; // A bye, a game not played or not finished yet.

        int opponentElo = values.at(8).toInt();
        if (opponentElo == 0)
            opponentElo = values.at(9).toInt();
        const bool playerIsBlack = values.at(2) == QLatin1String("N");

        ImportedGame imported;
        imported.externalId = QStringLiteral("%1/%2").arg(tournament.code, values.at(1));
        GameRecord &record = imported.game;
        record.white = playerIsBlack ? opponent : player;
        record.black = playerIsBlack ? player : opponent;
        record.whiteElo = playerIsBlack ? opponentElo : playerElo;
        record.blackElo = playerIsBlack ? playerElo : opponentElo;
        record.event = tournament.name;
        record.site = kBaseUrl + QStringLiteral("tornei_d.php?codice=%1&tipo=p").arg(tournament.code);
        record.date = tournament.start.toString(QStringLiteral("yyyy.MM.dd"));
        record.round = values.at(1);
        const double whitePoints = playerIsBlack ? 1.0 - *points : *points;
        record.result = whitePoints == 1.0   ? QStringLiteral("1-0")
                        : whitePoints == 0.0 ? QStringLiteral("0-1")
                                             : QStringLiteral("1/2-1/2");
        games << imported;
    }
    return games;
}

void TorneiOnlineFetch::start()
{
    if (!m_state.value(QStringLiteral("progre")).toString().isEmpty()) {
        fetchTournaments();
        return;
    }
    const QString idType = m_source.settings.value(QLatin1String(TorneiOnlineSettings::idType)).toString();
    get(searchUrl(idType, m_source.account), [this, idType](const QString &page) {
        const std::optional<Player> player = parsePlayerSearch(page, m_source.account);
        if (!player) {
            Q_EMIT finished(tr("%1 has no player with %2 ID %3.")
                                .arg(kSite, idType == QLatin1String("fsi") ? QStringLiteral("FSI") : QStringLiteral("FIDE"),
                                     m_source.account));
            return;
        }
        m_state.insert(QStringLiteral("progre"), player->progre);
        fetchTournaments();
    });
}

void TorneiOnlineFetch::abort()
{
    m_pause->stop();
    m_pending = {};
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_tournaments.clear();
}

void TorneiOnlineFetch::fetchTournaments()
{
    const QString progre = m_state.value(QStringLiteral("progre")).toString();
    const QUrl url(kBaseUrl + QStringLiteral("giocatori_d.php?progre=%1&tipo=t").arg(progre));
    get(url, [this](const QString &page) {
        if (!page.contains(QLatin1String("Tornei disputati"))) {
            Q_EMIT finished(tr("Could not read the tournaments of the player on %1.").arg(kSite));
            return;
        }
        const QDate since = QDate::fromString(
            m_source.settings.value(QLatin1String(SourceSettings::since)).toString(), Qt::ISODate);
        const QJsonArray done = m_state.value(QStringLiteral("done")).toArray();
        for (const Tournament &tournament : parseTournaments(page)) {
            if (since.isValid() && tournament.start < since)
                continue;
            if (done.contains(tournament.code))
                continue;
            m_tournaments << tournament;
        }
        std::stable_sort(m_tournaments.begin(), m_tournaments.end(),
                         [](const Tournament &a, const Tournament &b) { return a.start < b.start; });
        fetchNextTournament();
    });
}

void TorneiOnlineFetch::fetchNextTournament()
{
    if (m_tournaments.isEmpty()) {
        Q_EMIT gamesFetched({}, m_state); // Keeps the player number found by the search.
        Q_EMIT finished(QString());
        return;
    }
    const Tournament tournament = m_tournaments.takeFirst();
    const QString progre = m_state.value(QStringLiteral("progre")).toString();
    const QUrl participants(kBaseUrl + QStringLiteral("tornei_d.php?codice=%1&tipo=p&ord=n&sen=a").arg(tournament.code));

    const auto store = [this, tournament](const QList<ImportedGame> &games) {
        const QDate end = tournament.end.isValid() ? tournament.end : tournament.start;
        if (end.addDays(kSettleDays) < QDate::currentDate()) {
            QJsonArray done = m_state.value(QStringLiteral("done")).toArray();
            done.append(tournament.code);
            m_state.insert(QStringLiteral("done"), done);
        }
        Q_EMIT gamesFetched(games, m_state);
        fetchNextTournament();
    };

    get(participants, [this, tournament, progre, store](const QString &page) {
        const QString number = parseParticipantNumber(page, progre);
        if (number.isEmpty()) {
            store({});
            return;
        }
        const QUrl scoreCard(kBaseUrl + QStringLiteral("tornei_d.php?codice=%1&gix=%2&tipo=g&ord=u&sen=a")
                                            .arg(tournament.code, number));
        get(scoreCard, [tournament, store](const QString &card) { store(parseScoreCard(card, tournament)); });
    });
}

void TorneiOnlineFetch::get(const QUrl &url, std::function<void(const QString &)> handle)
{
    m_pending = [this, url, handle] {
        m_requested = true;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
        request.setTransferTimeout(30000);
        m_reply = m_network->get(request);
        connect(m_reply, &QNetworkReply::finished, this, [this, handle] {
            QNetworkReply *reply = m_reply;
            m_reply = nullptr;
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                Q_EMIT finished(httpError(reply, kSite));
                return;
            }
            handle(decodePage(reply->readAll()));
        });
    };
    if (m_requested) {
        m_pause->start();
    } else {
        const std::function<void()> send = std::exchange(m_pending, {});
        send();
    }
}
