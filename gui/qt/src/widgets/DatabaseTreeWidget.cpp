#include "DatabaseTreeWidget.h"

#include "app/GameDatabase.h"
#include "app/UserFolders.h"
#include "app/sources/SourceCatalog.h"
#include "platform/SymbolicIcons.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>

namespace {

constexpr int kNodeRole = Qt::UserRole;
/// Database path or source id.
constexpr int kValueRole = Qt::UserRole + 1;

} // namespace

DatabaseTreeWidget::DatabaseTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setColumnCount(2);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setUniformRowHeights(true);
    setAccessibleName(tr("Databases"));
    connect(this, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current) { onCurrentItemChanged(current); });
}

DatabaseTreeWidget::Node DatabaseTreeWidget::nodeOf(const QTreeWidgetItem *item)
{
    return Node(item->data(0, kNodeRole).toInt());
}

void DatabaseTreeWidget::setDatabase(const GameDatabase *database)
{
    m_database = database;
    refresh();
    selectAllGames();
}

void DatabaseTreeWidget::refresh()
{
    // Remember what was selected to select it again.
    std::optional<std::pair<Node, QVariant>> selected;
    if (const QTreeWidgetItem *item = currentItem())
        selected = std::make_pair(nodeOf(item), item->data(0, kValueRole));

    m_refreshing = true;
    clear();
    const QLocale locale;
    const auto addItem = [&](QTreeWidgetItem *parent, Node node, const QString &text, const QVariant &value) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
        item->setText(0, text);
        item->setData(0, kNodeRole, int(node));
        item->setData(0, kValueRole, value);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        if (selected && selected->first == node && selected->second == value)
            setCurrentItem(item);
        return item;
    };

    QStringList paths;
    const QDir folder(UserFolders::databasesDir());
    for (const QFileInfo &file : folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)},
                                                      QDir::Files, QDir::Name))
        paths << file.absoluteFilePath();
    if (m_database && !m_database->location().isEmpty() && !paths.contains(m_database->location()))
        paths.prepend(m_database->location()); // Opened from elsewhere.

    for (const QString &path : std::as_const(paths)) {
        const bool open = m_database && m_database->location() == path;
        QTreeWidgetItem *database = addItem(nullptr, Node::Database, QFileInfo(path).completeBaseName(), path);
        database->setIcon(0, SymbolicIcons::icon(QStringLiteral("folder")));
        database->setToolTip(0, QDir::toNativeSeparators(path));
        if (!open)
            continue;

        QFont bold = database->font(0);
        bold.setBold(true);
        database->setFont(0, bold);
        QTreeWidgetItem *all = addItem(database, Node::AllGames, tr("All Games"), QVariant());
        all->setText(1, locale.toString(m_database->gameCount()));

        const QList<GameSource> sources = m_database->sources();
        if (!sources.isEmpty()) {
            QTreeWidgetItem *group = addItem(database, Node::Sources, tr("Sources"), QVariant());
            for (const GameSource &source : sources) {
                QTreeWidgetItem *item = addItem(group, Node::Source, SourceCatalog::displayName(source), source.id);
                item->setText(1, locale.toString(source.importedGames));
                if (!source.lastError.isEmpty()) {
                    item->setIcon(0, SymbolicIcons::icon(QStringLiteral("help-about")));
                    item->setToolTip(0, source.lastError);
                }
            }
            group->setExpanded(true);
        }
        database->setExpanded(true);
    }
    m_refreshing = false;
}

void DatabaseTreeWidget::selectAllGames()
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *database = topLevelItem(i);
        for (int j = 0; j < database->childCount(); ++j) {
            if (nodeOf(database->child(j)) == Node::AllGames) {
                const bool wasRefreshing = m_refreshing;
                m_refreshing = true; // The caller already shows all games.
                setCurrentItem(database->child(j));
                m_refreshing = wasRefreshing;
                return;
            }
        }
    }
}

void DatabaseTreeWidget::onCurrentItemChanged(QTreeWidgetItem *current)
{
    if (m_refreshing || !current)
        return;
    switch (nodeOf(current)) {
    case Node::Database: {
        const QString path = current->data(0, kValueRole).toString();
        if (!m_database || m_database->location() != path)
            Q_EMIT databaseRequested(path);
        break;
    }
    case Node::AllGames:
    case Node::Sources:
        Q_EMIT allGamesSelected();
        break;
    case Node::Source:
        Q_EMIT sourceSelected(current->data(0, kValueRole).toLongLong());
        break;
    }
}

void DatabaseTreeWidget::contextMenuEvent(QContextMenuEvent *event)
{
    const QTreeWidgetItem *item = itemAt(viewport()->mapFromGlobal(event->globalPos()));
    if (!item || !m_database)
        return;
    const Node node = nodeOf(item);
    const bool openDatabase = node != Node::Database || item->data(0, kValueRole).toString() == m_database->location();
    if (!openDatabase)
        return;

    QMenu menu(this);
    if (node == Node::Source) {
        const qint64 id = item->data(0, kValueRole).toLongLong();
        menu.addAction(tr("S&ync Now"), this, [this, id] { Q_EMIT syncSourceRequested(id); });
        menu.addSeparator();
    }
    menu.addAction(tr("Connect &Source…"), this, &DatabaseTreeWidget::connectSourceRequested);
    menu.addAction(tr("&Manage Sources…"), this, &DatabaseTreeWidget::manageSourcesRequested);
    menu.exec(event->globalPos());
}
