#include "DatabaseTreeWidget.h"

#include "PaddedItemDelegate.h"

#include "app/DatabaseOutline.h"
#include "app/TimeControl.h"
#include "app/TrainingSets.h"
#include "app/UiLanguage.h"
#include "app/GameDatabase.h"
#include "app/sources/SourceCatalog.h"
#include "platform/SymbolicIcons.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QFileInfo>
#include <QPainter>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>
#include <QTimer>

namespace {

constexpr int kNodeRole = Qt::UserRole;
/// ECO letter or code, event, year, study or chapter key, or source id.
constexpr int kValueRole = Qt::UserRole + 1;
/// What follows the text in the normal weight: the root's "(games.pdb)".
constexpr int kAfterRole = Qt::UserRole + 2;

/// The tree's cells, padded; the root's name, bold, followed by its file in
/// the normal weight (an item has one font: the rest is drawn here).
class TreeDelegate : public PaddedItemDelegate {
public:
    explicit TreeDelegate(QObject *parent)
        : PaddedItemDelegate(kPadding, kPadding, parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        PaddedItemDelegate::paint(painter, option, index);
        const QString after = index.data(kAfterRole).toString();
        if (after.isEmpty())
            return;
        QStyleOptionViewItem cell = option;
        initStyleOption(&cell, index);
        cell.rect.adjust(kPadding, kPadding, -kPadding, -kPadding);
        const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
        const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &cell, cell.widget);
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, cell.widget) + 1;
        const int used = QFontMetrics(cell.font).horizontalAdvance(cell.text) + margin;
        QFont normal = cell.font;
        normal.setBold(false);
        const QFontMetrics metrics(normal);
        const QRect rest(text.left() + used + metrics.horizontalAdvance(QLatin1Char(' ')), text.top(),
                         text.right() - text.left() - used, text.height());
        if (rest.width() <= 0)
            return;
        painter->save();
        painter->setFont(normal);
        painter->setPen(cell.palette.color(cell.state & QStyle::State_Selected ? QPalette::HighlightedText
                                                                               : QPalette::Text));
        painter->drawText(rest, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(after, Qt::ElideRight, rest.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = PaddedItemDelegate::sizeHint(option, index);
        if (const QString after = index.data(kAfterRole).toString(); !after.isEmpty())
            size.rwidth() += QFontMetrics(option.font).horizontalAdvance(QLatin1Char(' ') + after);
        return size;
    }

private:
    static constexpr int kPadding = 2;
};

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
    setItemDelegate(new TreeDelegate(this));
    setAccessibleName(tr("Database"));
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(700);
    connect(m_refreshTimer, &QTimer::timeout, this, &DatabaseTreeWidget::refresh);
    connect(this, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current) { onCurrentItemChanged(current); });
    const auto changed = [this] {
        if (!m_refreshing)
            Q_EMIT stateChanged();
    };
    connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        if (item && item->childCount() == 0)
            Q_EMIT leafActivated();
    });
    connect(this, &QTreeWidget::itemExpanded, this, changed);
    connect(this, &QTreeWidget::itemCollapsed, this, changed);
    connect(this, &QTreeWidget::currentItemChanged, this, changed);
}

QStringList DatabaseTreeWidget::expandedKeys() const
{
    QStringList keys;
    for (QTreeWidgetItemIterator it(const_cast<DatabaseTreeWidget *>(this)); *it; ++it) {
        if ((*it)->isExpanded())
            keys << keyOf(*it);
    }
    return keys;
}

QString DatabaseTreeWidget::selectedKey() const
{
    return currentItem() ? keyOf(currentItem()) : QString();
}

void DatabaseTreeWidget::restoreState(const QStringList &expanded, const QString &selected)
{
    QTreeWidgetItem *chosen = nullptr;
    {
        const bool wasRefreshing = m_refreshing;
        m_refreshing = true; // Nothing to tell yet: this is how it was.
        const QSet<QString> open(expanded.cbegin(), expanded.cend());
        for (QTreeWidgetItemIterator it(this); *it; ++it) {
            const QString key = keyOf(*it);
            (*it)->setExpanded(open.contains(key));
            if (key == selected)
                chosen = *it;
        }
        m_refreshing = wasRefreshing;
    }
    if (chosen && chosen != currentItem())
        setCurrentItem(chosen); // Filters the list, as a click does.
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

    // The name given in Database Settings, bold, then the file; or the file alone.
    const QString fileBaseName = QFileInfo(m_database->location()).completeBaseName();
    const QString given = m_database->properties().givenName(UiLanguage::effective(), fileBaseName);
    QTreeWidgetItem *root =
        addItem(nullptr, Node::Database, given.isEmpty() ? m_database->name() : given, QVariant(), liveGames);
    if (!given.isEmpty())
        root->setData(0, kAfterRole, QStringLiteral("(%1)").arg(QFileInfo(m_database->location()).fileName()));
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
        {PlayerRole::Me, tr("Me")}, {PlayerRole::Friend, tr("Friend")}, {PlayerRole::Opponent, tr("Opponent")}};
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
        QTreeWidgetItem *group = addItem(root, Node::Tournaments, tr("Tournament/Event"), QVariant(), -1);
        for (auto event = outline.events.cbegin(); event != outline.events.cend(); ++event) {
            QTreeWidgetItem *item = addItem(group, Node::Event, event.key(), event.key(), event.value());
            item->setToolTip(0, event.key());
        }
    }
    if (!outline.years.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Years, tr("Year"), QVariant(), -1);
        // Most recent first.
        for (auto year = outline.years.cend(); year != outline.years.cbegin();) {
            --year;
            addItem(group, Node::Year, QString::number(year.key()), year.key(), year.value());
        }
    }
    // The time controls the games were played at, from the fastest.
    if (!outline.timeControls.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::TimeControls, tr("Time Control"), QVariant(), -1);
        QStringList values = outline.timeControls.keys();
        std::sort(values.begin(), values.end(), [](const QString &a, const QString &b) {
            const qint64 left = TimeControl::estimatedSeconds(a);
            const qint64 right = TimeControl::estimatedSeconds(b);
            return left != right ? left < right : a < b;
        });
        for (const QString &value : std::as_const(values))
            addItem(group, Node::TimeControl, TimeControl::label(value), value, outline.timeControls.value(value))
                ->setToolTip(0, value);
    }
    // The studies the games came from, each with its chapters as the study has them.
    if (!outline.studies.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Studies, tr("Study"), QVariant(), -1);
        for (const DatabaseOutline::Study &study : outline.studies) {
            QTreeWidgetItem *studyItem = addItem(group, Node::Study, study.name, study.key, study.games);
            studyItem->setToolTip(0, study.name);
            for (const DatabaseOutline::StudyChapter &chapter : study.chapters)
                addItem(studyItem, Node::StudyChapter, chapter.name, chapter.key, chapter.games)->setToolTip(0, chapter.name);
        }
    }
    // Training: endgames by family and material, tactics by theme.
    if (!outline.endgames.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Endgames, tr("Endgames"), QVariant(), -1);
        for (const QString &family : TrainingSets::endgameFamilies()) {
            if (!outline.endgames.contains(family))
                continue;
            const QMap<QString, int> &endgames = outline.endgames.value(family);
            int games = 0;
            for (const int count : endgames)
                games += count;
            QTreeWidgetItem *familyItem =
                addItem(group, Node::EndgameFamily, TrainingSets::endgameFamilyName(family), family, games);
            // The most common material first.
            QStringList keys = endgames.keys();
            std::stable_sort(keys.begin(), keys.end(),
                             [&](const QString &a, const QString &b) { return endgames.value(a) > endgames.value(b); });
            for (const QString &endgame : std::as_const(keys))
                addItem(familyItem, Node::Endgame, TrainingSets::endgameName(endgame), endgame, endgames.value(endgame));
        }
    }
    if (!outline.tactics.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Tactics, tr("Tactics"), QVariant(), -1);
        for (const QString &theme : TrainingSets::tacticThemes()) {
            if (outline.tactics.contains(theme))
                addItem(group, Node::Tactic, TrainingSets::tacticName(theme), theme, outline.tactics.value(theme));
        }
    }
    const QList<GameSource> sources = m_database->sources();
    if (!sources.isEmpty()) {
        QTreeWidgetItem *group = addItem(root, Node::Sources, tr("Source"), QVariant(), -1);
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
    case Node::TimeControls:
    case Node::Studies:
    case Node::Sources:
    case Node::Endgames:
    case Node::Tactics:
        break;
    case Node::EndgameFamily:
        category = {GameCategory::Kind::EndgameFamily, value.toString()};
        break;
    case Node::Endgame:
        category = {GameCategory::Kind::Endgame, value.toString()};
        break;
    case Node::Tactic:
        category = {GameCategory::Kind::Tactic, value.toString()};
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
    case Node::TimeControl:
        category = {GameCategory::Kind::TimeControl, value.toString()};
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
    if (nodeOf(item) == Node::Database) {
        // The database itself: its name, description and columns.
        menu.addAction(tr("Database &Settings…"), this, &DatabaseTreeWidget::settingsRequested);
        menu.addSeparator();
    }
    if (nodeOf(item) == Node::Source) {
        const qint64 id = item->data(0, kValueRole).toLongLong();
        menu.addAction(tr("S&ync Now"), this, [this, id] { Q_EMIT syncSourceRequested(id); });
        menu.addSeparator();
    }
    menu.addAction(tr("Connect &Source…"), this, &DatabaseTreeWidget::connectSourceRequested);
    menu.addAction(tr("&Manage Sources…"), this, &DatabaseTreeWidget::manageSourcesRequested);
    menu.exec(event->globalPos());
}
