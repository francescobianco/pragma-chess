#include "DesktopApi.h"

#include "MainWindow.h"
#include "app/GameSession.h"
#include "app/Pgn.h"
#include "app/api/LocalHttpServer.h"
#include "app/GameDatabase.h"
#include "widgets/BoardWidget.h"

#include <QAction>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>

namespace {

constexpr quint16 kDefaultPort = 7457;

using Request = LocalHttpServer::Request;
using Response = LocalHttpServer::Response;

Response json(const QJsonObject &object)
{
    return {200, "application/json", QJsonDocument(object).toJson()};
}

/// The body of a request as a JSON object; nothing if it is not one.
std::optional<QJsonObject> bodyOf(const Request &request)
{
    if (request.body.trimmed().isEmpty())
        return QJsonObject();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(request.body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

QString sideName(Side side)
{
    return side == Side::White ? QStringLiteral("white") : QStringLiteral("black");
}

QString pieceLetter(const Piece &piece)
{
    if (piece.isNull())
        return {};
    const QChar letter = QStringLiteral(" PNBRQK").at(int(piece.type));
    return piece.side == Side::White ? QString(letter) : QString(letter.toLower());
}

QString arrowKind(BoardArrow::Kind kind)
{
    switch (kind) {
    case BoardArrow::Kind::Refutation: return QStringLiteral("refutation");
    case BoardArrow::Kind::Idea: return QStringLiteral("idea");
    case BoardArrow::Kind::Reply: return QStringLiteral("reply");
    case BoardArrow::Kind::Alternative: return QStringLiteral("alternative");
    case BoardArrow::Kind::Threat: return QStringLiteral("threat");
    }
    return {};
}

QString verdictName(MoveExplanation::Verdict verdict)
{
    static const char *const names[] = {"none", "best", "good", "inaccuracy", "mistake", "blunder"};
    return QLatin1String(names[int(verdict)]);
}

QJsonObject evaluationJson(const EngineEvaluation &evaluation, const ChessPosition &position)
{
    QJsonObject object{{QStringLiteral("depth"), evaluation.depth}, {QStringLiteral("text"), evaluation.text()}};
    if (evaluation.isMate)
        object.insert(QStringLiteral("mate"), evaluation.mating == Side::White ? evaluation.mateIn : -evaluation.mateIn);
    else
        object.insert(QStringLiteral("cp"), evaluation.centipawns);
    object.insert(QStringLiteral("pv"), QJsonArray::fromStringList(evaluation.pv));
    object.insert(QStringLiteral("line"), position.lineText(evaluation.pv, 12));
    return object;
}

QJsonObject explanationJson(const MoveExplanation &explanation)
{
    QJsonArray arrows;
    for (const BoardArrow &arrow : explanation.arrows) {
        QJsonObject object{{QStringLiteral("from"), BoardState::squareName(arrow.from)},
                           {QStringLiteral("to"), BoardState::squareName(arrow.to)},
                           {QStringLiteral("kind"), arrowKind(arrow.kind)},
                           {QStringLiteral("number"), arrow.step}};
        if (!arrow.piece.isNull())
            object.insert(QStringLiteral("piece"), pieceLetter(arrow.piece));
        arrows.append(object);
    }
    QJsonArray lost;
    for (int square : explanation.lostPieces)
        lost.append(BoardState::squareName(square));
    return {{QStringLiteral("verdict"), verdictName(explanation.verdict)},
            {QStringLiteral("summary"), explanation.summary},
            {QStringLiteral("arrows"), arrows},
            {QStringLiteral("lost"), lost},
            {QStringLiteral("playback"), QJsonArray::fromStringList(explanation.playback)}};
}

} // namespace

DesktopApi::DesktopApi(MainWindow *window)
    : QObject(window)
    , m_window(window)
    , m_server(new LocalHttpServer(this))
{
    addRoutes();
}

bool DesktopApi::isListening() const
{
    return m_server->isListening();
}

quint16 DesktopApi::port() const
{
    return m_server->port();
}

bool DesktopApi::startIfAsked(QString *error)
{
    if (qEnvironmentVariable("PRAGMA_DEV_API") != QLatin1String("1"))
        return true;
    bool ok = false;
    const int asked = qEnvironmentVariableIntValue("PRAGMA_DEV_API_PORT", &ok);
    const quint16 port = ok && asked > 0 && asked < 65536 ? quint16(asked) : kDefaultPort;
    if (!m_server->start(port)) {
        if (error)
            *error = QStringLiteral("port %1 is in use (PRAGMA_DEV_API_PORT chooses another)").arg(port);
        return false;
    }
    return true;
}

void DesktopApi::addRoutes()
{
    MainWindow *w = m_window;
    const auto state = [w]() {
        GameSession *session = w->m_session;
        const ChessPosition &position = session->position();
        QJsonArray line;
        for (int ply = 1; ply <= session->plyCount(); ++ply) {
            const MoveRecord &move = session->moveAt(ply);
            line.append(QJsonObject{{QStringLiteral("ply"), ply},
                                    {QStringLiteral("san"), session->positionAt(ply - 1).moveNumberText() + move.san},
                                    {QStringLiteral("uci"), move.uci}});
        }
        QJsonArray path;
        for (int index : session->path())
            path.append(index);
        const GameRecord &game = session->game();
        QJsonObject engine{{QStringLiteral("analyzing"), w->m_startEngineAction->isChecked()},
                           {QStringLiteral("name"), w->m_engineName}};
        if (w->m_lastEvaluation.depth > 0 || !w->m_lastEvaluation.pv.isEmpty())
            engine.insert(QStringLiteral("evaluation"), evaluationJson(w->m_lastEvaluation, position));
        QJsonObject explain = explanationJson(w->m_explanation);
        explain.insert(QStringLiteral("on"), w->m_explainAction->isChecked());
        return QJsonObject{
            {QStringLiteral("fen"), position.fen()},
            {QStringLiteral("ply"), session->ply()},
            {QStringLiteral("plyCount"), session->plyCount()},
            {QStringLiteral("sideToMove"), sideName(position.sideToMove())},
            {QStringLiteral("line"), line},
            {QStringLiteral("path"), path},
            {QStringLiteral("flipped"), w->m_flipBoardAction->isChecked()},
            {QStringLiteral("training"), QJsonObject{{QStringLiteral("on"), w->m_trainingModeAction->isChecked()},
                                                     {QStringLiteral("side"), sideName(w->m_trainingSide)}}},
            {QStringLiteral("engine"), engine},
            {QStringLiteral("explain"), explain},
            {QStringLiteral("game"), QJsonObject{{QStringLiteral("white"), game.white},
                                                 {QStringLiteral("black"), game.black},
                                                 {QStringLiteral("event"), game.event},
                                                 {QStringLiteral("result"), game.result}}},
            {QStringLiteral("database"), w->m_database ? w->m_database->location() : QString()},
        };
    };

    m_server->route(QStringLiteral("GET"), QStringLiteral("/api"), [this](const Request &) {
        return json({{QStringLiteral("routes"), QJsonArray::fromStringList(m_server->routes())}});
    });
    m_server->route(QStringLiteral("GET"), QStringLiteral("/api/state"), [state](const Request &) { return json(state()); });
    m_server->route(QStringLiteral("GET"), QStringLiteral("/api/explanation"), [w](const Request &) {
        QJsonObject explain = explanationJson(w->m_explanation);
        explain.insert(QStringLiteral("on"), w->m_explainAction->isChecked());
        return json(explain);
    });
    m_server->route(QStringLiteral("GET"), QStringLiteral("/api/screenshot"), [w](const Request &) {
        // The window as it is drawn, taken from inside: no compositor permission needed.
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        w->grab().save(&buffer, "PNG");
        return Response{200, "image/png", png};
    });
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/ply"), [w, state](const Request &request) {
        const std::optional<QJsonObject> body = bodyOf(request);
        if (!body || !body->value(QStringLiteral("ply")).isDouble())
            return LocalHttpServer::error(400, QStringLiteral("expected {\"ply\": n}"));
        w->m_session->goToPly(qBound(0, body->value(QStringLiteral("ply")).toInt(), w->m_session->plyCount()));
        return json(state());
    });
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/move"), [w, state](const Request &request) {
        const std::optional<QJsonObject> body = bodyOf(request);
        const QString uci = body ? body->value(QStringLiteral("uci")).toString() : QString();
        const std::optional<ChessMove> move = w->m_session->position().moveFromUci(uci);
        if (!move)
            return LocalHttpServer::error(400, QStringLiteral("not a legal move here: \"%1\" (expected {\"uci\": \"e2e4\"})").arg(uci));
        w->playMove(*move);
        return json(state());
    });
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/line"), [w, state](const Request &request) {
        // A line of moves (PGN, SAN or UCI) as a new game, shown at its end or at "ply".
        const std::optional<QJsonObject> body = bodyOf(request);
        if (!body || !body->value(QStringLiteral("moves")).isString())
            return LocalHttpServer::error(400, QStringLiteral("expected {\"moves\": \"1.e4 e5 …\", \"fen\": optional, \"ply\": optional}"));
        QString error;
        const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(
            body->value(QStringLiteral("moves")).toString(), body->value(QStringLiteral("fen")).toString(), &error);
        if (!line)
            return LocalHttpServer::error(400, error);
        if (!w->canLeaveGame())
            return LocalHttpServer::error(409, QStringLiteral("the board is on an online game"));
        GameRecord game;
        game.startFen = line->startFen;
        game.moves = line->moves;
        game.variations = line->variations;
        game.result = QStringLiteral("*");
        w->startGame(game);
        const int plies = w->m_session->plyCount();
        w->m_session->goToPly(body->contains(QStringLiteral("ply")) ? qBound(0, body->value(QStringLiteral("ply")).toInt(), plies)
                                                                    : plies);
        return json(state());
    });
    const auto toggle = [state](QAction *action, const QString &name) {
        return [state, action, name](const Request &request) {
            const std::optional<QJsonObject> body = bodyOf(request);
            if (!body || !body->value(QStringLiteral("on")).isBool())
                return LocalHttpServer::error(400, QStringLiteral("expected {\"on\": true or false} for %1").arg(name));
            action->setChecked(body->value(QStringLiteral("on")).toBool());
            return json(state());
        };
    };
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/explain"), toggle(w->m_explainAction, QStringLiteral("Explain")));
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/analysis"),
                    toggle(w->m_startEngineAction, QStringLiteral("the analysis")));
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/flip"), toggle(w->m_flipBoardAction, QStringLiteral("the board")));
    m_server->route(QStringLiteral("POST"), QStringLiteral("/api/peek"), [w, state](const Request &request) {
        // As the Engine panel's eye held down (true) or let go (false).
        const std::optional<QJsonObject> body = bodyOf(request);
        if (!body || !body->value(QStringLiteral("on")).isBool())
            return LocalHttpServer::error(400, QStringLiteral("expected {\"on\": true or false}"));
        w->peekAtEngineLine(body->value(QStringLiteral("on")).toBool());
        return json(state());
    });
}
