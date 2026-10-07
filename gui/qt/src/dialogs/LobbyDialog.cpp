#include "LobbyDialog.h"

#include "app/ChessPosition.h"
#include "app/lobby/RoomName.h"
#include "widgets/BoardWidget.h"
#include "widgets/FigurineFont.h"
#include "widgets/PaddedHeaderView.h"
#include "widgets/PaddedItemDelegate.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTreeWidget>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <numeric>

namespace {

constexpr int kRoomRole = Qt::UserRole;
/// The room's id on a row of the list.
constexpr int kRoomIdRole = Qt::UserRole + 1;
/// How strongly a cell glows, 0 to 1 (GlowDelegate).
constexpr int kGlowRole = Qt::UserRole + 2;

/// The cells of the lobby's tables, padded as the games list, with the glow
/// of a game that turned to the user's move painted over them: over the
/// selection too, which a background would leave hidden.
class GlowDelegate : public PaddedItemDelegate {
public:
    GlowDelegate(int vertical, int horizontal, QObject *parent)
        : PaddedItemDelegate(vertical, horizontal, parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        PaddedItemDelegate::paint(painter, option, index);
        const qreal strength = index.data(kGlowRole).toReal();
        if (strength <= 0)
            return;
        const bool selected = option.state & QStyle::State_Selected;
        QColor glow = option.palette.color(QPalette::Highlight);
        glow = selected ? glow.lighter(170) : glow;
        glow.setAlphaF(float((selected ? 0.55 : 0.45) * strength));
        painter->fillRect(option.rect, glow);
    }
};
constexpr int kGameRole = Qt::UserRole;

/// The position after `moves`, and the last move's squares.
ChessPosition positionAfter(const QStringList &moves, int *from, int *to)
{
    ChessPosition position = ChessPosition::startingPosition();
    *from = *to = -1;
    for (const QString &uci : moves) {
        const std::optional<ChessMove> move = position.moveFromUci(uci);
        if (!move)
            break;
        position.play(*move);
        *from = move->from;
        *to = move->to;
    }
    return position;
}

/// The padded header and cells of the games list, with the cells' text
/// where the titles' is: the style draws a cell's text a margin in from
/// the padding, and a title's at the padding, so the cells' padding gives
/// that margin back.
void padTable(QTreeWidget *view)
{
    PaddedHeaderView::install(view);
    const int textMargin = view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, view) + 1;
    view->setItemDelegate(new GlowDelegate(CellPadding::vertical, CellPadding::horizontal - textMargin, view));
}

QLabel *noteLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

} // namespace

LobbyDialog::LobbyDialog(LobbyService *service, QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_me(service->me())
    , m_pages(new QStackedWidget(this))
{
    setWindowTitle(tr("Lobby"));
    resize(900, 560);

    // The lobby: the rooms with a free seat.
    auto *lobbyPage = new QWidget(m_pages);
    auto *lobbyLayout = new QVBoxLayout(lobbyPage);
    lobbyLayout->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QLabel(tr("Tournaments"), lobbyPage);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSizeF(headingFont.pointSizeF() * 1.2);
    heading->setFont(headingFont);
    lobbyLayout->addWidget(heading);
    lobbyLayout->addWidget(noteLabel(tr("Each room is a tournament of four players: everyone plays everyone twice, "
                                        "once with White and once with Black. It starts as soon as two sit down, "
                                        "and there is no clock. Yours come first, in bold, then the rooms with a free seat, "
                                        "then the full ones, whose games you can follow. Enter a room to see its games."),
                                     lobbyPage));
    m_rooms = new QTreeWidget(lobbyPage);
    padTable(m_rooms);
    m_rooms->setAlternatingRowColors(true);
    m_rooms->setHeaderLabels({tr("Room"), tr("Players"), tr("Free Seats"), tr("Games"), tr("Your Move")});
    for (int column = 1; column < m_rooms->columnCount(); ++column)
        m_rooms->headerItem()->setTextAlignment(column, Qt::AlignCenter); // The figures centred, title and cells.
    m_rooms->setRootIsDecorated(false);
    m_rooms->setUniformRowHeights(true);
    m_rooms->header()->setStretchLastSection(false);
    m_rooms->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    lobbyLayout->addWidget(m_rooms, 1);
    auto *lobbyButtons = new QHBoxLayout;
    lobbyButtons->addStretch();
    m_enterButton = new QPushButton(tr("&Enter Room"), lobbyPage);
    m_enterButton->setDefault(true);
    lobbyButtons->addWidget(m_enterButton);
    lobbyLayout->addLayout(lobbyButtons);
    m_pages->addWidget(lobbyPage);

    // A room: its seats, its games and the selected game's position.
    auto *roomPage = new QWidget(m_pages);
    auto *roomLayout = new QVBoxLayout(roomPage);
    roomLayout->setContentsMargins(0, 0, 0, 0);
    auto *top = new QHBoxLayout;
    auto *back = new QPushButton(tr("‹ &Lobby"), roomPage);
    top->addWidget(back);
    m_roomTitle = new QLabel(roomPage);
    m_roomTitle->setFont(headingFont);
    top->addWidget(m_roomTitle, 1);
    roomLayout->addLayout(top);

    // The board first: the tables are lined up with its frame.
    m_board = new BoardWidget(roomPage);
    m_board->setFixedSize(BoardWidget::sideForAvailable(280), BoardWidget::sideForAvailable(280));
    auto *columns = new QHBoxLayout;
    auto *seatsColumn = new QVBoxLayout;
    seatsColumn->addWidget(new QLabel(tr("Standings"), roomPage));
    // The seats as a table: who leads, with wins, draws, losses and points; the free seats under them.
    m_standings = new QTreeWidget(roomPage);
    padTable(m_standings);
    m_standings->setAlternatingRowColors(true);
    m_standings->setHeaderLabels({tr("#"), tr("Player"), tr("W"), tr("D"), tr("L"), tr("Pts")});
    for (int column : {0, 2, 3, 4, 5})
        m_standings->headerItem()->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter); // Over the figures.
    m_standings->headerItem()->setToolTip(2, tr("Wins"));
    m_standings->headerItem()->setToolTip(3, tr("Draws"));
    m_standings->headerItem()->setToolTip(4, tr("Losses"));
    m_standings->headerItem()->setToolTip(5, tr("Points: 1 a win, ½ a draw"));
    m_standings->setRootIsDecorated(false);
    m_standings->setUniformRowHeights(true);
    m_standings->setSelectionMode(QAbstractItemView::NoSelection);
    m_standings->setFocusPolicy(Qt::NoFocus);
    m_standings->header()->setStretchLastSection(false);
    m_standings->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_standings->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_standings->setFixedWidth(290);
    // The tables start where the board's frame does, not its widget: one top line for all three.
    const int boardTop = m_board->boardArea().top() - BoardWidget::kFrameWidth;
    seatsColumn->addSpacing(boardTop);
    seatsColumn->addWidget(m_standings, 1);
    m_joinButton = new QPushButton(tr("&Take a Seat"), roomPage);
    seatsColumn->addWidget(m_joinButton);
    m_joinHint = noteLabel(QString(), roomPage);
    m_joinHint->setMaximumWidth(290);
    seatsColumn->addWidget(m_joinHint);
    columns->addLayout(seatsColumn);

    auto *gamesColumn = new QVBoxLayout;
    gamesColumn->addWidget(new QLabel(tr("Games"), roomPage));
    m_games = new QTreeWidget(roomPage);
    padTable(m_games);
    m_games->setAlternatingRowColors(true);
    m_games->setHeaderLabels({tr("White"), tr("Black"), tr("Moves"), tr("State")});
    m_games->setRootIsDecorated(false);
    m_games->setUniformRowHeights(true);
    m_games->header()->setStretchLastSection(true);
    gamesColumn->addSpacing(boardTop);
    gamesColumn->addWidget(m_games, 1);
    columns->addLayout(gamesColumn, 1);

    auto *boardColumn = new QVBoxLayout;
    // The players over the board, centred, on the line of the tables' titles.
    m_gameNames = new QLabel(roomPage);
    QFont namesFont = m_gameNames->font();
    namesFont.setBold(true);
    m_gameNames->setFont(namesFont);
    m_gameNames->setAlignment(Qt::AlignCenter);
    m_gameNames->setFixedWidth(m_board->width());
    m_gameNames->setTextFormat(Qt::PlainText);
    boardColumn->addWidget(m_gameNames);
    boardColumn->addWidget(m_board);
    m_gameLine = new QLabel(roomPage);
    m_gameLine->setTextFormat(Qt::PlainText);
    m_gameLine->setWordWrap(true);
    m_gameLine->setFont(FigurineFont::apply(m_gameLine->font()));
    m_gameLine->setMaximumWidth(m_board->width());
    m_gameLine->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    boardColumn->addWidget(m_gameLine, 1);
    m_playButton = new QPushButton(tr("&Play"), roomPage);
    boardColumn->addWidget(m_playButton);
    columns->addLayout(boardColumn);
    roomLayout->addLayout(columns, 1);
    m_pages->addWidget(roomPage);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_pages, 1);
    m_notice = noteLabel(QString(), this);
    layout->addWidget(m_notice);
    auto *bottom = new QHBoxLayout;
    m_network = noteLabel(QString(), this);
    bottom->addWidget(m_network, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    bottom->addWidget(buttons);
    layout->addLayout(bottom);

    connect(m_rooms, &QTreeWidget::itemActivated, this, &LobbyDialog::enterSelectedRoom);
    connect(m_rooms, &QTreeWidget::currentItemChanged, this,
            [this] { m_enterButton->setEnabled(m_rooms->currentItem() != nullptr); });
    connect(m_enterButton, &QPushButton::clicked, this, &LobbyDialog::enterSelectedRoom);
    connect(back, &QPushButton::clicked, this, &LobbyDialog::showLobbyPage);
    connect(m_joinButton, &QPushButton::clicked, this, &LobbyDialog::join);
    connect(m_games, &QTreeWidget::currentItemChanged, this, &LobbyDialog::showSelectedGame);
    connect(m_games, &QTreeWidget::itemActivated, this, &LobbyDialog::play);
    connect(m_playButton, &QPushButton::clicked, this, &LobbyDialog::play);

    connect(m_service, &LobbyService::networkChanged, this, &LobbyDialog::showNetwork);
    showNetwork();
    m_glow = new QVariantAnimation(this);
    m_glow->setDuration(2500);
    m_glow->setStartValue(0.0);
    m_glow->setEndValue(1.0);
    connect(m_glow, &QVariantAnimation::valueChanged, this, &LobbyDialog::paintGlow);
    connect(m_glow, &QVariantAnimation::finished, this, &LobbyDialog::paintGlow);
    // Events come in from the network while the window is open: it is live.
    m_waiting = waitingGames();
    connect(m_service, &LobbyService::changed, this, &LobbyDialog::lobbyChanged);
    fillLobby();
}

QSet<QString> LobbyDialog::waitingGames() const
{
    QSet<QString> waiting;
    for (const LobbyRoom &room : lobby().rooms()) {
        for (const LobbyGame &game : room.games) {
            if (game.waitsFor(m_me))
                waiting.insert(room.id + QLatin1Char('|') + game.white + QLatin1Char('|') + game.black);
        }
    }
    return waiting;
}

void LobbyDialog::lobbyChanged()
{
    const QSet<QString> now = waitingGames();
    const QSet<QString> turned = now - m_waiting;
    m_waiting = now;
    if (!isVisible())
        return; // What happened meanwhile is shown when it opens, without glowing: nothing is pressed on the user.
    refresh();
    if (turned.isEmpty())
        return;
    m_glowGames = turned;
    m_glowRooms.clear();
    for (const QString &game : turned)
        m_glowRooms.insert(game.section(QLatin1Char('|'), 0, 0));
    m_glow->stop();
    m_glow->start();
}

void LobbyDialog::paintGlow()
{
    const bool running = m_glow->state() == QAbstractAnimation::Running;
    const qreal strength = running ? 1.0 - qreal(m_glow->currentTime()) / qreal(m_glow->duration()) : 0.0;
    for (int row = 0; row < m_rooms->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_rooms->topLevelItem(row);
        const bool glows = m_glowRooms.contains(item->data(0, kRoomIdRole).toString());
        for (int column = 0; column < m_rooms->columnCount(); ++column)
            item->setData(column, kGlowRole, glows ? strength : 0.0);
    }
    for (int row = 0; row < m_games->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_games->topLevelItem(row);
        const bool glows = m_glowGames.contains(m_roomId + QLatin1Char('|') + item->data(1, kGameRole).toString());
        for (int column = 0; column < m_games->columnCount(); ++column)
            item->setData(column, kGlowRole, glows ? strength : 0.0);
    }
    if (!running) {
        m_glowGames.clear();
        m_glowRooms.clear();
    }
}

void LobbyDialog::showNetwork()
{
    // As it is, without pretending (IDEA.md §38): the lobby is what the peers keep alive.
    if (!m_service->isOnline())
        m_network->setText(tr("Not on the network: no relay or peer can be reached. What you do leaves when one can."));
    else
        m_network->setText(tr("On the network: %1, %2.")
                               .arg(tr("%n relay(s)", nullptr, m_service->relayCount()),
                                    tr("%n peer(s) connected", nullptr, m_service->peerCount())));
}

int LobbyDialog::roomIndex() const
{
    return m_roomId.isEmpty() ? -1 : lobby().indexOfRoom(m_roomId);
}

void LobbyDialog::fillLobby()
{
    const QTreeWidgetItem *selected = m_rooms->currentItem();
    const QString selectedText = selected ? selected->text(0) : QString();
    m_rooms->clear();
    const QList<LobbyRoom> &rooms = lobby().rooms();
    // The user's rooms first, full or not — those with games waiting for
    // their move before the others: the lobby is also the way back to them.
    QList<int> shown;
    for (int i = 0; i < rooms.size(); ++i) {
        if (rooms.at(i).isSeated(m_me))
            shown << i;
    }
    std::stable_sort(shown.begin(), shown.end(), [&](int a, int b) {
        return rooms.at(a).gamesWaitingFor(m_me).size() > rooms.at(b).gamesWaitingFor(m_me).size();
    });
    for (int index : lobby().joinableRooms()) {
        if (!shown.contains(index))
            shown << index;
    }
    // Then the full ones, whose games can be followed.
    for (int i = 0; i < rooms.size(); ++i) {
        if (!shown.contains(i))
            shown << i;
    }
    const auto centre = [this](QTreeWidgetItem *item) {
        for (int column = 1; column < m_rooms->columnCount(); ++column)
            item->setTextAlignment(column, Qt::AlignCenter);
    };
    int buttonWidth = 0;
    for (int index : std::as_const(shown)) {
        const LobbyRoom &room = rooms.at(index);
        const int going = int(std::count_if(room.games.cbegin(), room.games.cend(),
                                            [](const LobbyGame &game) { return !game.isOver(); }));
        auto *item = new QTreeWidgetItem(m_rooms, {room.name(),
                                                   tr("%1 / %2").arg(room.players()).arg(LobbyRoom::kSeats),
                                                   room.isJoinable() ? QString::number(LobbyRoom::kSeats - room.players())
                                                                     : tr("Full"),
                                                   tr("%n going on", nullptr, going)});
        item->setData(0, kRoomRole, index);
        item->setData(0, kRoomIdRole, room.id);
        centre(item);
        if (room.isSeated(m_me)) {
            QFont font = item->font(0);
            font.setBold(true);
            for (int column = 0; column < m_rooms->columnCount(); ++column)
                item->setFont(column, font);
            item->setToolTip(0, tr("You sit in this room"));
        }
        // Games waiting for the user's move: the room says so, and takes them there.
        const int waiting = int(room.gamesWaitingFor(m_me).size());
        if (waiting > 0) {
            auto *button = new QPushButton(waiting == 1 ? tr("Play Now") : tr("Play Now (%1)").arg(waiting), m_rooms);
            button->setToolTip(tr("%n game(s) waiting for your move", nullptr, waiting));
            const int row = m_rooms->indexOfTopLevelItem(item);
            connect(button, &QPushButton::clicked, this, [this, row] { playNow(row); });
            m_rooms->setItemWidget(item, 4, button);
            buttonWidth = std::max(buttonWidth, button->sizeHint().width());
        }
    }
    // The lobby keeps rooms open to newcomers: a new one exists once someone
    // sits in it, but it has its name already.
    for (int i = 0; i < lobby().newRooms(); ++i) {
        auto *item = new QTreeWidgetItem(m_rooms, {RoomName::text(lobby().offeredRooms().at(i)),
                                                   tr("0 / %1").arg(LobbyRoom::kSeats),
                                                   QString::number(LobbyRoom::kSeats), tr("New")});
        item->setData(0, kRoomRole, -1 - i);
        centre(item);
        QFont font = item->font(0);
        font.setItalic(true);
        item->setFont(0, font);
        item->setToolTip(0, tr("A new room: nobody sits here yet, and it opens when you take a seat"));
    }
    for (int column = 1; column < m_rooms->columnCount(); ++column)
        m_rooms->resizeColumnToContents(column);
    m_rooms->header()->resizeSection(4, std::max(m_rooms->header()->sectionSize(4), buttonWidth + 8));
    const QList<QTreeWidgetItem *> same = m_rooms->findItems(selectedText, Qt::MatchExactly);
    m_rooms->setCurrentItem(same.isEmpty() ? m_rooms->topLevelItem(0) : same.constFirst());
    paintGlow(); // Rows made again while it fades.
}

void LobbyDialog::playNow(int row)
{
    const QTreeWidgetItem *item = m_rooms->topLevelItem(row);
    const int index = item ? item->data(0, kRoomRole).toInt() : -1;
    if (index < 0)
        return;
    const LobbyRoom &room = lobby().rooms().at(index);
    const QList<int> waiting = room.gamesWaitingFor(m_me);
    if (waiting.size() == 1) {
        playGame(index, waiting.constFirst());
        return;
    }
    // Several games wait: the user chooses which, by opponent and position.
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (int game : waiting) {
        const LobbyGame &entry = room.games.at(game);
        const bool white = entry.white == m_me;
        const QString opponent = room.displayName(white ? entry.black : entry.white);
        const QString line = entry.moves.isEmpty()
            ? tr("first move")
            : ChessPosition::startingPosition().lineText(entry.moves, -1, SanStyle::Letters).section(QLatin1Char(' '), -2);
        QAction *action = menu->addAction(white ? tr("With White against %1 — %2").arg(opponent, line)
                                                : tr("With Black against %1 — %2").arg(opponent, line));
        connect(action, &QAction::triggered, this, [this, index, game] { playGame(index, game); });
    }
    const QWidget *button = m_rooms->itemWidget(m_rooms->topLevelItem(row), 4);
    menu->popup(button ? button->mapToGlobal(QPoint(0, button->height())) : QCursor::pos());
}

void LobbyDialog::playGame(int room, int game)
{
    enterRoom(room);
    for (int i = 0; i < m_games->topLevelItemCount(); ++i) {
        if (m_games->topLevelItem(i)->data(0, kGameRole).toInt() == game)
            m_games->setCurrentItem(m_games->topLevelItem(i));
    }
    play();
}

LobbyRoom LobbyDialog::shownRoom() const
{
    if (const int index = roomIndex(); index >= 0)
        return lobby().rooms().at(index);
    LobbyRoom room;
    if (m_offer >= 0 && m_offer < lobby().newRooms())
        room.seed = lobby().offeredRooms().at(m_offer);
    return room;
}

void LobbyDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_waiting = waitingGames(); // From now on what turns to the user's move glows.
    refresh();
}

void LobbyDialog::refresh()
{
    if (m_pages->currentIndex() == 0)
        fillLobby();
    else
        showRoom();
}

void LobbyDialog::showLobbyPage()
{
    fillLobby();
    m_pages->setCurrentIndex(0);
}

void LobbyDialog::enterRoomAt(int row)
{
    if (QTreeWidgetItem *item = m_rooms->topLevelItem(row)) {
        m_rooms->setCurrentItem(item);
        enterSelectedRoom();
    }
}

void LobbyDialog::selectGame(int row)
{
    if (QTreeWidgetItem *item = m_games->topLevelItem(row))
        m_games->setCurrentItem(item);
}

void LobbyDialog::enterSelectedRoom()
{
    if (const QTreeWidgetItem *item = m_rooms->currentItem())
        enterRoom(item->data(0, kRoomRole).toInt());
}

void LobbyDialog::enterRoom(int index)
{
    m_roomId = index >= 0 ? lobby().rooms().at(index).id : QString();
    m_offer = index >= 0 ? -1 : -1 - index;
    m_notice->clear();
    showRoom();
    m_pages->setCurrentIndex(1);
}

void LobbyDialog::showRoom()
{
    const LobbyRoom room = shownRoom();
    const bool seated = room.isSeated(m_me);
    m_roomTitle->setText(roomIndex() >= 0 ? tr("%1 — %2 / %3 players").arg(room.name()).arg(room.players()).arg(LobbyRoom::kSeats)
                                     : tr("%1 — a new room").arg(room.name()));

    m_standings->clear();
    const auto points = [](int halfPoints) {
        return halfPoints % 2 ? (halfPoints > 1 ? QString::number(halfPoints / 2) : QString()) + QStringLiteral("½")
                              : QString::number(halfPoints / 2);
    };
    for (const LobbyStanding &line : room.standings()) {
        auto *item = new QTreeWidgetItem(m_standings, {QString::number(line.place), room.displayName(line.player),
                                                       QString::number(line.wins), QString::number(line.draws),
                                                       QString::number(line.losses), points(line.halfPoints)});
        item->setToolTip(1, tr("%n game(s) finished", nullptr, line.played));
        for (int column : {0, 2, 3, 4, 5})
            item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        QFont font = item->font(0);
        font.setBold(line.player == m_me);
        for (int column = 0; column < m_standings->columnCount(); ++column)
            item->setFont(column, font);
    }
    for (int i = room.players(); i < LobbyRoom::kSeats; ++i) {
        auto *item = new QTreeWidgetItem(m_standings, {QString(), tr("Free seat")});
        QFont font = item->font(1);
        font.setItalic(true);
        item->setFont(1, font);
        item->setForeground(1, palette().brush(QPalette::PlaceholderText));
    }
    m_joinButton->setVisible(!seated);
    m_joinButton->setEnabled(room.isJoinable());
    if (seated)
        m_joinHint->setText(tr("You sit here: your games are in bold."));
    else if (!room.isJoinable())
        m_joinHint->setText(tr("The room is full: you can follow its games."));
    else if (room.players() == 0)
        m_joinHint->setText(tr("Sit first: the room opens, and your games begin as the others come."));
    else
        m_joinHint->setText(tr("Take a seat and you play two games with each of the %n player(s) here, "
                               "one with each colour.", nullptr, room.players()));

    // The game selected stays selected when the room changes under it.
    const QTreeWidgetItem *selected = m_games->currentItem();
    const QString selectedGame = selected ? selected->data(1, kGameRole).toString() : QString();
    m_games->clear();
    // The games waiting for the user's move first, then their other games, then the rest.
    QList<int> order(room.games.size());
    std::iota(order.begin(), order.end(), 0);
    const auto rank = [&](int i) {
        const LobbyGame &game = room.games.at(i);
        return game.waitsFor(m_me) ? 0 : game.involves(m_me) ? 1 : 2;
    };
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return rank(a) < rank(b); });
    for (int i : std::as_const(order)) {
        const LobbyGame &game = room.games.at(i);
        QString state;
        if (game.isOver())
            state = game.result;
        else if (game.moves.isEmpty())
            state = game.toMove() == m_me ? tr("Your move: it starts with you") : tr("Not started");
        else
            state = game.toMove() == m_me ? tr("Your move") : tr("%1 to move").arg(room.displayName(game.toMove()));
        auto *item = new QTreeWidgetItem(m_games, {room.displayName(game.white), room.displayName(game.black),
                                                   QString::number((game.moves.size() + 1) / 2), state});
        item->setData(0, kGameRole, i);
        item->setData(1, kGameRole, game.white + QLatin1Char('|') + game.black);
        if (game.involves(m_me)) {
            QFont font = item->font(0);
            font.setBold(true);
            for (int column = 0; column < m_games->columnCount(); ++column)
                item->setFont(column, font);
        }
    }
    for (int column = 0; column < m_games->columnCount() - 1; ++column)
        m_games->resizeColumnToContents(column);
    // The first game waiting for the user is first in the list.
    QTreeWidgetItem *current = m_games->topLevelItem(0);
    for (int i = 0; i < m_games->topLevelItemCount(); ++i) {
        if (!selectedGame.isEmpty() && m_games->topLevelItem(i)->data(1, kGameRole).toString() == selectedGame)
            current = m_games->topLevelItem(i);
    }
    m_games->setCurrentItem(current);
    showSelectedGame();
    paintGlow();
}

void LobbyDialog::showSelectedGame()
{
    const QTreeWidgetItem *item = m_games->currentItem();
    const int index = roomIndex();
    if (!item || index < 0) {
        m_board->setFlipped(false);
        m_board->setBoard(BoardFrame{ChessPosition::startingPosition().boardState()});
        m_gameNames->clear();
        m_gameLine->setText(index < 0 ? QString() : tr("No games yet: they begin when two players sit."));
        m_playButton->setEnabled(false);
        m_playButton->setText(tr("&Play"));
        return;
    }
    const LobbyRoom &room = lobby().rooms().at(index);
    const LobbyGame &game = room.games.at(item->data(0, kGameRole).toInt());
    int from = -1;
    int to = -1;
    const ChessPosition position = positionAfter(game.moves, &from, &to);
    m_board->setFlipped(game.black == m_me);
    m_board->setBoard(BoardFrame{position.boardState(), from, to});
    const QString line = ChessPosition::startingPosition().lineText(game.moves, -1, SanStyle::Figurines);
    m_gameNames->setText(QStringLiteral("%1 – %2").arg(room.displayName(game.white), room.displayName(game.black)));
    m_gameLine->setText(line.isEmpty() ? tr("No moves yet.") : line);
    m_playButton->setEnabled(game.involves(m_me) && !game.isOver());
    m_playButton->setText(game.involves(m_me) && !game.isOver() && game.toMove() == m_me ? tr("&Play Your Move")
                                                                                       : tr("&Play"));
}

void LobbyDialog::join()
{
    if (roomIndex() >= 0) {
        if (!m_service->joinRoom(m_roomId))
            return;
    } else {
        if (m_offer < 0 || m_offer >= lobby().newRooms())
            return;
        const QString id = m_service->openRoom(lobby().offeredRooms().at(m_offer));
        if (id.isEmpty())
            return;
        m_roomId = id;
        m_offer = -1;
    }
    showRoom();
    m_notice->setText(tr("You sat in %1. Your games are in bold: the ones where you have White start with your move.")
                          .arg(shownRoom().name()));
}

void LobbyDialog::play()
{
    const QTreeWidgetItem *item = m_games->currentItem();
    const int index = roomIndex();
    if (!item || index < 0)
        return;
    const LobbyGame &game = lobby().rooms().at(index).games.at(item->data(0, kGameRole).toInt());
    if (!game.involves(m_me) || game.isOver())
        return;
    Q_EMIT playRequested(m_roomId, game.white, game.black);
}
