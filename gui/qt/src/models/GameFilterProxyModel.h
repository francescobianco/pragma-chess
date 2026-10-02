#pragma once

#include "app/GameRecord.h"

#include <QSortFilterProxyModel>

#include <functional>

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

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    const GameDatabase *m_database = nullptr;
    Predicate m_predicate;
    GameState m_state = GameState::Live;
};
