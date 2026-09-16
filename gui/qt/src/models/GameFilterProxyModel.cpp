#include "GameFilterProxyModel.h"

#include "GameListModel.h"

void GameFilterProxyModel::setGameIds(const std::optional<QSet<qint64>> &ids)
{
    // Qt 6.4 has no beginFilterChange(); invalidateFilter() re-evaluates every row.
    m_ids = ids;
    invalidateFilter();
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_ids)
        return true;
    const QModelIndex number = sourceModel()->index(sourceRow, GameListModel::Number, sourceParent);
    return m_ids->contains(number.data().toLongLong());
}
