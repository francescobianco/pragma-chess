#pragma once

#include <QSet>
#include <QTreeWidget>

class GameDatabase;
class QTimer;

/// A part of the database chosen in the tree.
struct GameCategory {
    enum class Kind { All, Role, Player, EcoLetter, Eco, Event, Year, Source };
    Kind kind = Kind::All;
    /// Role key ("me", "friend", "opponent"), player name, ECO letter or code, event name or year.
    QString value;
    qint64 sourceId = 0;

    bool operator==(const GameCategory &) const = default;
};

/// Navigation next to the games list: the open database and, under it, the
/// games of the user, friends and opponents, its games by ECO code, tournament
/// and year, and the sources it syncs with.
/// Only values some game actually has are listed, with their game counts.
class DatabaseTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit DatabaseTreeWidget(QWidget *parent = nullptr);

    /// Shows a database (may be null) with all its games selected.
    void setDatabase(const GameDatabase *database);
    /// Recounts the games, keeping the selection and the expanded nodes.
    void refresh();
    /// Refreshes shortly, coalescing bursts (e.g. games arriving from a sync).
    void scheduleRefresh();

Q_SIGNALS:
    void categorySelected(const GameCategory &category);
    void connectSourceRequested();
    void manageSourcesRequested();
    void syncSourceRequested(qint64 sourceId);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum class Node { Database, Role, Player, EcoGroup, EcoLetter, Eco, Tournaments, Event, Years, Year, Sources, Source };

    void onCurrentItemChanged(QTreeWidgetItem *current);
    static Node nodeOf(const QTreeWidgetItem *item);
    static QString keyOf(const QTreeWidgetItem *item);

    const GameDatabase *m_database = nullptr;
    bool m_refreshing = false;
    QTimer *m_refreshTimer;
};
