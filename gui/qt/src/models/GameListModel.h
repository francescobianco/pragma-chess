#pragma once

#include <QAbstractTableModel>

class GameDatabase;

/// Table of game headers for a database. Rows map 1:1 to database indices.
class GameListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Number, White, WhiteElo, Black, BlackElo, Result, Date, Event, Site, Eco, Moves, ColumnCount };

    explicit GameListModel(QObject *parent = nullptr);

    void setDatabase(const GameDatabase *database);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    const GameDatabase *m_database = nullptr;
};
