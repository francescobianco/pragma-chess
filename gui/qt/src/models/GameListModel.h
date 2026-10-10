#pragma once

#include "app/StandInNames.h"

#include <QAbstractTableModel>
#include <QSet>

class GameDatabase;

/// Table of game headers for a database. Rows map 1:1 to database indices.
class GameListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { Number, White, WhiteElo, Black, BlackElo, Result, Date, Event, Site, Eco, Moves, Line, ColumnCount };

    explicit GameListModel(QObject *parent = nullptr);

    /// What a column is called in the files that remember it ("white-elo"):
    /// never translated, never renumbered.
    static QString columnKey(int column);
    /// The column's name in full, for menus: the two "Elo" headers are
    /// "White Elo" and "Black Elo" there.
    static QString columnName(int column);

    void setDatabase(const GameDatabase *database);
    /// Reads again who the players are (Who Is This?): the names of "me"
    /// are shown in bold, and the database's type (a training database
    /// shows no Line). Called when a role or the properties change.
    void refreshRoles();
    /// The names shown for the players a training database leaves unnamed
    /// (StandInNames): the user's, and the trainer's.
    void setStandInNames(const StandInNames &names);
    /// Call after the header of the game in `row` changed in the database.
    void refreshRow(int row);
    /// Call after games were appended to the database.
    void refreshAppended();

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    const GameDatabase *m_database = nullptr;
    QSet<QString> m_me; // The players who are the user, in bold.
    bool m_training = false; // A training database: the Line column stays empty, stand-in names.
    StandInNames m_standIns;
    int m_rows = 0;
};
