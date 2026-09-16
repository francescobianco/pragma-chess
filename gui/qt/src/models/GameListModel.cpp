#include "GameListModel.h"

#include "app/GameDatabase.h"

GameListModel::GameListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void GameListModel::setDatabase(const GameDatabase *database)
{
    beginResetModel();
    m_database = database;
    endResetModel();
}

void GameListModel::refreshRow(int row)
{
    Q_EMIT dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

int GameListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_database)
        return 0;
    return int(m_database->gameCount());
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
        case Moves:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case Result:
            return QVariant(Qt::AlignCenter);
        default:
            return {};
        }
    }

    if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
        return {};

    // TODO: cache headers once databases hold millions of games.
    const GameRecord game = m_database->header(index.row());
    const auto elo = [](int value) { return value > 0 ? QVariant(value) : QVariant(); };
    switch (index.column()) {
    case Number: return game.id;
    case White: return game.white;
    case WhiteElo: return elo(game.whiteElo);
    case Black: return game.black;
    case BlackElo: return elo(game.blackElo);
    case Result: return game.result;
    case Date: return game.date;
    case Event: return game.event;
    case Site: return game.site;
    case Eco: return game.eco;
    case Moves: return (game.plyCount + 1) / 2;
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
    default: return {};
    }
}
