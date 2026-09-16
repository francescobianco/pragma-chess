#include "MoveListModel.h"

#include "app/GameSession.h"

MoveListModel::MoveListModel(GameSession *session, QObject *parent)
    : QAbstractTableModel(parent)
    , m_session(session)
{
    connect(m_session, &GameSession::gameChanged, this, [this] {
        beginResetModel();
        endResetModel();
    });
}

int MoveListModel::blackStartsOffset() const
{
    return m_session->initialBoard().sideToMove() == Side::Black ? 1 : 0;
}

int MoveListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return (m_session->plyCount() + blackStartsOffset() + 1) / 2;
}

int MoveListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 2;
}

int MoveListModel::plyForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return -1;
    const int ply = index.row() * 2 + index.column() - blackStartsOffset() + 1;
    return ply >= 1 && ply <= m_session->plyCount() ? ply : -1;
}

QModelIndex MoveListModel::indexForPly(int ply) const
{
    if (ply < 1 || ply > m_session->plyCount())
        return {};
    const int cell = ply - 1 + blackStartsOffset();
    return index(cell / 2, cell % 2);
}

QVariant MoveListModel::data(const QModelIndex &index, int role) const
{
    const int ply = plyForIndex(index);
    if (ply < 0)
        return role == Qt::DisplayRole && index.isValid() ? QVariant(QStringLiteral("…")) : QVariant();
    if (role == Qt::DisplayRole)
        return m_session->game().moves.at(ply - 1).san;
    return {};
}

QVariant MoveListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return QStringLiteral("%1.").arg(section + 1);
    return section == 0 ? tr("White") : tr("Black");
}

Qt::ItemFlags MoveListModel::flags(const QModelIndex &index) const
{
    if (plyForIndex(index) < 0)
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
