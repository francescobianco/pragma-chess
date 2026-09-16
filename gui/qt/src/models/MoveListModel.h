#pragma once

#include <QAbstractTableModel>

class GameSession;

/// Main line of the current game as a two-column (White / Black) table.
class MoveListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MoveListModel(GameSession *session, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /// Ply reached after the move in the cell, or -1 for an empty cell.
    int plyForIndex(const QModelIndex &index) const;
    QModelIndex indexForPly(int ply) const;

private:
    int blackStartsOffset() const;

    GameSession *m_session;
};
