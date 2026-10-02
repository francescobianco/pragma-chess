#include "GameFilterProxyModel.h"

#include "app/GameDatabase.h"

void GameFilterProxyModel::setDatabase(const GameDatabase *database)
{
    m_database = database;
    m_predicate = {};
    m_state = GameState::Live;
    invalidateFilter();
}

void GameFilterProxyModel::setPredicate(const Predicate &predicate, GameState state)
{
    // Qt 6.4 has no beginFilterChange(); invalidateFilter() re-evaluates every row.
    m_predicate = predicate;
    m_state = state;
    invalidateFilter();
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (!m_database || sourceRow >= m_database->gameCount())
        return true;
    const GameRecord header = m_database->header(sourceRow);
    return header.state == m_state && (!m_predicate || m_predicate(header));
}
