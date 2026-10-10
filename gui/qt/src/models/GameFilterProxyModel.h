#pragma once

#include "app/GameRecord.h"

#include <QSortFilterProxyModel>

#include <functional>
#include <vector>

class GameDatabase;

/// Sorts the games list and, with a predicate on the game headers (an ECO
/// code, an event, a year, the games of a source…), shows only those games.
/// Only games in one state are ever listed: the live ones, or the trash.
class GameFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using Predicate = std::function<bool(const GameRecord &header)>;

    using QSortFilterProxyModel::QSortFilterProxyModel;

    /// The database whose headers the predicate is given; rows map 1:1 to its indices.
    void setDatabase(const GameDatabase *database);
    /// Games to show among those in `state` (Live, or Trashed for the trash),
    /// or an empty predicate for all of them.
    void setPredicate(const Predicate &predicate, GameState state = GameState::Live);
    bool isFiltered() const { return bool(m_predicate); }
    GameState state() const { return m_state; }

    void setSourceModel(QAbstractItemModel *model) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    /// By GameListModel::sortKey, each row's read once and kept while the
    /// column sorts: never every row's text through data().
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    /// Runs `change` to the filter's state and filters the rows again.
    void changeFilter(const std::function<void()> &change);

    const GameDatabase *m_database = nullptr;
    Predicate m_predicate;
    GameState m_state = GameState::Live;
    /// The sort keys of m_keysColumn by source row, read when first compared.
    mutable std::vector<QVariant> m_keys;
    mutable std::vector<bool> m_haveKey;
    mutable int m_keysColumn = -1;
};
