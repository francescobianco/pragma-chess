#include "GameFilterProxyModel.h"

#include "app/GameDatabase.h"

void GameFilterProxyModel::setDatabase(const GameDatabase *database)
{
    m_database = database;
    m_predicate = {};
    invalidateFilter();
}

void GameFilterProxyModel::setPredicate(const Predicate &predicate)
{
    // Qt 6.4 has no beginFilterChange(); invalidateFilter() re-evaluates every row.
    m_predicate = predicate;
    invalidateFilter();
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (!m_predicate || !m_database || sourceRow >= m_database->gameCount())
        return true;
    return m_predicate(m_database->header(sourceRow));
}
