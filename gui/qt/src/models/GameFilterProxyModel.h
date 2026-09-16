#pragma once

#include "app/GameRecord.h"

#include <QSortFilterProxyModel>

#include <functional>

class GameDatabase;

/// Sorts the games list and, with a predicate on the game headers (an ECO
/// code, an event, a year, the games of a source…), shows only those games.
class GameFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using Predicate = std::function<bool(const GameRecord &header)>;

    using QSortFilterProxyModel::QSortFilterProxyModel;

    /// The database whose headers the predicate is given; rows map 1:1 to its indices.
    void setDatabase(const GameDatabase *database);
    /// Games to show, or an empty predicate for all games.
    void setPredicate(const Predicate &predicate);
    bool isFiltered() const { return bool(m_predicate); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    const GameDatabase *m_database = nullptr;
    Predicate m_predicate;
};
