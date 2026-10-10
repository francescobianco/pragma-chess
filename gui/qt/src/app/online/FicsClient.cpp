#include "FicsClient.h"

#include <QCoreApplication>
#include <QTcpSocket>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(FicsClient)
};

/// What the server puts before a line when it shows its prompt.
const QString kPrompt = QStringLiteral("fics% ");

/// The square's index in Style12::squares (a8 first) for "e2".
int squareIndex(QStringView square)
{
    const int file = square.at(0).unicode() - 'a';
    const int rank = square.at(1).unicode() - '1';
    return (7 - rank) * 8 + file;
}

} // namespace

FicsClient::FicsClient(const QString &username, const QString &password, QObject *parent)
    : OnlineClient(parent)
    , m_socket(new QTcpSocket(this))
    , m_username(username.trimmed().isEmpty() ? QStringLiteral("guest") : username.trimmed())
    , m_password(password)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &FicsClient::read);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this] {
        const bool playing = isPlaying();
        Q_EMIT failed(Text::tr("freechess.org: %1").arg(m_socket->errorString()));
        if (playing) {
            // The session is gone, and the game with it.
            m_game.status = QStringLiteral("aborted");
            Q_EMIT gameFinished(m_game);
        }
    });
}

FicsClient::~FicsClient()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write("quit\n");
        m_socket->flush();
    }
    m_socket->disconnect(this);
    m_socket->abort();
}

void FicsClient::setServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
}

void FicsClient::connectToServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        m_socket->connectToHost(m_host, m_port);
}

void FicsClient::seek(const Seek &seek)
{
    if (isPlaying())
        return;
    m_game = OnlineGame();
    m_creating.reset();
    m_seek = seek;
    Q_EMIT seeking();
    if (m_loggedIn)
        sendSeek();
    else
        connectToServer(); // The seek goes once logged in.
}

void FicsClient::sendSeek()
{
    if (!m_seek)
        return;
    // Guests play unrated games only.
    const bool rated = m_seek->rated && !m_handle.startsWith(QLatin1String("Guest"));
    QString command = QStringLiteral("seek %1 %2 %3").arg(m_seek->minutes).arg(m_seek->increment).arg(rated ? QStringLiteral("rated") : QStringLiteral("unrated"));
    if (m_seek->color == QLatin1String("white") || m_seek->color == QLatin1String("black"))
        command += QLatin1Char(' ') + m_seek->color;
    send(command);
}

void FicsClient::cancelSeek()
{
    if (m_seek && m_game.id.isEmpty() && m_loggedIn)
        send(QStringLiteral("unseek"));
    m_seek.reset();
}

void FicsClient::move(const QString &uci)
{
    if (!isPlaying() || uci.size() < 4)
        return;
    const QChar piece = m_squares.size() == 64 ? m_squares.at(squareIndex(QStringView(uci).left(2))) : QChar();
    send(Fics::moveCommand(uci, piece));
    // A move answers a draw offer: no.
    m_game.whiteOffersDraw = m_game.blackOffersDraw = false;
}

void FicsClient::resign()
{
    if (isPlaying())
        send(QStringLiteral("resign"));
}

void FicsClient::abort()
{
    if (isPlaying())
        send(QStringLiteral("abort"));
}

void FicsClient::offerDraw()
{
    if (!isPlaying())
        return;
    send(QStringLiteral("draw"));
    // An offer of the user's waits for the answer; one of the opponent's is accepted.
    const bool white = m_game.white.compare(m_handle, Qt::CaseInsensitive) == 0;
    if (!(white ? m_game.blackOffersDraw : m_game.whiteOffersDraw)) {
        (white ? m_game.whiteOffersDraw : m_game.blackOffersDraw) = true;
        Q_EMIT gameUpdated(m_game);
    }
}

void FicsClient::send(const QString &command)
{
    m_socket->write(command.toLatin1() + '\n');
}

void FicsClient::read()
{
    m_buffer += m_socket->readAll();
    // Telnet's negotiations, if the server sends any, are no text.
    for (qsizetype i = m_buffer.indexOf(char(0xff)); i >= 0; i = m_buffer.indexOf(char(0xff)))
        m_buffer.remove(i, qMin<qsizetype>(3, m_buffer.size() - i));
    for (qsizetype end = m_buffer.indexOf('\n'); end >= 0; end = m_buffer.indexOf('\n')) {
        QString text = QString::fromLatin1(m_buffer.left(end));
        m_buffer.remove(0, end + 1);
        text.remove(QLatin1Char('\r'));
        while (text.startsWith(kPrompt))
            text.remove(0, kPrompt.size());
        line(text);
    }
    // A prompt waits for an answer without ending its line.
    prompt(QString::fromLatin1(m_buffer));
}

void FicsClient::prompt(const QString &pending)
{
    const QString text = pending.trimmed();
    if (m_loggedIn || text.isEmpty())
        return;
    if (text.endsWith(QLatin1String("login:"))) {
        m_buffer.clear();
        send(m_username);
    } else if (text.endsWith(QLatin1String("password:"))) {
        m_buffer.clear();
        if (m_password.isEmpty()) {
            Q_EMIT failed(Text::tr("freechess.org asks for the password of %1.").arg(m_username));
            m_socket->abort();
            return;
        }
        send(m_password);
    } else if (text.contains(QLatin1String("Press return to enter the server as"))) {
        // A guest, or a name nobody registered: the server says the name it gives.
        m_buffer.clear();
        send(QString());
    }
}

void FicsClient::line(const QString &text)
{
    if (!m_loggedIn) {
        // "Press return to enter the server as …" comes as a whole line.
        prompt(text);
        if (const std::optional<QString> handle = Fics::parseSessionStart(text)) {
            loggedIn(*handle);
        } else if (text.contains(QLatin1String("Invalid password"))) {
            Q_EMIT failed(Text::tr("freechess.org refused the password of %1.").arg(m_username));
            m_socket->abort();
        }
        return;
    }
    if (const std::optional<Fics::Style12> board = Fics::parseStyle12(text)) {
        update(*board);
    } else if (const std::optional<Fics::Creating> creating = Fics::parseCreating(text)) {
        m_creating = creating;
    } else if (const std::optional<Fics::GameEnd> end = Fics::parseGameEnd(text)) {
        if (QString::number(end->game) != m_game.id)
            return;
        m_game.status = end->status;
        m_game.winner = end->winner;
        m_game.whiteOffersDraw = m_game.blackOffersDraw = false;
        Q_EMIT gameUpdated(m_game);
        Q_EMIT gameFinished(m_game);
        m_seek.reset();
    } else if (const std::optional<QString> offering = Fics::parseDrawOffer(text)) {
        if (!isPlaying())
            return;
        (m_game.white.compare(*offering, Qt::CaseInsensitive) == 0 ? m_game.whiteOffersDraw : m_game.blackOffersDraw) = true;
        Q_EMIT gameUpdated(m_game);
    } else if (text.contains(QLatin1String("declines the draw request"))) {
        m_game.whiteOffersDraw = m_game.blackOffersDraw = false;
        Q_EMIT gameUpdated(m_game);
    } else if (text.startsWith(QLatin1String("Illegal move"))) {
        // The window plays the user's move at once: the game as the server has it takes it back.
        Q_EMIT gameUpdated(m_game);
    }
}

void FicsClient::loggedIn(const QString &handle)
{
    m_loggedIn = true;
    m_handle = handle;
    // A board after every move, times in milliseconds, long lines whole, and
    // none of the chatter: the session is for one game.
    for (const char *setting : {"set style 12", "iset ms 1", "iset nowrap 1", "set seek 0", "set shout 0",
                                "set cshout 0", "set kibitz 0", "set gin 0", "set pin 0", "set bell 0",
                                "set interface Pragma Chess"})
        send(QString::fromLatin1(setting));
    sendSeek();
}

void FicsClient::update(const Fics::Style12 &board)
{
    // Only the game the user plays.
    if (!board.isPlayed() || (!m_game.id.isEmpty() && QString::number(board.game) != m_game.id))
        return;
    m_squares = board.squares;
    const bool started = m_game.id.isEmpty();
    if (started) {
        m_game = OnlineGame();
        m_game.id = QString::number(board.game);
        m_game.white = board.white;
        m_game.black = board.black;
        m_game.status = QStringLiteral("started");
        m_game.timeControl = QStringLiteral("%1+%2").arg(board.initialMinutes * 60).arg(board.incrementSeconds);
        if (m_creating && m_creating->white == board.white) {
            m_game.whiteRating = m_creating->whiteRating;
            m_game.blackRating = m_creating->blackRating;
            m_game.rated = m_creating->rated;
        }
        m_seek.reset();
    }
    // The moves as the server counts them: one more is the move just played,
    // fewer a take-back.
    const int plies = board.plies();
    if (plies == m_game.moves.size() + 1) {
        const QString uci = Fics::uciOfVerbose(board.verboseMove, !board.whiteToMove);
        if (!uci.isEmpty()) {
            m_game.moves << uci;
            m_game.whiteOffersDraw = m_game.blackOffersDraw = false;
        }
    } else if (plies < m_game.moves.size()) {
        m_game.moves.resize(plies);
    }
    m_game.whiteTimeMs = board.whiteTimeMs;
    m_game.blackTimeMs = board.blackTimeMs;
    if (started)
        Q_EMIT gameStarted(m_game);
    Q_EMIT gameUpdated(m_game);
}
