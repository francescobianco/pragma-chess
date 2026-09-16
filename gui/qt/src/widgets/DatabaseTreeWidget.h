#pragma once

#include <QTreeWidget>

class GameDatabase;

/// Navigation next to the games list: the databases of the user's folder
/// and, under the open one, all its games and the sources it syncs with.
class DatabaseTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit DatabaseTreeWidget(QWidget *parent = nullptr);

    /// The open database, shown expanded (may be null).
    void setDatabase(const GameDatabase *database);
    /// Rescans the databases folder and the sources, keeping the selection.
    void refresh();
    /// Selects "All Games" of the open database.
    void selectAllGames();

Q_SIGNALS:
    void databaseRequested(const QString &path);
    void allGamesSelected();
    void sourceSelected(qint64 sourceId);
    void connectSourceRequested();
    void manageSourcesRequested();
    void syncSourceRequested(qint64 sourceId);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum class Node { Database, AllGames, Sources, Source };

    void onCurrentItemChanged(QTreeWidgetItem *current);
    static Node nodeOf(const QTreeWidgetItem *item);

    const GameDatabase *m_database = nullptr;
    bool m_refreshing = false;
};
