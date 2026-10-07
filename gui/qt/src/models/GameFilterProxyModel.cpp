#include "GameFilterProxyModel.h"

#include "app/GameDatabase.h"

void GameFilterProxyModel::setDatabase(const GameDatabase *database)
{
    changeFilter([&] {
        m_database = database;
        m_predicate = {};
        m_state = GameState::Live;
    });
}

void GameFilterProxyModel::setPredicate(const Predicate &predicate, GameState state)
{
    changeFilter([&] {
        m_predicate = predicate;
        m_state = state;
    });
}

void GameFilterProxyModel::changeFilter(const std::function<void()> &change)
{
    // Qt 6.10 brackets the change (invalidateFilter() is deprecated there);
    // Qt 6.4 has no beginFilterChange(): every row is evaluated again after it.
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    change();
    endFilterChange(Direction::Rows);
#else
    change();
    invalidateFilter();
#endif
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (!m_database || sourceRow >= m_database->gameCount())
        return true;
    const GameRecord header = m_database->header(sourceRow);
    return header.state == m_state && (!m_predicate || m_predicate(header));
}
