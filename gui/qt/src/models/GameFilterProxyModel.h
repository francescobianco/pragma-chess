#pragma once

#include <QSet>
#include <QSortFilterProxyModel>

#include <optional>

/// Sorts the games list and, when a set of game ids is given (e.g. the games
/// of one source), shows only those games.
class GameFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    /// Database ids to show, or nothing for all games.
    void setGameIds(const std::optional<QSet<qint64>> &ids);
    bool isFiltered() const { return m_ids.has_value(); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    std::optional<QSet<qint64>> m_ids;
};
