#include "GameFilterProxyModel.h"

#include "GameListModel.h"
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
    // Not invalidateFilter(): it takes the rows out range by range, and the
    // view pays for each — seconds when a filter leaves half of 100 000 games.
    // invalidate() maps the rows again at once and keeps the selection.
    change();
    invalidate();
#endif
}

void GameFilterProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel())
        sourceModel()->disconnect(this);
    if (!model) {
        QSortFilterProxyModel::setSourceModel(model);
        return;
    }
    // Keys read before a change no longer hold: connected before the proxy's
    // own handlers, which sort the changed rows again at once.
    const auto forget = [this] {
        m_keys.clear();
        m_haveKey.clear();
    };
    connect(model, &QAbstractItemModel::modelReset, this, forget);
    connect(model, &QAbstractItemModel::layoutChanged, this, forget);
    connect(model, &QAbstractItemModel::rowsRemoved, this, forget);
    connect(model, &QAbstractItemModel::rowsInserted, this, forget);
    connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                for (int row = topLeft.row(); row <= bottomRight.row() && row < int(m_haveKey.size()); ++row)
                    m_haveKey[row] = false;
            });
    QSortFilterProxyModel::setSourceModel(model);
}

bool GameFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *games = qobject_cast<const GameListModel *>(sourceModel());
    if (!games)
        return QSortFilterProxyModel::lessThan(left, right);
    const int column = left.column();
    // The rows are in the order of the games' ids: the number sorts by row.
    if (column == GameListModel::Number)
        return left.row() < right.row();
    if (column != m_keysColumn) {
        m_keys.clear();
        m_haveKey.clear();
        m_keysColumn = column;
    }
    if (m_keys.size() != size_t(games->rowCount())) {
        m_keys.assign(games->rowCount(), QVariant());
        m_haveKey.assign(games->rowCount(), false);
    }
    const auto key = [&](int row) -> const QVariant & {
        if (!m_haveKey[row]) {
            m_keys[row] = games->sortKey(row, column);
            m_haveKey[row] = true;
        }
        return m_keys[row];
    };
    const QVariant &a = key(left.row());
    const QVariant &b = key(right.row());
    // As QSortFilterProxyModel compares: nothing goes last.
    if (!a.isValid())
        return false;
    if (!b.isValid())
        return true;
    if (a.typeId() == QMetaType::QString || b.typeId() == QMetaType::QString) {
        const QString x = a.toString();
        const QString y = b.toString();
        return isSortLocaleAware() ? QString::localeAwareCompare(x, y) < 0 : QString::compare(x, y, sortCaseSensitivity()) < 0;
    }
    return a.toLongLong() < b.toLongLong();
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    if (!m_database || sourceRow >= m_database->gameCount())
        return true;
    // The brief header: cheap for every game, whatever the database's size.
    return m_database->stateOf(sourceRow) == m_state && (!m_predicate || m_predicate(m_database->brief(sourceRow)));
}
