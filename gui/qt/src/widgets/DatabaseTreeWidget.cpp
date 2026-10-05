#include "DatabaseTreeWidget.h"

#include "PaddedItemDelegate.h"

#include "app/DatabaseOutline.h"
#include "app/GameDatabase.h"
#include "app/sources/SourceCatalog.h"
#include "platform/SymbolicIcons.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>
#include <QTimer>

namespace {

constexpr int kNodeRole = Qt::UserRole;
/// ECO letter or code, event, year, study or chapter key, or source id.
constexpr int kValueRole = Qt::UserRole + 1;

} // namespace

DatabaseTreeWidget::DatabaseTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
    , m_refreshTimer(new QTimer(this))
{
    setHeaderHidden(true);
    setColumnCount(2);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setUniformRowHeights(true);
    // A little room between the entries, the least that keeps them apart.
    setItemDelegate(new PaddedItemDelegate(2, 2, this));
    setAccessibleName(tr("Database"));
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(700);
    connect(m_refreshTimer, &QTimer::timeout, this, &DatabaseTreeWidget::refresh);
    connect(this, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current) { onCurrentItemChanged(current); });
}

DatabaseTreeWidget::Node DatabaseTreeWidget::nodeOf(const QTreeWidgetItem *item)
{
    return Node(item->data(0, kNodeRole).toInt());
}

QString DatabaseTreeWidget::keyOf(const QTreeWidgetItem *item)
{
    return QStringLiteral("%1:%2").arg(item->data(0, kNodeRole).toInt()).arg(item->data(0, kValueRole).toString());
}

void DatabaseTreeWidget::setDatabase(const GameDatabase *database)
{
    m_database = database;
    m_positionCount = m_variantCount = -1;
    clear();
    refresh();
    if (topLevelItemCount() > 0) {
        const bool wasRefreshing = m_refreshing;
        m_refreshing = true; // The caller already shows all games.
        setCurrentItem(topLevelItem(0));
        m_refreshing = wasRefreshing;
    }
}

void DatabaseTreeWidget::scheduleRefresh()
{
    if (!m_refreshTimer->isActive())
        m_refreshTimer->start();
}

void DatabaseTreeWidget::setBoardCounts(int position, int variant)
{
    m_positionCount = position;
    m_variantCount = variant;
    showBoardCounts();
}

void DatabaseTreeWidget::showBoardCounts()
{
    const QLocale locale;
    const auto show = [&](QTreeWidgetItem *item, int count) {
        if (!item)
            return;
        item->setText(1, count < 0 ? QStringLiteral("…") : locale.toString(count));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    };
    show(m_positionItem, m_positionCount);
    show(m_variantItem, m_variantCount);
}

void DatabaseTreeWidget::refresh()
{
    m_refreshTimer->stop();

    // Keep what the user selected and opened.
    const QString selectedKey = currentItem() ? keyOf(currentItem()) : QString();
    QSet<QString> expanded;
    for (QTreeWidgetItemIterator it(this); *it; ++it) {
        if ((*it)->isExpanded())
            expanded.insert(keyOf(*it));
    }
    const bool firstFill = topLevelItemCount() == 0;

    m_refreshing = true;
    clear();
    m_positionItem = m_variantItem = nullptr;
    if (!m_database) {
        m_refreshing = false;
        return;
    }

    const QLocale locale;
    const auto addItem = [&](QTreeWidgetItem *parent, Node node, const QString &text, const QVariant &value, int count) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
        item->setText(0, text);
        item->setData(0, kNodeRole, int(node));
        item->setData(0, kValueRole, value);
        if (count >= 0) {
            item->setText(1, locale.toString(count));
            item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (keyOf(item) == selectedKey)
            setCurrentItem(item);
        item->setExpanded(expanded.contains(keyOf(item)));
        return item;
    };

    const PlayerRoles roles = m_database->playerRoles();
    DatabaseOutline outline;
    QMap<PlayerRole, int> roleGames;
    int liveGames = 0;
    int trashedGames = 0;
    int recentlyTrashed = 0;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (qint64 index = 0; index < m_database->gameCount(); ++index) {
        const GameRecord header = m_database->header(index);
        if (header.state == GameState::Trashed) {
            ++trashedGames;
            recentlyTrashed += GameStates::isRecent(header.stateModified, now);
        }
        if (header.state != GameState::Live)
            continue;
        ++liveGames;
        outline.add(header, roles);
        for (auto role = outline.players.cbegin(); role != outline.players.cend(); ++role) {
            if (DatabaseOutline::hasRole(header, roles, role.key()))
                ++roleGames[role.key()];
        }
    }

    QTreeWidgetItem *root = addItem(nullptr, Node::Database, m_database->name(), QVariant(), liveGames);
    root->setIcon(0, SymbolicIcons::icon(QStringLiteral("pragma-database")));
    root->setToolTip(0, m_database->location());
    QFont bold = root->font(0);
    bold.setBold(true);
    root->setFont(0, bold);
    root->setExpanded(true);

    // Views that follow the board come first.
    QTreeWidgetItem *board = addItem(root, Node::Board, tr("Board"), QVariant(), -1);
    m_positionItem = addItem(board, Node::Position, tr("Position"), QVariant(), -1);
    m_positionItem->setToolTip(0, tr("Games in which the position on the board occurs, in any move order"));
    m_variantItem = addItem(board, Node::Variant, tr("Variant"), QVariant(), -1);
    m_variantItem->setToolTip(0, tr("Games that begin with exactly the moves played to reach the board"));
    showBoardCounts();

    const std::pair<PlayerRole, QString> roleGroups[] = {
        {PlayerRole::Me, tr("Me")}, {PlayerRole::Friend, tr("Friends")}, {PlayerRole::Opponent, tr("Opponents")}};
    for (const auto &[role, title] : roleGroups) {
        const QMap<QString, int> players = outline.players.value(role);
        if (players.isEmpty())
            continue;
        QTreeWidgetItem *group = addItem(root, Node::Role, title, playerRoleKey(role), roleGames.value(role));
        for (auto player = players.cbegin(); player != players.cend(); ++player)
            addItem(group, Node::Player, player.key(), player.key(), player.value());
    }
    if (!outline.eco.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::EcoGroup, tr("ECO"), QVariant(), -1);
        for (auto letter = outline.eco.cbegin(); letter != outline.eco.cend(); ++letter) {
            int games = 0;
            for (int count : letter.value())
                games += count;
            QTreeWidgetItem *letterItem = addItem(group, Node::EcoLetter, letter.key(), letter.key(), games);
            for (auto code = letter.value().cbegin(); code != letter.value().cend(); ++code)
                addItem(letterItem, Node::Eco, code.key(), code.key(), code.value());
        }
    }
    if (!outline.events.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Tournaments, tr("Tournaments"), QVariant(), -1);
        for (auto event = outline.events.cbegin(); event != outline.events.cend(); ++event) {
            QTreeWidgetItem *item = addItem(group, Node::Event, event.key(), event.key(), event.value());
            item->setToolTip(0, event.key());
        }
    }
    if (!outline.years.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Years, tr("Years"), QVariant(), -1);
        // Most recent first.
        for (auto year = outline.years.cend(); year != outline.years.cbegin();) {
            --year;
            addItem(group, Node::Year, QString::number(year.key()), year.key(), year.value());
        }
    }
    // The studies the games came from, each with its chapters as the study has them.
    if (!outline.studies.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Studies, tr("Studies"), QVariant(), -1);
        for (const DatabaseOutline::Study &study : outline.studies) {
            QTreeWidgetItem *studyItem = addItem(group, Node::Study, study.name, study.key, study.games);
            studyItem->setToolTip(0, study.name);
            for (const DatabaseOutline::StudyChapter &chapter : study.chapters)
                addItem(studyItem, Node::StudyChapter, chapter.name, chapter.key, chapter.games)->setToolTip(0, chapter.name);
        }
    }
    const QList<GameSource> sources = m_database->sources();
    if (!sources.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Sources, tr("Sources"), QVariant(), -1);
        for (const GameSource &source : sources) {
            QTreeWidgetItem *item = addItem(group, Node::Source, SourceCatalog::displayName(source), source.id,
                                            int(source.importedGames));
            if (!source.lastError.isEmpty()) {
                item->setIcon(0, SymbolicIcons::icon(QStringLiteral("help-about")));
                item->setToolTip(0, source.lastError);
            }
        }
        if (firstFill)
            group->setExpanded(true);
    }
    // Always there, and last: where the games taken out of the lists wait.
    QTreeWidgetItem *trash = addItem(root, Node::Trash, tr("Trash"), QVariant(), trashedGames);
    trash->setToolTip(0, tr("Games put in the trash: restore them, or delete them from here"));
    QTreeWidgetItem *recent = addItem(trash, Node::TrashRecent, tr("Recent"), QVariant(), recentlyTrashed);
    recent->setToolTip(0, tr("Put in the trash in the last %n day(s)", nullptr, GameStates::kRecentDays));
    QTreeWidgetItem *old = addItem(trash, Node::TrashOld, tr("Old"), QVariant(), trashedGames - recentlyTrashed);
    old->setToolTip(0, tr("In the trash for %n day(s) or more", nullptr, GameStates::kRecentDays));
    root->setExpanded(true);
    m_refreshing = false;
}

void DatabaseTreeWidget::onCurrentItemChanged(QTreeWidgetItem *current)
{
    if (m_refreshing || !current)
        return;
    GameCategory category;
    const QVariant value = current->data(0, kValueRole);
    switch (nodeOf(current)) {
    case Node::Position:
        category.kind = GameCategory::Kind::Position;
        break;
    case Node::Variant:
        category.kind = GameCategory::Kind::Variant;
        break;
    case Node::Database:
    case Node::Board:
    case Node::EcoGroup:
    case Node::Tournaments:
    case Node::Years:
    case Node::Studies:
    case Node::Sources:
        break;
    case Node::Role:
        category = {GameCategory::Kind::Role, value.toString()};
        break;
    case Node::Player:
        category = {GameCategory::Kind::Player, value.toString()};
        break;
    case Node::EcoLetter:
        category = {GameCategory::Kind::EcoLetter, value.toString()};
        break;
    case Node::Eco:
        category = {GameCategory::Kind::Eco, value.toString()};
        break;
    case Node::Event:
        category = {GameCategory::Kind::Event, value.toString()};
        break;
    case Node::Year:
        category = {GameCategory::Kind::Year, value.toString()};
        break;
    case Node::Study:
        category = {GameCategory::Kind::Study, value.toString()};
        break;
    case Node::StudyChapter:
        category = {GameCategory::Kind::StudyChapter, value.toString()};
        break;
    case Node::Source:
        category = {GameCategory::Kind::Source, QString(), value.toLongLong()};
        break;
    case Node::Trash:
        category.kind = GameCategory::Kind::Trash;
        break;
    case Node::TrashRecent:
        category.kind = GameCategory::Kind::TrashRecent;
        break;
    case Node::TrashOld:
        category.kind = GameCategory::Kind::TrashOld;
        break;
    }
    Q_EMIT categorySelected(category);
}

void DatabaseTreeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    const QTreeWidgetItem *item = itemAt(viewport()->mapFromGlobal(event->globalPos()));
    if (!item || !m_database)
        return;
    QMenu menu(this);
    if (nodeOf(item) == Node::Source) {
        const qint64 id = item->data(0, kValueRole).toLongLong();
        menu.addAction(tr("S&ync Now"), this, [this, id] { Q_EMIT syncSourceRequested(id); });
        menu.addSeparator();
    }
    menu.addAction(tr("Connect &Source…"), this, &DatabaseTreeWidget::connectSourceRequested);
    menu.addAction(tr("&Manage Sources…"), this, &DatabaseTreeWidget::manageSourcesRequested);
    menu.exec(event->globalPos());
}
