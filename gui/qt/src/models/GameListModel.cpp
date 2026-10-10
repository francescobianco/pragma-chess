#include "GameListModel.h"

#include "app/ChessPosition.h"
#include "app/DatabaseProperties.h"
#include "app/GameDatabase.h"
#include "app/PlayerRole.h"

#include <QFont>

GameListModel::GameListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void GameListModel::setDatabase(const GameDatabase *database)
{
    beginResetModel();
    m_database = database;
    m_rows = database ? int(database->gameCount()) : 0;
    m_me.clear();
    // Puzzles and exercises: their moves are the solution, not to be shown.
    m_training = m_database && m_database->properties().type == DatabaseType::Training;
    if (m_database) {
        const PlayerRoles roles = m_database->playerRoles();
        for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
            if (it.value() == PlayerRole::Me)
                m_me.insert(it.key());
        }
    }
    endResetModel();
}

void GameListModel::setStandInNames(const StandInNames &names)
{
    if (names.trainee == m_standIns.trainee && names.trainer == m_standIns.trainer)
        return;
    m_standIns = names;
    if (m_training && m_rows > 0)
        Q_EMIT dataChanged(index(0, White), index(m_rows - 1, Black));
}

void GameListModel::refreshRoles()
{
    setDatabase(m_database); // Reads the roles again; the list is short enough to reset.
}

void GameListModel::refreshRow(int row)
{
    Q_EMIT dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

void GameListModel::refreshAppended()
{
    const int count = m_database ? int(m_database->gameCount()) : 0;
    if (count <= m_rows)
        return;
    beginInsertRows(QModelIndex(), m_rows, count - 1);
    m_rows = count;
    endInsertRows();
}

int GameListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows;
}

int GameListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant GameListModel::data(const QModelIndex &index, int role) const
{
    if (!m_database || !index.isValid())
        return {};

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case Number:
        case WhiteElo:
        case BlackElo:
        case Result:
        case Date:
        case Eco:
        case Moves:
            return QVariant(Qt::AlignCenter);
        default:
            return {};
        }
    }

    if (role == Qt::FontRole) {
        // The user's own name stands out: bold wherever "me" plays, and on
        // the side the user plays in a training database.
        if (index.column() != White && index.column() != Black)
            return {};
        const Side side = index.column() == White ? Side::White : Side::Black;
        if (m_me.isEmpty() && !m_training)
            return {};
        const GameRecord game = m_database->header(index.row());
        const QString &own = side == Side::White ? game.white : game.black;
        const bool trainee = m_training && StandInNames::isUnnamed(own) && side == StandInNames::traineeSide(game);
        if (!trainee && !m_me.contains(own))
            return {};
        QFont font;
        font.setBold(true);
        return font;
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};

    // TODO: cache headers once databases hold millions of games.
    const GameRecord game = m_database->header(index.row());
    const auto elo = [](int value) { return value > 0 ? QVariant(value) : QVariant(); };
    switch (index.column()) {
    case Number: return game.id;
    case White: return m_training ? m_standIns.name(game, Side::White) : game.white;
    case WhiteElo: return elo(game.whiteElo);
    case Black: return m_training ? m_standIns.name(game, Side::Black) : game.black;
    case BlackElo: return elo(game.blackElo);
    case Result: return game.result;
    case Date: return game.date;
    case Event: return game.event;
    case Site: return game.site;
    case Eco: return game.eco;
    case Moves: return (game.plyCount + 1) / 2;
    case Line: return m_training ? QVariant() : figurineLine(game.linePreview);
    default: return {};
    }
}

QString GameListModel::columnKey(int column)
{
    switch (column) {
    case Number: return QStringLiteral("number");
    case White: return QStringLiteral("white");
    case WhiteElo: return QStringLiteral("white-elo");
    case Black: return QStringLiteral("black");
    case BlackElo: return QStringLiteral("black-elo");
    case Result: return QStringLiteral("result");
    case Date: return QStringLiteral("date");
    case Event: return QStringLiteral("event");
    case Site: return QStringLiteral("site");
    case Eco: return QStringLiteral("eco");
    case Moves: return QStringLiteral("moves");
    case Line: return QStringLiteral("line");
    default: return {};
    }
}

QString GameListModel::columnName(int column)
{
    switch (column) {
    case Number: return tr("Number");
    case White: return tr("White");
    case WhiteElo: return tr("White Elo");
    case Black: return tr("Black");
    case BlackElo: return tr("Black Elo");
    case Result: return tr("Result");
    case Date: return tr("Date");
    case Event: return tr("Event");
    case Site: return tr("Site");
    case Eco: return tr("ECO");
    case Moves: return tr("Moves");
    case Line: return tr("Line");
    default: return {};
    }
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Number: return tr("#");
    case White: return tr("White");
    case WhiteElo: return tr("Elo");
    case Black: return tr("Black");
    case BlackElo: return tr("Elo");
    case Result: return tr("Result");
    case Date: return tr("Date");
    case Event: return tr("Event");
    case Site: return tr("Site");
    case Eco: return tr("ECO");
    case Moves: return tr("Moves");
    case Line: return tr("Line");
    default: return {};
    }
}
