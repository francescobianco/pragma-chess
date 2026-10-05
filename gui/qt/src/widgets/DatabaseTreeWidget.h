#pragma once

#include <QSet>
#include <QTreeWidget>

class GameDatabase;
class QTimer;

/// A part of the database chosen in the tree.
struct GameCategory {
    /// Position and Variant follow the board: the games reaching the position
    /// on it (in any move order) and those beginning with the moves played.
    /// Trash lists the games put in the trash, which no other part shows;
    /// TrashRecent those put there in the last GameStates::kRecentDays days,
    /// TrashOld the others. Study and StudyChapter are the games of a study
    /// and of one of its chapters (DatabaseOutline::studyKey, chapterKey).
    enum class Kind { All, Position, Variant, Role, Player, EcoLetter, Eco, Event, Year, Study, StudyChapter, Source, Trash, TrashRecent, TrashOld };
    Kind kind = Kind::All;
    /// Role key ("me", "friend", "opponent"), player name, ECO letter or code,
    /// event name, year, or study or chapter key.
    QString value;
    qint64 sourceId = 0;

    bool operator==(const GameCategory &) const = default;
};

/// Navigation next to the games list: the open database and, under it, the
/// Board views Position and Variant (the games that match the board), the
/// games of the user, friends and opponents, its games by ECO code, tournament
/// and year, the studies they came from with their chapters, the sources it
/// syncs with and, last, the Trash.
/// Only values some game actually has are listed, with their game counts;
/// games in the trash count only there.
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
    /// Games matching the board for Position and Variant; -1 while they are
    /// still being counted.
    void setBoardCounts(int position, int variant);

Q_SIGNALS:
    void categorySelected(const GameCategory &category);
    void connectSourceRequested();
    void manageSourcesRequested();
    void syncSourceRequested(qint64 sourceId);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum class Node { Database, Board, Position, Variant, Role, Player, EcoGroup, EcoLetter, Eco, Tournaments, Event, Years, Year, Studies, Study, StudyChapter, Sources, Source, Trash, TrashRecent, TrashOld };

    void onCurrentItemChanged(QTreeWidgetItem *current);
    static Node nodeOf(const QTreeWidgetItem *item);
    static QString keyOf(const QTreeWidgetItem *item);
    void showBoardCounts();

    const GameDatabase *m_database = nullptr;
    bool m_refreshing = false;
    int m_positionCount = -1;
    int m_variantCount = -1;
    QTreeWidgetItem *m_positionItem = nullptr;
    QTreeWidgetItem *m_variantItem = nullptr;
    QTimer *m_refreshTimer;
};
