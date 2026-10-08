#include "MainWindow.h"

#include "app/BookWeights.h"
#include "app/ClassicGames.h"
#include "app/GameIdentity.h"
#include "app/OpeningNames.h"
#include "app/DatabaseMerge.h"
#include "app/ShippedOpeningNames.h"
#include "DesktopApi.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/GraphicsSettingsDialog.h"
#include "dialogs/FolderSettingsDialog.h"
#include "dialogs/DatabaseSettingsDialog.h"
#include "app/PolyglotBook.h"
#include "app/PositionIndexBuilder.h"
#include "app/DatabaseOutline.h"
#include "dialogs/ConnectSourceWizard.h"
#include "dialogs/GameInfoDialog.h"
#include "dialogs/HelpDialog.h"
#include "dialogs/DrawersDialog.h"
#include "dialogs/LobbyDialog.h"
#include "app/lobby/LobbyPlans.h"
#ifdef PRAGMA_HAS_PHONE_LINK
#include "app/lobby/net/LobbyIdentity.h"
#include "app/lobby/net/LobbyNetwork.h"
#include "app/lobby/net/LobbyNode.h"
#endif
#include "dialogs/ManageChaptersDialog.h"
#include "dialogs/ManageEnginesDialog.h"
#include "dialogs/ManageSourcesDialog.h"
#include "dialogs/NewGameChoiceDialog.h"
#include "dialogs/NewTrainingDialog.h"
#include "dialogs/PositionSetupDialog.h"
#include "dialogs/PersonalSettingsDialog.h"
#include "dialogs/PlayOnlineDialog.h"
#include "dialogs/ProjectSettingsDialog.h"
#include "dialogs/ManageSyncFilesDialog.h"
#include "dialogs/SyncDialog.h"
#ifdef PRAGMA_HAS_PHONE_LINK
#include "app/phone/DatabaseFolderStore.h"
#include "app/phone/PhoneLink.h"
#include "dialogs/ConnectMobileDialog.h"
#endif
#include "app/Explainer.h"
#include "app/LineInsight.h"
#include "app/GameSession.h"
#include "app/GameVariations.h"
#include "app/MoveAnnotation.h"
#include "app/MoveComment.h"
#include "app/Pgn.h"
#include "app/TimeControl.h"
#include "app/TrainingSets.h"
#include "app/Project.h"
#include "app/PersonalSettings.h"
#include "app/SqliteGameDatabase.h"
#include "app/UiLanguage.h"
#include "app/UciEngine.h"
#include "app/UserFolders.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceCredentials.h"
#include "app/sources/SourceSync.h"
#include "app/sync/FolderSync.h"
#include "app/sync/RemoteStore.h"
#include "app/sync/SyncPipeline.h"
#include "app/sync/SyncTasks.h"
#include "models/GameFilterProxyModel.h"
#include "models/GameListModel.h"
#include "platform/Appearance.h"
#include "platform/MoveSound.h"
#include "platform/SymbolicIcons.h"
#include "platform/WindowChrome.h"
#include "widgets/BoardPanel.h"
#include "widgets/BookPanel.h"
#include "widgets/PaddedHeaderView.h"
#include "widgets/PaddedItemDelegate.h"
#include "widgets/BoardWidget.h"
#include "widgets/BoardSideColumn.h"
#include "widgets/BoardTheme.h"
#include "widgets/CapturedPiecesWidget.h"
#include "widgets/PaddedStatusBar.h"
#include "widgets/CentralArea.h"
#include "widgets/DatabaseTreeWidget.h"
#include "widgets/EnginePanel.h"
#include "widgets/EvaluationBar.h"
#include "widgets/FigurineFont.h"
#include "widgets/GameHeaderWidget.h"
#include "widgets/GlyphMenuAction.h"
#include "widgets/MoveTreeView.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCryptographicHash>
#include <QSet>
#include <QDataStream>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QCloseEvent>
#include <QDate>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace {

/// The online game in progress (QSettings, per user and computer), followed again at the next start.
constexpr char kActiveGameKey[] = "online/activeGame";
constexpr char kActiveAccountKey[] = "online/activeAccount";

/// Delay before maximizing a window restored maximized, once it is mapped.
constexpr int kRestoreWindowStateDelayMs = 250;

/// Depth of the search that picks the engine's move in training: deep
/// enough to play well, shallow enough to answer at once.
constexpr int kTrainingDepth = 12;
/// The tutor only judges a move against an evaluation at least this deep.
constexpr int kTutorMinDepth = 8;
/// How deep the position before a move is searched when Explain has no
/// evaluation of it: enough for EXPLAIN.smart to judge the move.
constexpr int kExplainBeforeDepth = 16;

/// How long the engine's move takes to cross the board, so that the user
/// cannot miss what just happened.
constexpr int kEngineMoveMs = 1500;

/// How long closing waits for a sync before giving up on the server.
constexpr int kCloseSyncTimeoutMs = 30000;

QIcon themeIcon(const char *name, QStyle::StandardPixmap)
{
    // Flat monochrome icons that follow the theme's text color.
    return SymbolicIcons::icon(QString::fromLatin1(name));
}

/// A position to show, with its last move and the king in check or mated marked.
BoardFrame frameFor(const ChessPosition &position, int lastMoveFrom, int lastMoveTo)
{
    BoardFrame frame{position.boardState(), lastMoveFrom, lastMoveTo};
    if (position.inCheck()) {
        frame.markedKing = position.kingSquare(position.sideToMove());
        frame.kingMark = position.legalMoves().isEmpty() ? KingMark::Mate : KingMark::Check;
    }
    return frame;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_session(new GameSession(this))
    , m_gameListModel(new GameListModel(this))
    , m_gameListProxy(new GameFilterProxyModel(this))
    , m_board(new BoardWidget)
    , m_sidebar(new QMainWindow)
    , m_evaluationBar(new EvaluationBar)
    , m_gameHeader(new GameHeaderWidget)
    , m_capturedPieces(new CapturedPiecesWidget)
    , m_boardSideColumn(new BoardSideColumn)
    , m_engine(new UciEngine(this))
    , m_explainer(new Explainer(this))
    , m_sourceSync(new SourceSync(this))
    , m_folderSync(new FolderSync(UserFolders::pragmaDir(), SyncSettings::statePath(), SyncSettings::deviceName(), this))
    , m_syncPipeline(new SyncPipeline(this))
    , m_syncTimer(new QTimer(this))
{
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);
    m_sidebar->setWindowFlags(Qt::Widget);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);

    m_gameListProxy->setSourceModel(m_gameListModel);
    m_gameListProxy->setSortRole(Qt::DisplayRole);

    createActions();
    m_boardPanel = new BoardPanel(m_board, m_evaluationBar, m_gameHeader, m_capturedPieces, m_boardSideColumn,
                                  {m_firstMoveAction, m_previousMoveAction, m_explainAction,
                                   m_nextMoveAction, m_lastMoveAction, m_flipBoardAction});
    applyGraphicsSettings(GraphicsSettings::load());
    setCentralWidget(new CentralArea(m_boardPanel, m_sidebar));
    createDocks();
    createToolBar();
    createMenus();
    // The toolbar's book drops down the choice alone: the books and No Book.
    auto *bookChoices = new QMenu(this);
    connect(bookChoices, &QMenu::aboutToShow, this, [this, bookChoices] {
        bookChoices->clear();
        fillBookChoices(bookChoices);
    });
    m_bookButton->setMenu(bookChoices);
    m_engineButton->setMenu(m_engineChoiceMenu);
    m_databaseButton->setMenu(m_databasesMenu);
    updateResourceButtons();
    createStatusBar();
    updateSeparatorStyle();

    // The chapter keeps the game on the board as it changes.
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::syncChapterGame);
    // Another game on the board leaves Lobby Mode; moves played on it are a plan in the making.
    connect(m_session, &GameSession::gameChanged, this, [this] {
        if (m_lobbyGame && !m_settingLobbyGame && m_session->game().uid != m_lobbyGame->uid)
            leaveLobbyGame();
        updateLobbyPanel();
    });
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateLobbyPanel);
    connect(m_session, &GameSession::headerChanged, this, &MainWindow::syncChapterGame);
    connect(m_session, &GameSession::annotationsChanged, this, &MainWindow::syncChapterGame);
    connect(m_session, &GameSession::commentsChanged, this, &MainWindow::syncChapterGame);
    connect(m_session, &GameSession::commentsChanged, this, &MainWindow::updateCommentMarks);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameHeader);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameActions);
    connect(m_session, &GameSession::headerChanged, this, &MainWindow::updateGameHeader);
    connect(m_gameHeader, &GameHeaderWidget::activated, this, &MainWindow::editGameInfo);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::syncBoard);
    // Before the analysis: in training the engine's own search replaces it.
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateTraining);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::analyzeCurrentPosition);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateBookMoves);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateBookMoves);
    m_positionIndex = new PositionIndexBuilder(this);
    m_positionIndexTimer = new QTimer(this);
    m_positionIndexTimer->setSingleShot(true);
    m_positionIndexTimer->setInterval(1000);
    connect(m_positionIndexTimer, &QTimer::timeout, this, &MainWindow::rebuildPositionIndex);
    connect(m_positionIndex, &PositionIndexBuilder::indexChanged, this, &MainWindow::updateBoardFilters);
    connect(m_positionIndex, &PositionIndexBuilder::indexChanged, this, &MainWindow::updateBookDatabaseStats);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateBoardFilters);
    connect(m_engine, &UciEngine::searchFinished, this, &MainWindow::finishEngineMove);
    connect(m_engine, &UciEngine::searchFinished, this, [this] {
        // The position before the move is searched: on to the one on the board.
        if (m_explainingBefore) {
            m_enginePanel->setStatus(QString());
            analyzeCurrentPosition();
        }
    });
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::clearTutor);
    connect(m_engine, &UciEngine::evaluationChanged, this, [this](const EngineEvaluation &evaluation) {
        if (m_explainingBefore) {
            // The position before the move, for Explain only: the panel and the
            // bar stay with the position on the board.
            if (m_session->ply() > 0)
                m_explainer->setEvaluation(m_session->positionAt(m_session->ply() - 1), evaluation);
            return;
        }
        m_lastEvaluation = evaluation;
        // While the user thinks in training, the analysis of their position
        // is what the tutor will judge their move against.
        if (m_trainingModeAction->isChecked() && !m_trainingThinking && !m_tutorReply && !isEngineTurn()) {
            const QString fen = m_session->position().fen();
            if (m_trainingBaselineFen != fen || evaluation.depth >= m_trainingBaseline.depth) {
                m_trainingBaseline = evaluation;
                m_trainingBaselineFen = fen;
            }
        }
        m_evaluationBar->setEvaluation(evaluation);
        m_explainer->setLiveEvaluation(evaluation);
        m_engineLine = m_session->position().lineText(evaluation.pv);
        m_enginePanel->setEvaluation(evaluation, m_session->position().lineText(evaluation.pv, -1, SanStyle::Figurines));
        // Held, the eye follows the engine: each new line moves the board to where it now ends.
        if (m_enginePanel->isPeeking())
            peekAtEngineLine(true);
    });
    connect(m_explainer, &Explainer::explanationChanged, this, [this](const MoveExplanation &explanation) {
        m_explanation = explanation;
        m_board->setExplanation(explanation.arrows, explanation.lostPieces);
        // A forced mate is shown by playing it; the board returns when Explain is turned off.
        // The same mate found again at a deeper search keeps playing, it does not start over.
        const bool samePlayback = explanation.playback == m_explanationPlayback;
        m_explanationPlayback = explanation.playback;
        if (samePlayback && !explanation.playback.isEmpty()) {
            m_enginePanel->setExplanation(explanation.summary);
            m_explanationText = explanation.summary;
            return;
        }
        QList<BoardFrame> frames;
        ChessPosition position = m_session->position();
        for (const QString &uci : explanation.playback) {
            const std::optional<ChessMove> move = position.moveFromUci(uci);
            if (!move)
                break;
            position.play(*move);
            frames << frameFor(position, move->from, move->to);
        }
        if (frames.isEmpty())
            m_board->stopSequence();
        else
            m_board->playSequence(frames);
        m_enginePanel->setExplanation(explanation.summary);
        m_explanationText = explanation.summary;
        // The border stops breathing and turns blue, but only for an answer.
        const bool explained = !explanation.summary.isEmpty() || !explanation.arrows.isEmpty();
        m_explainBorder = explained ? BoardBorder::Explained : BoardBorder::Plain;
        updateBoardBorder();
    });
    connect(m_engine, &UciEngine::nameChanged, m_enginePanel, &EnginePanel::setEngineName);
    connect(m_engine, &UciEngine::failed, this, [this](const QString &message) {
        m_startEngineAction->setChecked(false);
        m_enginePanel->setStatus(tr("The engine stopped: %1").arg(message));
    });
    connect(m_board, &BoardWidget::navigateRequested, this,
            [this](int steps) { m_session->goToPly(m_session->ply() + steps); });
    connect(m_board, &BoardWidget::moveRequested, this, &MainWindow::playBoardMove);
    // The engine's and the opponent's moves are heard when they land.
    connect(m_board, &BoardWidget::animatedMoveLanded, this, &MainWindow::playMoveSound);
    connect(m_sourceSync, &SourceSync::gamesImported, this, &MainWindow::showAddedGames);
    connect(m_sourceSync, &SourceSync::gamesUpdated, this, [this](const QList<qint64> &indexes) {
        for (const qint64 index : indexes)
            m_gameListModel->refreshRow(int(index));
        // The game on the board changed in its file: the board shows the new
        // version, or a later edit here would write the old one back.
        if (m_database && indexes.contains(m_openGameIndex)) {
            if (const std::optional<GameRecord> game = m_database->loadGame(m_openGameIndex)) {
                const int ply = m_session->ply();
                m_session->setGame(*game);
                m_session->goToPly(qMin(ply, int(game->moves.size())));
            }
        }
        m_positionIndexTimer->start(); // Their moves may differ.
    });
    connect(m_sourceSync, &SourceSync::sourcesChanged, m_databaseTree, &DatabaseTreeWidget::scheduleRefresh);
    connect(m_sourceSync, &SourceSync::sourceUnavailable, this, &MainWindow::reportUnavailableSource);

    // The whole sync, in order, from the toolbar button or before closing.
    connect(m_syncPipeline, &SyncPipeline::started, this, [this] {
        updateSyncActions();
        m_folderSyncLabel->setToolTip(QString());
        m_folderSyncLabel->show();
    });
    connect(m_syncPipeline, &SyncPipeline::taskStarted, this,
            [this](const QString &name) { m_folderSyncLabel->setText(tr("Syncing %1…").arg(name)); });
    connect(m_syncPipeline, &SyncPipeline::progress, m_folderSyncLabel, &QLabel::setText);
    connect(m_syncPipeline, &SyncPipeline::finished, this, [this](const QStringList &errors) {
        updateSyncActions();
        if (errors.isEmpty()) {
            m_folderSyncLabel->hide();
            statusBar()->showMessage(tr("Everything is in sync"), 5000);
            return;
        }
        m_folderSyncLabel->setText(tr("Sync failed"));
        m_folderSyncLabel->setToolTip(errors.join(QLatin1Char('\n')));
    });

    // Duplicate databases are merged into one, and the merge reaches the other devices.
    m_folderSync->setDatabaseHooks(DatabaseMerge::hooks([](const QString &relative) {
        const QFileInfo file(QDir(UserFolders::pragmaDir()).filePath(relative));
        for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
            if (file == QFileInfo(QDir(UserFolders::openingNamesDir()).filePath(names.fileName)))
                return true;
        }
        return false;
    }));
    // Folder sync with a server: every few minutes, and shortly after starting.
    m_syncTimer->setInterval(5 * 60 * 1000);
    connect(m_syncTimer, &QTimer::timeout, m_folderSync, &FolderSync::sync);
    connect(m_folderSync, &FolderSync::started, this, [this] {
        m_folderSyncLabel->setText(tr("Syncing…"));
        m_folderSyncLabel->setToolTip(QString());
        m_folderSyncLabel->show();
    });
    connect(m_folderSync, &FolderSync::progress, m_folderSyncLabel, &QLabel::setText);
    connect(m_folderSync, &FolderSync::finished, this, [this](const QString &error, int changes) {
        // The personal settings may have come from another computer.
        if (changes > 0)
            applyBoardTheme(PersonalSettings::read(PersonalSettings::path()));
        if (!error.isEmpty()) {
            m_folderSyncLabel->setText(tr("Sync failed"));
            m_folderSyncLabel->setToolTip(error);
            return;
        }
        m_folderSyncLabel->hide();
        if (changes > 0)
            statusBar()->showMessage(tr("Synced %n file(s) with the server", nullptr, changes), 5000);
    });
    // Files the user deleted by hand: asked once the sync has finished.
    connect(m_folderSync, &FolderSync::deletedByHand, this, [this](const QStringList &paths) {
        QTimer::singleShot(0, this, [this, paths] { askAboutFilesDeletedByHand(paths); });
    });
    // A database the sync replaces is closed first and opened again after.
    connect(m_folderSync, &FolderSync::localFileAboutToChange, this, [this](const QString &path) {
        if (!m_database || QFileInfo(m_database->location()) != QFileInfo(path))
            return;
        m_reopenAfterSync = m_database->location();
        m_reopenGameId = m_openGameIndex >= 0 ? m_database->header(m_openGameIndex).id : -1;
        m_reopenPly = m_session->ply();
        setDatabase(nullptr);
    });
    // An open database merged into another: the other one is opened instead.
    connect(m_folderSync, &FolderSync::localFileMerged, this, [this](const QString &from, const QString &into) {
        // The opening names follow their database too.
        if (!m_openingNamesPath.isEmpty() && QFileInfo(m_openingNamesPath) == QFileInfo(from))
            chooseOpeningNames(into);
        if (m_reopenAfterSync.isEmpty() || QFileInfo(m_reopenAfterSync) != QFileInfo(from))
            return;
        m_reopenAfterSync = into;
        m_reopenGameId = -1;
        m_reopenPly = 0;
    });
    connect(m_folderSync, &FolderSync::localFileChanged, this, [this](const QString &path) {
        if (m_reopenAfterSync.isEmpty() || QFileInfo(m_reopenAfterSync) != QFileInfo(path))
            return;
        const QString reopen = m_reopenAfterSync;
        m_reopenAfterSync.clear();
        if (!QFileInfo::exists(reopen)) {
            openInitialDatabase(QString());
            return;
        }
        if (m_onlinePlay) {
            // The online game goes on: only the database is opened again.
            openDatabaseFile(reopen);
            return;
        }
        Project project = captureProject();
        project.databasePath = reopen;
        project.gameId = m_reopenGameId;
        project.ply = m_reopenPly;
        applyProject(project, false);
    });
    connect(m_sourceSync, &SourceSync::activityChanged, this, [this](const QString &text) {
        m_syncLabel->setText(text);
        m_syncLabel->setVisible(!text.isEmpty());
    });

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveSession);
    // quit() (e.g. on SIGTERM) does not deliver closeEvent, so save here as well.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    applyDefaultLayout();
    applyBoardTheme(PersonalSettings::read(PersonalSettings::path()));
    restoreBook();
    migrateOpeningNames(); // Before the session, which may have the moved database open.
    restoreSession();
    // Someone who plays in the lobby is on the network from the start.
    if (QFileInfo::exists(QDir(lobbyDirectory()).filePath(QStringLiteral("ledger.jsonl"))))
        lobbyService();
    resumeOnlineGame(); // After the session: the online game takes the board.
    adoptShippedLineages();
    updateDistributedDatabases();
    seedDistributedProjects();
    restoreOpeningNames(); // After the session, so the first launch still seeds Classic Games first.
    applySyncSettings();
    createPhoneLink();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_flipBoardAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_coordinatesAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_startEngineAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_trainingModeAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    const QHeaderView *gameHeader = m_gameView->horizontalHeader();
    connect(gameHeader, &QHeaderView::sectionMoved, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sectionResized, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sortIndicatorChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_gamesSplitter, &QSplitter::splitterMoved, this, &MainWindow::separatorReleased);
    m_sidebar->installEventFilter(this);
    for (QDockWidget *dock : findChildren<QDockWidget *>()) {
        connect(dock, &QDockWidget::visibilityChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::topLevelChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::dockLocationChanged, this, &MainWindow::scheduleSaveSession);
        dock->installEventFilter(this); // Dragging a separator resizes the docks and tells nobody else.
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize && qobject_cast<QDockWidget *>(watched))
        scheduleSaveSession(); // The panels' shares are part of the project.
    // A separator of the sidebar let go: QMainWindow moves its separators
    // in its own mouse events, so the release is where a drag has ended.
    if (event->type() == QEvent::MouseButtonRelease && watched == m_sidebar)
        separatorReleased();
    return QMainWindow::eventFilter(watched, event);
}

MainWindow::~MainWindow()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    delete m_phoneLink; // Before the game store it uses.
#endif
}

void MainWindow::showAddedGames()
{
    m_gameListModel->refreshAppended();
    if (m_filterSource != 0)
        showCategory({GameCategory::Kind::Source, QString(), m_filterSource}); // Its new games too.
    updateGameCount();
    m_databaseTree->scheduleRefresh();
    m_positionIndexTimer->start();
}

void MainWindow::createPhoneLink()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    m_phoneGameStore = std::make_unique<DatabaseFolderStore>();
    // Games for the open database go through it, on this thread, so its game
    // list stays right; other databases are opened on their own.
    m_phoneGameStore->setOpenDatabase([this] { return m_database.get(); },
                                      [this](int added, const QList<qint64> &updated) {
                                          for (const qint64 index : updated)
                                              m_gameListModel->refreshRow(int(index));
                                          if (added > 0)
                                              showAddedGames();
                                          else if (!updated.isEmpty())
                                              m_positionIndexTimer->start(); // Their moves may differ.
                                      });
    // A sync may be replacing the files: the phone sends its games next time.
    m_phoneGameStore->setBusy([this] { return m_syncPipeline->isRunning(); });
    const QString state = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                              .filePath(QStringLiteral("phone-link.json"));
    m_phoneLink = new PhoneLink(state, UserFolders::databasesDir(), this);
    m_phoneLink->setGameStore(m_phoneGameStore.get());
    // Never asked from inside the phone's session: it is answered first.
    connect(m_phoneLink, &PhoneLink::deletionRequested, this,
            [this] { QTimer::singleShot(0, this, &MainWindow::askAboutPhoneDeletions); });
    // What was left unanswered last time, once the window is up.
    QTimer::singleShot(0, this, &MainWindow::askAboutPhoneDeletions);
#endif
}

void MainWindow::askAboutPhoneDeletions()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    if (!m_phoneLink || m_askingPhoneDeletions)
        return;
    if (m_syncPipeline->isRunning() || m_folderSync->isRunning()) {
        // Files are being replaced: ask when they are not.
        QTimer::singleShot(5000, this, &MainWindow::askAboutPhoneDeletions);
        return;
    }
    m_askingPhoneDeletions = true;
    for (const PhoneLink::DeletionRequest &request : m_phoneLink->deletionRequests()) {
        if (m_postponedPhoneDeletions.contains(request.lineage))
            continue;
        const QString path = m_phoneLink->databasePath(request.lineage);
        if (path.isEmpty()) {
            m_phoneLink->resolveDeletion(request.lineage, false); // Gone already: nothing to ask.
            continue;
        }
        const QString name = QFileInfo(path).completeBaseName();
        const QString phone = request.phoneName.isEmpty() ? tr("Phone") : request.phoneName;
        QMessageBox box(QMessageBox::Question, tr("Database Deleted on the Mobile App"),
                        tr("The database “%1” was deleted on the mobile app “%2”.").arg(name, phone),
                        QMessageBox::NoButton, this);
        box.setInformativeText(tr("Do you want to delete it here too, or keep it on this computer? "
                                  "The mobile app will not receive it again either way."));
        QPushButton *remove = box.addButton(tr("Delete Everywhere…"), QMessageBox::DestructiveRole);
        QPushButton *keep = box.addButton(tr("Keep It"), QMessageBox::AcceptRole);
        QPushButton *later = box.addButton(tr("Ask Me Later"), QMessageBox::RejectRole);
        box.setDefaultButton(keep);
        box.setEscapeButton(later);
        box.exec();
        if (box.clickedButton() == keep) {
            m_phoneLink->resolveDeletion(request.lineage, false);
            continue;
        }
        if (box.clickedButton() != remove) {
            m_postponedPhoneDeletions.insert(request.lineage);
            continue;
        }

        QMessageBox warning(QMessageBox::Warning, tr("Delete “%1” Everywhere?").arg(name),
                            tr("The database “%1” will be deleted from every synced device.").arg(name),
                            QMessageBox::NoButton, this);
        warning.setInformativeText(
            tr("It goes to the trash on this computer, it is removed from the sync folder on the server, and "
               "every other computer that syncs with it deletes its copy at its next sync. Its games go with it."));
        QPushButton *confirm = warning.addButton(tr("Delete"), QMessageBox::DestructiveRole);
        QPushButton *cancel = warning.addButton(QMessageBox::Cancel);
        warning.setDefaultButton(cancel);
        warning.setEscapeButton(cancel);
        warning.exec();
        if (warning.clickedButton() != confirm) {
            m_postponedPhoneDeletions.insert(request.lineage);
            continue;
        }
        QString error;
        if (!m_folderSync->deleteDatabase(path, &error)) {
            QMessageBox::warning(this, tr("Could Not Delete the Database"), error);
            m_postponedPhoneDeletions.insert(request.lineage);
            continue;
        }
        m_phoneLink->resolveDeletion(request.lineage, true);
        statusBar()->showMessage(tr("Deleted “%1”").arg(name), 5000);
        if (m_folderSync->hasStore())
            QTimer::singleShot(0, m_folderSync, &FolderSync::sync); // The other devices hear of it.
    }
    m_askingPhoneDeletions = false;
    // A phone may have deleted more while a question was on screen.
    for (const PhoneLink::DeletionRequest &request : m_phoneLink->deletionRequests()) {
        if (!m_postponedPhoneDeletions.contains(request.lineage)) {
            QTimer::singleShot(0, this, &MainWindow::askAboutPhoneDeletions);
            break;
        }
    }
#endif
}

void MainWindow::askAboutFilesDeletedByHand(const QStringList &paths)
{
    QStringList asked;
    for (const QString &path : paths) {
        if (!m_postponedHandDeletions.contains(path))
            asked << path;
    }
    if (asked.isEmpty() || m_askingHandDeletions)
        return;
    m_askingHandDeletions = true;
    QStringList names;
    for (const QString &path : std::as_const(asked))
        names << QStringLiteral("“%1”").arg(QDir::toNativeSeparators(path));
    QMessageBox box(QMessageBox::Question, tr("Files Deleted from the Pragma Folder"),
                    tr("%n file(s) deleted from this computer's Pragma folder: %1.", nullptr, int(asked.size()))
                        .arg(names.join(QStringLiteral(", "))),
                    QMessageBox::NoButton, this);
    box.setInformativeText(tr("Delete them from every synced device, or bring them back? Deleted everywhere, "
                              "they are removed from the sync folder on the server, and every other computer "
                              "that syncs with it moves its copy to the trash at its next sync."));
    QPushButton *remove = box.addButton(tr("Delete Everywhere"), QMessageBox::DestructiveRole);
    QPushButton *restore = box.addButton(tr("Restore"), QMessageBox::AcceptRole);
    QPushButton *later = box.addButton(tr("Ask Me Later"), QMessageBox::RejectRole);
    box.setDefaultButton(restore);
    box.setEscapeButton(later);
    box.exec();
    m_askingHandDeletions = false;
    if (box.clickedButton() == restore) {
        m_folderSync->restoreDeleted(asked);
    } else if (box.clickedButton() == remove) {
        QString error;
        m_folderSync->deleteEverywhere(asked, &error);
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Could Not Delete the Files"), error);
    } else {
        // Asked again when Pragma Chess starts next.
        for (const QString &path : std::as_const(asked))
            m_postponedHandDeletions.insert(path);
        return;
    }
    QTimer::singleShot(0, m_folderSync, &FolderSync::sync);
}

void MainWindow::openConnectMobileDialog()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    if (!m_phoneLink)
        return;
    UserFolders::ensureDatabasesDir();
    ConnectMobileDialog dialog(m_phoneLink, this);
    dialog.exec();
#endif
}

void MainWindow::createActions()
{
    m_newProjectAction = new QAction(themeIcon("document-new", QStyle::SP_FileIcon), tr("&New Project"), this);
    m_newProjectAction->setShortcut(QKeySequence::New);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::newProject);

    m_openProjectAction = new QAction(themeIcon("document-open", QStyle::SP_DialogOpenButton),
                                      tr("&Open Project…"), this);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);

    m_saveProjectAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                      tr("&Save Project"), this);
    m_saveProjectAction->setShortcut(QKeySequence::Save);
    connect(m_saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveProjectAsAction = new QAction(themeIcon("document-save-as", QStyle::SP_DialogSaveButton),
                                        tr("Save Project &As…"), this);
    m_saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_syncNowAction = new QAction(themeIcon("view-refresh", QStyle::SP_BrowserReload), tr("S&ync Now"), this);
    m_syncNowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    m_syncNowAction->setToolTip(tr("Sync Everything…"));
    connect(m_syncNowAction, &QAction::triggered, this, [this] { syncNow(); });

#ifdef PRAGMA_HAS_PHONE_LINK
    m_connectMobileAction = new QAction(tr("Connect &Mobile App…"), this);
    m_connectMobileAction->setToolTip(tr("Copy your databases to the Pragma Chess app on your phone"));
    connect(m_connectMobileAction, &QAction::triggered, this, &MainWindow::openConnectMobileDialog);
#endif
    m_syncAction = new QAction(tr("&Sync Settings…"), this);
    m_syncAction->setToolTip(tr("Keep databases and projects the same on several computers through a server"));
    connect(m_syncAction, &QAction::triggered, this, &MainWindow::openSyncDialog);

    m_newDatabaseAction = new QAction(themeIcon("document-new", QStyle::SP_FileIcon),
                                      tr("&New Database…"), this);
    connect(m_newDatabaseAction, &QAction::triggered, this, &MainWindow::newDatabase);

    m_openDatabaseAction = new QAction(themeIcon("document-open", QStyle::SP_DialogOpenButton),
                                       tr("&Open Database…"), this);
    connect(m_openDatabaseAction, &QAction::triggered, this, &MainWindow::openDatabase);

    m_saveDatabaseAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                       tr("&Save Database"), this);
    connect(m_saveDatabaseAction, &QAction::triggered, this, &MainWindow::saveDatabase);

    m_saveDatabaseAsAction = new QAction(themeIcon("document-save-as", QStyle::SP_DialogSaveButton),
                                         tr("Save Database &As…"), this);
    connect(m_saveDatabaseAsAction, &QAction::triggered, this, &MainWindow::saveDatabaseAs);

    m_showDatabasesFolderAction = new QAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon),
                                              tr("Show Databases &Folder"), this);
    connect(m_showDatabasesFolderAction, &QAction::triggered, this, [] {
        UserFolders::ensureDatabasesDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::databasesDir()));
    });

    m_connectSourceAction = new QAction(tr("Connect &Source…"), this);
    m_connectSourceAction->setToolTip(tr("Import and keep in sync the games of a lichess.org or chess.com account"));
    connect(m_connectSourceAction, &QAction::triggered, this, &MainWindow::connectSource);

    m_manageSourcesAction = new QAction(tr("&Manage Sources…"), this);
    connect(m_manageSourcesAction, &QAction::triggered, this, &MainWindow::manageSources);

    m_databaseSettingsAction = new QAction(themeIcon("document-properties", QStyle::SP_FileDialogInfoView),
                                           tr("Database Se&ttings…"), this);
    m_databaseSettingsAction->setToolTip(tr("Type and description of the open database"));
    connect(m_databaseSettingsAction, &QAction::triggered, this, &MainWindow::editDatabaseSettings);

    m_quitAction = new QAction(themeIcon("application-exit", QStyle::SP_DialogCloseButton),
                               tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    m_quitAction->setMenuRole(QAction::QuitRole);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_copyFenAction = new QAction(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("Position (&FEN)"), this);
    m_copyFenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_copyFenAction, &QAction::triggered, this, &MainWindow::copyFen);

    m_pasteFenAction = new QAction(tr("Paste &FEN"), this);
    m_pasteFenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(m_pasteFenAction, &QAction::triggered, this, &MainWindow::pasteFen);

    m_firstMoveAction = new QAction(themeIcon("go-first", QStyle::SP_MediaSkipBackward),
                                    tr("&First Move"), this);
    m_firstMoveAction->setShortcut(Qt::Key_Home);
    connect(m_firstMoveAction, &QAction::triggered, m_session, &GameSession::goToStart);

    m_previousMoveAction = new QAction(themeIcon("go-previous", QStyle::SP_MediaSeekBackward),
                                       tr("&Previous Move"), this);
    m_previousMoveAction->setShortcut(Qt::Key_Left);
    connect(m_previousMoveAction, &QAction::triggered, m_session, &GameSession::goBack);

    m_nextMoveAction = new QAction(themeIcon("go-next", QStyle::SP_MediaSeekForward),
                                   tr("&Next Move"), this);
    m_nextMoveAction->setShortcut(Qt::Key_Right);
    connect(m_nextMoveAction, &QAction::triggered, m_session, &GameSession::goForward);

    m_lastMoveAction = new QAction(themeIcon("go-last", QStyle::SP_MediaSkipForward),
                                   tr("&Last Move"), this);
    m_lastMoveAction->setShortcut(Qt::Key_End);
    connect(m_lastMoveAction, &QAction::triggered, m_session, &GameSession::goToEnd);

    m_flipBoardAction = new QAction(themeIcon("object-flip-vertical", QStyle::SP_BrowserReload),
                                    tr("&Flip Board"), this);
    m_flipBoardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_flipBoardAction->setCheckable(true);
    connect(m_flipBoardAction, &QAction::toggled, m_board, &BoardWidget::setFlipped);
    connect(m_flipBoardAction, &QAction::toggled, m_evaluationBar, &EvaluationBar::setFlipped);
    connect(m_flipBoardAction, &QAction::toggled, m_capturedPieces, &CapturedPiecesWidget::setFlipped);
    connect(m_flipBoardAction, &QAction::toggled, m_boardSideColumn, &BoardSideColumn::setFlipped);

    m_defaultLayoutAction = new QAction(tr("&Reset Panel Layout"), this);
    m_defaultLayoutAction->setToolTip(tr("Put the panels back where a new project starts"));
    connect(m_defaultLayoutAction, &QAction::triggered, this, &MainWindow::applyDefaultLayout);

    m_coordinatesAction = new QAction(tr("Show &Coordinates"), this);
    m_coordinatesAction->setCheckable(true);
    m_coordinatesAction->setChecked(true);
    connect(m_coordinatesAction, &QAction::toggled, m_board, &BoardWidget::setShowCoordinates);

    m_newGameAction = new QAction(themeIcon("pragma-new-game", QStyle::SP_FileIcon), tr("&New Game"), this);
    m_newGameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_newGameAction->setToolTip(tr("Start a game to enter move by move"));
    connect(m_newGameAction, &QAction::triggered, this, &MainWindow::newGame);

    m_newTrainingAction = new QAction(themeIcon("pragma-training", QStyle::SP_MediaPlay), tr("New &Training…"), this);
    m_newTrainingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    m_newTrainingAction->setToolTip(tr("Start a game against the engine, choosing your colour"));
    connect(m_newTrainingAction, &QAction::triggered, this, [this] { newTraining(true); });
    // The toolbar's button skips the question once an answer was remembered.
    m_quickTrainingAction = new QAction(m_newTrainingAction->icon(), m_newTrainingAction->text(), this);
    m_quickTrainingAction->setToolTip(m_newTrainingAction->toolTip());
    connect(m_quickTrainingAction, &QAction::triggered, this, [this] { newTraining(false); });

    m_playOnlineAction = new QAction(themeIcon("pragma-online", QStyle::SP_ComputerIcon), tr("New &Online Game…"), this);
    m_playOnlineAction->setToolTip(tr("Play a game against a person on lichess.org, with one of your accounts"));
    connect(m_playOnlineAction, &QAction::triggered, this, [this] { playOnline(true); });
    m_lobbyAction = new QAction(themeIcon("pragma-lobby", QStyle::SP_ComputerIcon), tr("Enter the &Lobby…"), this);
    m_lobbyAction->setToolTip(tr("Tournaments of four players without a clock: sit at a table with a free seat"));
    connect(m_lobbyAction, &QAction::triggered, this, &MainWindow::showLobby);
#ifndef PRAGMA_HAS_PHONE_LINK
    m_lobbyAction->setEnabled(false); // The lobby is on the network: built without it, there is none.
    m_lobbyAction->setToolTip(tr("This build has no network: the lobby needs the libraries the mobile app connects with"));
#endif
    // The toolbar's button skips the question once an answer was remembered.
    m_quickOnlineAction = new QAction(m_playOnlineAction->icon(), m_playOnlineAction->text(), this);
    m_quickOnlineAction->setToolTip(m_playOnlineAction->toolTip());
    connect(m_quickOnlineAction, &QAction::triggered, this, [this] { playOnline(false); });

    m_setUpPositionAction = new QAction(tr("Set &Up Position…"), this);
    m_setUpPositionAction->setToolTip(tr("Draw a position on a board and start a game from it"));
    connect(m_setUpPositionAction, &QAction::triggered, this, &MainWindow::setUpPosition);

    m_trainingModeAction = new QAction(tr("&Training Mode"), this);
    m_trainingModeAction->setCheckable(true);
    m_trainingModeAction->setToolTip(tr("The engine answers as the other colour and hides its line while you think"));
    connect(m_trainingModeAction, &QAction::toggled, this, &MainWindow::setTrainingMode);

    m_saveGameAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                   tr("Save Game to &Database"), this);
    connect(m_saveGameAction, &QAction::triggered, this, &MainWindow::saveGameToDatabase);
    m_saveGameElsewhereAction = new QAction(tr("Save Game to Another D&atabase…"), this);
    m_saveGameElsewhereAction->setToolTip(tr("Save the game on the board to a database you choose; the open database stays open"));
    connect(m_saveGameElsewhereAction, &QAction::triggered, this, &MainWindow::saveGameToAnotherDatabase);

    m_explainAction = new QAction(themeIcon("pragma-explain", QStyle::SP_MessageBoxQuestion), tr("E&xplain"), this);
    m_explainAction->setShortcut(Qt::Key_E);
    m_explainAction->setCheckable(true);
    m_explainAction->setToolTip(tr("Explain the evaluation: show on the board what the last move allows or wins (E)"));
    connect(m_explainAction, &QAction::toggled, this, &MainWindow::setExplainEnabled);

    m_startEngineAction = new QAction(themeIcon("media-playback-start", QStyle::SP_MediaPlay),
                                      tr("&Analyze"), this);
    m_startEngineAction->setCheckable(true);
    m_startEngineAction->setToolTip(tr("Analyze the position with the engine"));
    connect(m_startEngineAction, &QAction::toggled, this, &MainWindow::setAnalysisEnabled);
    // The menu's entry keeps one name and a check mark; the Engine panel's
    // button (m_startEngineAction) says Analyze or Stop Analysis.
    m_analysisAction = new QAction(tr("&Analysis"), this);
    m_analysisAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    m_analysisAction->setCheckable(true);
    m_analysisAction->setToolTip(m_startEngineAction->toolTip());
    connect(m_analysisAction, &QAction::toggled, m_startEngineAction, &QAction::setChecked);
    connect(m_startEngineAction, &QAction::toggled, m_analysisAction, &QAction::setChecked);
    connect(m_startEngineAction, &QAction::enabledChanged, m_analysisAction, &QAction::setEnabled);

    // Checked while looking for an opponent or playing: chosen, it starts or stops playing online.
    m_onlineModeAction = new QAction(tr("&Online Play Mode"), this);
    m_onlineModeAction->setCheckable(true);
    m_onlineModeAction->setToolTip(tr("Play against a person on a platform: the engine and Explain stay off"));
    connect(m_onlineModeAction, &QAction::triggered, this, [this] {
        m_onlineModeAction->setChecked(m_onlinePlay); // The mode follows the play, not the click.
        if (m_onlinePlay)
            stopOnline();
        else
            playOnline(true);
    });

    // Checked while a lobby game is on the board: the Engine panel sends its moves.
    // Nothing is turned off: correspondence play welcomes engines and preparation.
    m_lobbyModeAction = new QAction(tr("&Lobby Mode"), this);
    m_lobbyModeAction->setCheckable(true);
    m_lobbyModeAction->setEnabled(false);
    m_lobbyModeAction->setToolTip(tr("A game of the lobby on the board: send your move, or the plan you prepared, "
                                     "from the Engine panel"));
    connect(m_lobbyModeAction, &QAction::toggled, this, &MainWindow::updateLobbyPanel);

    m_aboutAction = new QAction(themeIcon("help-about", QStyle::SP_MessageBoxInformation),
                                tr("&About Pragma Chess"), this);
    m_aboutAction->setMenuRole(QAction::AboutRole);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    m_guideAction = new QAction(tr("Pragma Chess &Guide"), this);
    m_guideAction->setShortcut(QKeySequence::HelpContents);
    connect(m_guideAction, &QAction::triggered, this, &MainWindow::showGuide);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newProjectAction);
    file->addAction(m_openProjectAction);
    m_recentProjectsMenu = file->addMenu(tr("Open &Recent Project"));
    connect(m_recentProjectsMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentProjectsMenu);
    file->addSeparator();
    file->addAction(m_saveProjectAction);
    file->addAction(m_saveProjectAsAction);
    file->addSeparator();
    // The project's chapters: its games one after the other, as in a book.
    file->addAction(tr("New C&hapter…"), this, &MainWindow::newChapter);
    m_switchChapterMenu = file->addMenu(tr("S&witch Chapter"));
    connect(m_switchChapterMenu, &QMenu::aboutToShow, this, &MainWindow::fillChapterMenu);
    file->addAction(tr("Project Se&ttings…"), this, &MainWindow::editProjectSettings);
    file->addSeparator();
    // The toolbar's first button; its settings are in Options.
    file->addAction(m_syncNowAction);
    file->addSeparator();
    file->addAction(m_quitAction);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    QMenu *copy = edit->addMenu(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("&Copy"));
    QAction *copyMovesToHere = copy->addAction(tr("&Moves up to Current Position"), this, [this] {
        copyText(Pgn::moveText(m_session->game(), m_session->ply()), tr("Moves up to the current position copied"));
    });
    copyMovesToHere->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
    QAction *copyAllMoves = copy->addAction(tr("&All Moves"), this, [this] {
        copyText(Pgn::moveText(m_session->game()), tr("Moves copied"));
    });
    QAction *copyCurrentMove = copy->addAction(tr("Current &Move"), this, [this] {
        copyText(moveText(m_session->ply()), tr("Move copied"));
    });
    copy->addSeparator();
    QAction *copyPgn = copy->addAction(tr("Game as &PGN"), this, [this] {
        copyText(Pgn::game(m_session->game()), tr("Game copied as PGN"));
    });
    QAction *copyPgnToHere = copy->addAction(tr("Game up to Current Position as P&GN"), this, [this] {
        copyText(Pgn::game(m_session->game(), m_session->ply()), tr("Game up to the current position copied as PGN"));
    });
    copy->addSeparator();
    copy->addAction(m_copyFenAction);
    copy->addSeparator();
    QAction *copyEngineLine = copy->addAction(tr("&Engine Line"), this, [this] {
        copyText(m_engineLine, tr("Engine line copied"));
    });
    QAction *copyExplanation = copy->addAction(tr("E&xplanation"), this, [this] {
        copyText(m_explanationText, tr("Explanation copied"));
    });
    const auto updateCopyActions = [=, this] {
        const bool hasMoves = m_session->plyCount() > 0;
        copyMovesToHere->setEnabled(m_session->ply() > 0);
        copyCurrentMove->setEnabled(m_session->ply() > 0);
        copyAllMoves->setEnabled(hasMoves);
        copyPgnToHere->setEnabled(m_session->ply() > 0);
        copyPgn->setEnabled(true);
        copyEngineLine->setEnabled(m_startEngineAction->isChecked() && !m_engineLine.isEmpty());
        copyExplanation->setEnabled(m_explainAction->isChecked() && !m_explanationText.isEmpty());
    };
    connect(copy, &QMenu::aboutToShow, this, updateCopyActions);
    connect(m_session, &GameSession::plyChanged, this, updateCopyActions); // Keeps shortcuts in step.
    connect(m_session, &GameSession::gameChanged, this, updateCopyActions);
    updateCopyActions();
    edit->addSeparator();
    QMenu *paste = edit->addMenu(themeIcon("edit-paste", QStyle::SP_FileIcon), tr("&Paste"));
    paste->addAction(m_pasteFenAction);
    paste->addAction(tr("Paste &Line"), this, &MainWindow::pasteLine);
    QAction *pasteHere = paste->addAction(tr("Paste Line from &Current Position"), this,
                                          &MainWindow::pasteLineFromCurrentPosition);
    pasteHere->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V)); // As Copy ▸ Moves up to Current Position.
    // Online the moves are the platform's: none are played from the clipboard.
    connect(paste, &QMenu::aboutToShow, this, [this, pasteHere] { pasteHere->setEnabled(!m_onlinePlay); });
    edit->addSeparator();
    QAction *drawers = edit->addAction(tr("&Drawers…"), this, &MainWindow::manageDrawers);
    drawers->setToolTip(tr("Named drawers for moves, variations, positions and notes, the same on every synced computer"));

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_flipBoardAction);
    m_viewMenu->addAction(m_coordinatesAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_mainToolBar->toggleViewAction());
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock, m_gamesDock})
        m_viewMenu->addAction(dock->toggleViewAction());
    m_gamesDock->toggleViewAction()->setText(tr("Games &List")); // The panel is the tree and the list, last in the menu.
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_defaultLayoutAction);

    QMenu *game = menuBar()->addMenu(tr("&Game"));
    game->addAction(m_newGameAction);
    game->addAction(m_newTrainingAction);
    game->addAction(m_playOnlineAction);
    game->addAction(m_lobbyAction);
    game->addAction(m_setUpPositionAction);
    game->addAction(m_saveGameAction);
    game->addAction(m_saveGameElsewhereAction);
    game->addSeparator();
    game->addAction(m_firstMoveAction);
    game->addAction(m_previousMoveAction);
    game->addAction(m_nextMoveAction);
    game->addAction(m_lastMoveAction);
    game->addSeparator();
    game->addAction(m_explainAction);

    // As the Database menu: new and open on top, the books to switch to in a submenu.
    m_bookMenu = menuBar()->addMenu(tr("&Book"));
    m_bookMenu->addAction(themeIcon("document-new", QStyle::SP_FileIcon), tr("N&ew Book…"), this, &MainWindow::newBook);
    m_bookMenu->addAction(tr("&Open Book…"), this, &MainWindow::openBookFile);
    m_switchBookMenu = m_bookMenu->addMenu(themeIcon("folder", QStyle::SP_DirIcon), tr("S&witch Book"));
    connect(m_switchBookMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildBookMenu);
    rebuildBookMenu();

    QMenu *engine = menuBar()->addMenu(tr("E&ngine"));
    engine->addAction(m_analysisAction);
    engine->addAction(m_explainAction);
    engine->addAction(m_trainingModeAction);
    engine->addAction(m_onlineModeAction);
    engine->addAction(m_lobbyModeAction);
    engine->addSeparator();
    m_engineChoiceMenu = engine->addMenu(tr("S&witch Engine"));
    connect(m_engineChoiceMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildEngineChoiceMenu);
    rebuildEngineChoiceMenu();
    engine->addAction(tr("&Manage Engines…"), this, &MainWindow::manageEngines);
    engine->addSeparator();
    // A wrong explanation as a case for pragma-explain --replay and smart/tests.
    QAction *copyTicks = engine->addAction(tr("&Copy Explain's Ticks"), this, [this] {
        const QString ticks = m_explainer->recordedTicks();
        if (ticks.isEmpty())
            statusBar()->showMessage(tr("Explain has not explained a move yet"), 4000);
        else
            copyText(ticks, tr("Explain's ticks copied: paste them into a .ticks file to replay them"));
    });
    copyTicks->setToolTip(tr("Copy what the engine told Explain about the move, to replay it with pragma-explain --replay"));
    engine->setToolTipsVisible(true);

    QMenu *database = menuBar()->addMenu(tr("&Database"));
    database->addAction(m_newDatabaseAction);
    database->addAction(m_openDatabaseAction);
    m_databasesMenu = database->addMenu(themeIcon("folder", QStyle::SP_DirIcon), tr("S&witch Database"));
    connect(m_databasesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildDatabasesMenu);
    database->addSeparator();
    database->addAction(m_connectSourceAction);
    database->addAction(m_manageSourcesAction);
    database->addSeparator();
    database->addAction(m_databaseSettingsAction);
    database->addSeparator();
    database->addAction(m_saveDatabaseAction);
    database->addAction(m_saveDatabaseAsAction);

    menuBar()->addMenu(tr("&Tools"))->setEnabled(false);

    QMenu *options = menuBar()->addMenu(tr("&Options"));
    // How this computer reaches the others: the phone, the server.
    if (m_connectMobileAction)
        options->addAction(m_connectMobileAction);
    options->addAction(m_syncAction);
    options->addSeparator();
    options->addAction(tr("&Graphics Settings…"), this, &MainWindow::editGraphicsSettings);
    options->addAction(tr("&Folder Settings…"), this, &MainWindow::editFolderSettings);
    options->addAction(tr("&Personal Settings…"), this, &MainWindow::editPersonalSettings);
    m_openingNamesMenu = options->addMenu(tr("Switch Opening &Names"));
    m_openingNamesMenu->setToolTip(tr("The database whose games name the openings and variations"));
    connect(m_openingNamesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildOpeningNamesMenu);
    rebuildOpeningNamesMenu();
    QMenu *language = options->addMenu(tr("Switch &Language"));
    auto *languages = new QActionGroup(language);
    for (const UiLanguage::Language &entry : UiLanguage::available()) {
        QAction *action = language->addAction(entry.name);
        action->setCheckable(true);
        action->setActionGroup(languages);
        action->setChecked(entry.code == UiLanguage::chosen());
        const QString code = entry.code;
        connect(action, &QAction::triggered, this, [this, code] {
            if (code == UiLanguage::chosen())
                return;
            UiLanguage::setChosen(code);
            QMessageBox::information(this, tr("Language"),
                                     tr("The new language is used the next time Pragma Chess starts."));
        });
    }

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_guideAction);
    help->addSeparator();
    help->addAction(m_aboutAction);
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    m_mainToolBar = toolBar;
    toolBar->toggleViewAction()->setText(tr("&Toolbar"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    // The size the icons are drawn for, on every system: Linux's and Windows'
    // styles give 24, macOS's 32, which made the toolbar too large there.
    toolBar->setIconSize(QSize(24, 24));
    // Syncing everything is the one button that stands on its own.
    toolBar->addAction(m_syncNowAction);
    toolBar->addSeparator();
    // Saving: the project for now (the floppy).
    toolBar->addAction(m_saveProjectAction);
    toolBar->addSeparator();
    toolBar->addAction(m_newGameAction);
    toolBar->addAction(m_quickTrainingAction);
    toolBar->addAction(m_quickOnlineAction);
    toolBar->addAction(m_lobbyAction); // The fourth game: the lobby, a martini glass in the square.
    // Some air around each icon: bigger buttons to aim at, the same icons.
    constexpr int kButtonPadding = 4;
    // The book, the engine and the database in use: an icon each, which
    // drops down the list to choose another (the menus of the menu bar, set
    // in createMenus()); the tooltip says which one is in use.
    toolBar->addSeparator();
    const auto resourceButton = [toolBar](const char *icon) {
        auto *button = new QToolButton(toolBar);
        button->setIcon(themeIcon(icon, QStyle::SP_FileIcon));
        button->setPopupMode(QToolButton::InstantPopup);
        button->setFocusPolicy(Qt::NoFocus);
        toolBar->addWidget(button);
        return button;
    };
    m_bookButton = resourceButton("pragma-book");
    m_engineButton = resourceButton("pragma-engine");
    m_databaseButton = resourceButton("pragma-database");

    for (QAction *action : toolBar->actions()) {
        if (QWidget *button = toolBar->widgetForAction(action); button && !action->isSeparator())
            button->setMinimumSize(button->sizeHint() + 2 * QSize(kButtonPadding, kButtonPadding));
    }
}

void MainWindow::updateResourceButtons()
{
    if (!m_bookButton) // Called while the window is still being put together.
        return;
    const QString book = m_book ? QFileInfo(m_book->path()).completeBaseName() : tr("No Book");
    m_bookButton->setToolTip(tr("Opening book: %1").arg(book));
    const QString engine = m_engines.resolve(m_engineId, m_engineName).name;
    m_engineButton->setToolTip(tr("Engine: %1").arg(engine));
    const QString database = m_database ? m_database->name() : tr("No Database");
    m_databaseButton->setToolTip(tr("Database: %1").arg(database));
}

QDockWidget *MainWindow::addDock(QMainWindow *host, const QString &objectName, const QString &title,
                                 QWidget *widget, Qt::DockWidgetArea area)
{
    auto *dock = new QDockWidget(title, host);
    dock->setObjectName(objectName);
    // Closable, or Qt would disable the toggleViewAction the View menu shows;
    // the empty title bar set below keeps the close and float buttons hidden.
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    dock->setWidget(widget);
    host->addDockWidget(area, dock);
    return dock;
}

WorkspaceLayout MainWindow::captureLayout()
{
    WorkspaceLayout layout = m_layout;
    layout.toolbar = m_mainToolBar->isVisibleTo(this);
    layout.moves = m_movesDock->isVisibleTo(this);
    layout.openingTree = m_openingTreeDock->isVisibleTo(this);
    layout.engine = m_engineDock->isVisibleTo(this);
    layout.games = m_gamesDock->isVisibleTo(this);
    if (!isVisible() || m_layoutPending)
        return layout; // Nothing on screen to measure (closing, or not yet shown): the last shares.
    const auto percent = [](int part, int whole) {
        return whole > 0 ? WorkspaceLayout::clamped(100.0 * part / whole) : -1.0;
    };
    // Only what is on screen is measured: a hidden panel keeps its share.
    if (layout.games) {
        const int usable = centralWidget()->height() + m_gamesDock->height();
        if (const double share = percent(m_gamesDock->height(), usable); share > 0)
            layout.gamesHeight = share;
    }
    if (layout.moves && layout.openingTree) {
        if (const double share = percent(m_movesDock->width(), m_movesDock->width() + m_openingTreeDock->width()); share > 0)
            layout.movesWidth = share;
    }
    if (layout.engine && (layout.moves || layout.openingTree)) {
        if (const double share = percent(m_engineDock->height(), m_sidebar->height()); share > 0)
            layout.engineHeight = share;
    }
    const QList<int> sizes = m_gamesSplitter->sizes();
    if (sizes.size() == 2 && layout.games) {
        if (const double share = percent(sizes.at(0), sizes.at(0) + sizes.at(1)); share > 0)
            layout.treeWidth = share;
    }
    m_layout = layout; // Remembered for when there is nothing to measure.
    return layout;
}

void MainWindow::applyLayout(const WorkspaceLayout &layout)
{
    m_layout = layout;
    // The arrangement is fixed: Games below, Moves | Opening Tree over Engine at the right.
    for (QDockWidget *dock : {m_movesDock, m_gamesDock, m_openingTreeDock, m_engineDock})
        dock->setFloating(false);
    addDockWidget(Qt::BottomDockWidgetArea, m_gamesDock);
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock})
        m_sidebar->addDockWidget(Qt::RightDockWidgetArea, dock);
    m_sidebar->splitDockWidget(m_movesDock, m_openingTreeDock, Qt::Horizontal);
    m_mainToolBar->setVisible(layout.toolbar);
    m_movesDock->setVisible(layout.moves);
    m_openingTreeDock->setVisible(layout.openingTree);
    m_engineDock->setVisible(layout.engine);
    m_gamesDock->setVisible(layout.games);
    m_layoutPending = true;
    if (isVisible())
        QTimer::singleShot(0, this, &MainWindow::applyLayoutShares); // Once this round of layout is done.
}

void MainWindow::applyLayoutShares()
{
    if (!m_layoutPending || !isVisible())
        return;
    m_layoutPending = false;
    const WorkspaceLayout &layout = m_layout;
    // Shares into pixels, out of the room the panels have now.
    if (layout.games) {
        const int usable = centralWidget()->height() + m_gamesDock->height();
        resizeDocks({m_gamesDock}, {WorkspaceLayout::pixels(layout.gamesHeight, usable)}, Qt::Vertical);
    }
    if (layout.moves && layout.openingTree) {
        const int row = m_movesDock->width() + m_openingTreeDock->width();
        const int moves = WorkspaceLayout::pixels(layout.movesWidth, row);
        m_sidebar->resizeDocks({m_movesDock, m_openingTreeDock}, {moves, row - moves}, Qt::Horizontal);
    }
    if (layout.engine && (layout.moves || layout.openingTree)) {
        const int column = m_sidebar->height();
        m_sidebar->resizeDocks({m_engineDock}, {WorkspaceLayout::pixels(layout.engineHeight, column)}, Qt::Vertical);
    }
    const int width = m_gamesSplitter->width();
    if (width > 0) {
        const int tree = WorkspaceLayout::pixels(layout.treeWidth, width);
        m_gamesSplitter->setSizes({tree, width - tree});
    }
}

void MainWindow::restoreLegacyLayout(const QByteArray &layout)
{
    QDataStream stream(layout);
    QByteArray magic;
    QByteArray windowState;
    QByteArray sidebarState;
    QByteArray splitterState;
    stream >> magic;
    if (magic != "pragma-layout-4" && magic != "pragma-layout-3" && magic != "pragma-layout-2") {
        // Layouts from before the sidebar existed only hold the window state.
        restoreState(layout);
        return;
    }
    stream >> windowState >> sidebarState;
    restoreState(windowState);
    // Version 2 stacked the opening tree under the moves: keep the default sidebar.
    if (magic == "pragma-layout-2")
        return;
    m_sidebar->restoreState(sidebarState);
    // Version 4 added the tree | games list ruler inside the Games panel.
    if (magic == "pragma-layout-4") {
        stream >> splitterState;
        m_gamesSplitter->restoreState(splitterState);
    }
}

void MainWindow::createDocks()
{
    m_moveView = new MoveTreeView(m_session);
    connect(m_moveView, &MoveTreeView::moveActivated, m_session, &GameSession::goToLine);
    m_moveView->setBook(&m_chapters);
    connect(m_moveView, &MoveTreeView::gameMoveActivated, this, &MainWindow::switchToChapterGame);
    connect(m_moveView, &MoveTreeView::paragraphEdited, this, [this](int game, int index, const QString &text) {
        if (game < m_chapters.chapter().games.size()) {
            // A text shown from another language and left as it was is no translation: nothing is written.
            const LocalizedText &was = m_chapters.chapter().games.at(game).paragraphs.value(index).text;
            if (was.has(m_chapters.language) || text != was.text(m_chapters.language))
                m_chapters.setParagraph(game, index, text);
        }
        chapterChanged();
    });
    connect(m_moveView, &MoveTreeView::commentEdited, this, &MainWindow::writeComment);
    connect(m_moveView, &MoveTreeView::commentLineActivated, this, &MainWindow::playCommentLine);
    m_moveView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_moveView, &QWidget::customContextMenuRequested, this, &MainWindow::showMoveListMenu);
    m_movesDock = addDock(m_sidebar, QStringLiteral("movesDock"), tr("Moves"), m_moveView, Qt::RightDockWidgetArea);

    m_enginePanel = new EnginePanel(m_startEngineAction, m_explainAction);
    {
        QSettings settings;
        m_engines = EngineCatalog::load(settings);
    }
    m_enginePanel->setEngineName(m_engines.resolve(m_engineId, m_engineName).name);
    connect(m_enginePanel, &EnginePanel::takeBackRequested, this, &MainWindow::takeBackTutorMove);
    connect(m_enginePanel, &EnginePanel::ignoreRequested, this, &MainWindow::ignoreTutorAlert);
    connect(m_enginePanel, &EnginePanel::sendMoveRequested, this, [this] { sendLobby(false); });
    connect(m_enginePanel, &EnginePanel::sendPlanRequested, this, [this] { sendLobby(true); });
    connect(m_enginePanel, &EnginePanel::peekHeld, this, &MainWindow::peekAtEngineLine);
    m_engineDock = addDock(m_sidebar, QStringLiteral("engineDock"), tr("Engine"), m_enginePanel, Qt::RightDockWidgetArea);

    m_bookPanel = new BookPanel;
    connect(m_bookPanel, &BookPanel::moveActivated, this, &MainWindow::playUserMove);
    connect(m_bookPanel, &BookPanel::backActivated, m_session, &GameSession::goBack);
    connect(m_bookPanel, &BookPanel::repertoireToggled, this, [this](const ChessMove &move, bool inRepertoire) {
        QString error;
        if (!m_book || !m_book->setInRepertoire(m_session->position(), move, inRepertoire, &error))
            QMessageBox::warning(this, tr("Repertoire"), tr("Could not change the book: %1").arg(error));
        updateBookMoves();
    });
    connect(m_bookPanel, &BookPanel::weightAdjustRequested, this, [this](const ChessMove &move, int percent) {
        // The shares of the position's moves shift and are written into the book in use.
        if (!m_book)
            return;
        const ChessPosition &position = m_session->position();
        QList<PolyglotBook::Move> moves = m_book->moves(position);
        QList<int> weights;
        int index = -1;
        for (int i = 0; i < moves.size(); ++i) {
            weights << moves.at(i).weight;
            if (moves.at(i).move == move)
                index = i;
        }
        if (index < 0)
            return;
        const QList<int> changed = percent == 0 ? BookWeights::zeroed(weights, index)
                                                : BookWeights::adjusted(weights, index, percent);
        if (changed == weights)
            return;
        for (int i = 0; i < moves.size(); ++i)
            moves[i].weight = changed.at(i);
        QString error;
        if (!m_book->setWeights(position, moves, &error))
            QMessageBox::warning(this, tr("Opening Book"), tr("Could not change the book: %1").arg(error));
        updateBookMoves();
    });
    m_openingTreeDock = addDock(m_sidebar, QStringLiteral("openingTreeDock"), tr("Opening Tree"), m_bookPanel,
                                Qt::RightDockWidgetArea);
    // The panels speak for themselves: no title bars (the names stay in the View menu).
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock})
        dock->setTitleBarWidget(new QWidget(dock));
    // The opening tree sits right of the moves, with a separator to resize both.
    m_sidebar->splitDockWidget(m_movesDock, m_openingTreeDock, Qt::Horizontal);

    m_gameView = new QTableView;
    // The column titles with the padding of the Moves and Opening Tree headers.
    PaddedHeaderView::install(m_gameView, false);
    m_gameView->setModel(m_gameListProxy);
    m_gameView->setFont(FigurineFont::apply(m_gameView->font())); // The Line column shows moves.
    m_gameView->setSortingEnabled(true);
    m_gameView->sortByColumn(GameListModel::Number, Qt::AscendingOrder);
    m_gameView->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Shift and Ctrl select several games, for what the menu does to them all.
    m_gameView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gameView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gameView->setAlternatingRowColors(true);
    m_gameView->setShowGrid(false);
    m_gameView->setWordWrap(false);
    m_gameView->verticalHeader()->hide();
    m_gameView->horizontalHeader()->setStretchLastSection(true);
    m_gameView->horizontalHeader()->setSectionsMovable(true);
    connect(m_gameView, &QTableView::activated, this, &MainWindow::openGame);
    m_gameView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gameView, &QWidget::customContextMenuRequested, this, &MainWindow::showGameListMenu);
    m_gameView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gameView->horizontalHeader(), &QWidget::customContextMenuRequested, this,
            &MainWindow::showGameColumnsMenu);

    m_databaseTree = new DatabaseTreeWidget;
    connect(m_databaseTree, &DatabaseTreeWidget::categorySelected, this, &MainWindow::showCategory);
    // A leaf with one game: double-clicking it opens that game, as double-clicking it in the list.
    connect(m_databaseTree, &DatabaseTreeWidget::leafActivated, this, [this] {
        if (m_gameListProxy->rowCount() == 1)
            openGame(m_gameListProxy->index(0, 0));
    });
    connect(m_databaseTree, &DatabaseTreeWidget::stateChanged, this, &MainWindow::saveTreeState);
    connect(m_databaseTree, &DatabaseTreeWidget::connectSourceRequested, this, &MainWindow::connectSource);
    connect(m_databaseTree, &DatabaseTreeWidget::settingsRequested, this, &MainWindow::editDatabaseSettings);
    connect(m_databaseTree, &DatabaseTreeWidget::manageSourcesRequested, this, &MainWindow::manageSources);
    connect(m_databaseTree, &DatabaseTreeWidget::syncSourceRequested, this,
            [this](qint64 id) { m_sourceSync->syncSource(id); });

    // A vertical ruler between the tree and the list resizes them.
    m_gamesSplitter = new QSplitter(Qt::Horizontal);
    m_gamesSplitter->setObjectName(QStringLiteral("gamesSplitter"));
    m_gamesSplitter->setChildrenCollapsible(false);
    m_gamesSplitter->addWidget(m_databaseTree);
    m_gamesSplitter->addWidget(m_gameView);
    m_gamesSplitter->setStretchFactor(0, 0);
    m_gamesSplitter->setStretchFactor(1, 1);
    m_gamesSplitter->setSizes({220, 800});
    m_gamesDock = addDock(this, QStringLiteral("gamesDock"), tr("Games"), m_gamesSplitter, Qt::BottomDockWidgetArea);
    // The tree and the list speak for themselves: no title bar ("Games" stays in the View menu).
    m_gamesDock->setTitleBarWidget(new QWidget(m_gamesDock));
}

void MainWindow::createStatusBar()
{
    auto *bar = new PaddedStatusBar;
    setStatusBar(bar);

    // The sections on the right, each set apart by a thin line.
    m_folderSyncLabel = new QLabel;
    m_folderSyncLabel->hide();
    bar->addSection(m_folderSyncLabel);

    m_syncLabel = new QLabel;
    m_syncLabel->hide();
    bar->addSection(m_syncLabel);

    // The lobby's network, as it is (IDEA.md §38): shown while the lobby is on.
    m_lobbyNetworkLabel = new QLabel;
    m_lobbyNetworkLabel->hide();
    bar->addSection(m_lobbyNetworkLabel);

    m_gameCountLabel = new QLabel;
    bar->addSection(m_gameCountLabel);
}

void MainWindow::updateLobbyNetwork()
{
    if (!m_lobbyService) {
        m_lobbyNetworkLabel->hide();
        return;
    }
    if (!m_lobbyService->isOnline()) {
        m_lobbyNetworkLabel->setText(tr("Lobby: not on the network"));
        m_lobbyNetworkLabel->setToolTip(tr("No relay or peer can be reached: what you send leaves when one can"));
    } else {
        m_lobbyNetworkLabel->setText(tr("Lobby: %1, %2")
                                         .arg(tr("%n relay(s)", nullptr, m_lobbyService->relayCount()),
                                              tr("%n peer(s)", nullptr, m_lobbyService->peerCount())));
        m_lobbyNetworkLabel->setToolTip(tr("The lobby's network: the relays that keep its moves, and the other "
                                           "players' computers connected directly"));
    }
    m_lobbyNetworkLabel->show();
}

void MainWindow::setDatabase(std::unique_ptr<GameDatabase> database)
{
    m_sourceSync->setDatabase(nullptr); // Before the old database goes away.
    // The filter too: it reads the headers as soon as the list is reset.
    m_gameListProxy->setDatabase(nullptr);
    m_gameListModel->setDatabase(nullptr);
    m_database = std::move(database);
    m_gameListModel->setDatabase(m_database.get());
    m_gameView->resizeColumnsToContents();
    applyGameColumns();
    updateResourceButtons();
    m_openGameIndex = -1;

    updateGameCount();
    updateDatabaseActions();
    updateGameActions();
    m_filterSource = 0;
    m_category = {};
    m_gameListProxy->setDatabase(m_database.get());
    m_databaseTree->setDatabase(m_database.get());
    restoreTreeState();
    m_sourceSync->setDatabase(m_database.get());
    m_positionIndex->clear(); // Its games are another database's.
    rebuildPositionIndex();
}

QString MainWindow::treeStateKey() const
{
    // Per database, by its file: the tree of each comes back as it was left.
    const QByteArray location = m_database ? m_database->location().toUtf8() : QByteArray();
    return QStringLiteral("databaseTree/")
        + QString::fromLatin1(QCryptographicHash::hash(location, QCryptographicHash::Sha1).toHex());
}

void MainWindow::saveTreeState()
{
    if (!m_database)
        return;
    QSettings settings;
    settings.setValue(treeStateKey() + QStringLiteral("/expanded"), m_databaseTree->expandedKeys());
    settings.setValue(treeStateKey() + QStringLiteral("/selected"), m_databaseTree->selectedKey());
}

void MainWindow::restoreTreeState()
{
    if (!m_database)
        return;
    const QSettings settings;
    const QString key = treeStateKey();
    if (!settings.contains(key + QStringLiteral("/expanded")))
        return; // Never left: as the tree opens by itself.
    m_databaseTree->restoreState(settings.value(key + QStringLiteral("/expanded")).toStringList(),
                                 settings.value(key + QStringLiteral("/selected")).toString());
}

void MainWindow::rebuildPositionIndex()
{
    m_positionIndexTimer->stop();
    // Games were added or changed: the sources that write them out follow.
    m_sourceSync->scheduleWrite();
    if (m_database)
        m_positionIndex->build(m_database->gameLines());
    else
        m_positionIndex->clear();
    updateBoardFilters();
}

void MainWindow::updateBoardFilters()
{
    if (const PositionIndex *index = m_positionIndex->index()) {
        m_databaseTree->setBoardCounts(index->countWithPosition(m_session->position()),
                                       index->countWithLine(m_session->initialPosition(), m_session->movesToHere()));
    } else {
        m_databaseTree->setBoardCounts(-1, -1);
    }
    if (m_category.kind == GameCategory::Kind::Position || m_category.kind == GameCategory::Kind::Variant)
        showCategory(m_category);
}

void MainWindow::openGame(const QModelIndex &proxyIndex)
{
    const QModelIndex source = m_gameListProxy->mapToSource(proxyIndex);
    if (!m_database || !source.isValid())
        return;
    if (std::optional<GameRecord> game = m_database->loadGame(source.row())) {
        if (!canLeaveGame())
            return;
        // A game the chapter has already: the board goes there.
        const int inChapter = m_chapters.findGame(game->uid);
        if (inChapter >= 0 && inChapter != m_chapters.chapter().currentGame) {
            switchToChapterGame(inChapter, {}, 0);
            return;
        }
        // Otherwise it joins the chapter, at its end (or in place of an empty
        // game). Without chapters it takes the place of the game on the board,
        // once a game not saved anywhere was saved or let go.
        if (inChapter < 0 && !mayReplaceBoardGame())
            return;
        if (inChapter < 0 && keepsBoardGame())
            m_chapters.breakGame();
        // A game from the database is to be studied, not played: training
        // goes off first, or the engine would answer in it. A puzzle is the
        // exception: it is played, by the side to move, against the engine.
        const bool puzzle = m_database->properties().type == DatabaseType::Training;
        m_trainingModeAction->setChecked(false);
        m_gameView->selectRow(proxyIndex.row());
        m_openGameIndex = source.row();
        if (!puzzle)
            orientBoardForMe(*game);
        m_session->setGame(*game);
        chapterChanged();
        if (puzzle) {
            // The user solves for the side to move at the start, seen from below.
            m_trainingSide = m_session->position().sideToMove();
            m_flipBoardAction->setChecked(m_trainingSide == Side::Black);
            m_startEngineAction->setChecked(true); // The score stays visible throughout.
            m_trainingModeAction->setChecked(true);
            updateTraining();
        }
    }
}

void MainWindow::orientBoardForMe(const GameRecord &game)
{
    if (!m_database)
        return;
    if (const std::optional<Side> side = mySide(game, m_database->playerRoles()))
        m_flipBoardAction->setChecked(*side == Side::Black);
}

void MainWindow::showGameListMenu(const QPoint &position)
{
    const QModelIndex index = m_gameView->indexAt(position);
    if (!m_database || !index.isValid())
        return;
    const QModelIndex source = m_gameListProxy->mapToSource(index);
    const GameRecord header = m_database->header(source.row());
    QString player;
    if (source.column() == GameListModel::White || source.column() == GameListModel::WhiteElo)
        player = header.white;
    else if (source.column() == GameListModel::Black || source.column() == GameListModel::BlackElo)
        player = header.black;

    // A right click outside the selection selects the game clicked alone.
    if (!m_gameView->selectionModel()->isRowSelected(index.row(), QModelIndex()))
        m_gameView->selectRow(index.row());
    const QModelIndexList selected = m_gameView->selectionModel()->selectedRows();

    QMenu menu(this);
    // Who a player is concerns one game's player: not offered for several games.
    if (!player.isEmpty() && selected.size() <= 1) {
        QMenu *who = menu.addMenu(tr("Who Is This?"));
        who->setToolTip(player);
        const PlayerRole current = m_database->playerRoles().value(player);
        auto *roles = new QActionGroup(who);
        const std::pair<PlayerRole, QString> choices[] = {{PlayerRole::Me, tr("It's &Me")},
                                                          {PlayerRole::Friend, tr("A &Friend")},
                                                          {PlayerRole::Opponent, tr("An &Opponent")}};
        for (const auto &[role, text] : choices) {
            QAction *action = who->addAction(text);
            action->setCheckable(true);
            action->setChecked(role == current);
            action->setActionGroup(roles);
            connect(action, &QAction::triggered, this, [this, player, role] { setPlayerRole(player, role); });
        }
        who->addSeparator();
        QAction *forget = who->addAction(tr("&Nobody in Particular"), this,
                                         [this, player] { setPlayerRole(player, PlayerRole::None); });
        forget->setEnabled(current != PlayerRole::None);
        menu.addSeparator();
    }
    // A game is only ever deleted from the trash, and even then it stays in
    // the file until the database is optimized. The actions go to every
    // selected game in the same place as the one clicked.
    QStringList uids;
    for (const QModelIndex &row : selected) {
        const GameRecord game = m_database->header(m_gameListProxy->mapToSource(row).row());
        if (game.state == header.state)
            uids << game.uid;
    }
    if (uids.isEmpty())
        uids << header.uid;
    const int count = int(uids.size());
    if (header.state == GameState::Trashed) {
        menu.addAction(count > 1 ? tr("&Restore %n Games", nullptr, count) : tr("&Restore Game"), this,
                       [this, uids] { setGameState(uids, GameState::Live); });
        menu.addAction(count > 1 ? tr("&Delete %n Games…", nullptr, count) : tr("&Delete Game…"), this,
                       [this, uids] { deleteGames(uids); });
    } else {
        menu.addAction(count > 1 ? tr("Move %n Games to &Trash", nullptr, count) : tr("Move Game to &Trash"), this,
                       [this, uids] { setGameState(uids, GameState::Trashed); });
    }
    menu.exec(m_gameView->viewport()->mapToGlobal(position));
}

void MainWindow::applyGameColumns()
{
    const QStringList hidden = m_database ? m_database->properties().hiddenColumns : QStringList();
    for (int column = 0; column < GameListModel::ColumnCount; ++column)
        m_gameView->setColumnHidden(column, hidden.contains(GameListModel::columnKey(column)));
}

void MainWindow::setGameColumnsHidden(const QStringList &hidden)
{
    if (!m_database)
        return;
    DatabaseProperties properties = m_database->properties();
    properties.hiddenColumns = hidden;
    QString error;
    if (!m_database->setProperties(properties, &error)) {
        QMessageBox::warning(this, tr("Columns"), tr("Could not save the columns in the database: %1").arg(error));
        return;
    }
    applyGameColumns();
}

void MainWindow::showGameColumnsMenu(const QPoint &position)
{
    if (!m_database)
        return;
    const QHeaderView *header = m_gameView->horizontalHeader();
    const int column = header->logicalIndexAt(position);
    const QStringList hidden = m_database->properties().hiddenColumns;

    QMenu menu(this);
    QAction *hide = menu.addAction(column < 0 ? tr("&Hide") : tr("&Hide “%1”").arg(GameListModel::columnName(column)),
                                   this, [this, hidden, column] {
        setGameColumnsHidden(hidden + QStringList{GameListModel::columnKey(column)});
    });
    // The list never loses its last column.
    hide->setEnabled(column >= 0 && header->hiddenSectionCount() < GameListModel::ColumnCount - 1);

    QMenu *show = menu.addMenu(tr("&Show"));
    for (int hiddenColumn = 0; hiddenColumn < GameListModel::ColumnCount; ++hiddenColumn) {
        const QString key = GameListModel::columnKey(hiddenColumn);
        if (!hidden.contains(key))
            continue;
        show->addAction(GameListModel::columnName(hiddenColumn), this, [this, hidden, key] {
            QStringList remaining = hidden;
            remaining.removeAll(key);
            setGameColumnsHidden(remaining);
        });
    }
    if (show->actions().size() > 1) {
        show->addSeparator();
        show->addAction(tr("&All Columns"), this, [this] { setGameColumnsHidden({}); });
    }
    show->setEnabled(!show->isEmpty());
    menu.exec(header->viewport()->mapToGlobal(position));
}

QString MainWindow::moveText(int ply) const
{
    if (ply < 1 || ply > m_session->plyCount())
        return {};
    const MoveRecord &move = m_session->moveAt(ply);
    return m_session->positionAt(ply - 1).lineText({move.uci}) + MoveAnnotation::pgnSuffix(move.nags);
}

void MainWindow::showMoveListMenu(const QPoint &position)
{
    const MoveTreeView::Place place = m_moveView->placeAt(position);
    m_moveView->finishEditing();
    QMenu menu(this);
    // Where a paragraph goes: after the main-line move the place belongs to.
    const auto mainLinePly = [](const GameRecord &game, const QList<int> &path, int ply) {
        return path.isEmpty() ? ply : game.variations.value(path.first()).atPly;
    };
    int game = place.game >= 0 ? place.game : m_chapters.chapter().currentGame;
    int paragraphPly = 0;
    if (place.isBreak()) {
        // The rule between two games: it goes, and with it the game after it
        // when something was entered there.
        syncChapterGame();
        const bool empty = m_chapters.chapter().games.at(place.gameBreak).isEmpty();
        menu.addAction(empty ? tr("&Delete Game Break") : tr("&Delete Following Game"), this,
                       [this, following = place.gameBreak, empty] {
                           deleteChapterGame(following, empty ? tr("Delete Game Break") : tr("Delete Following Game"));
                       })
            ->setEnabled(!m_onlinePlay);
        menu.addSeparator();
    }
    if (place.isMove()) {
        // A move of another game or line is brought on the board first: the menu acts on it.
        if (place.game != m_chapters.chapter().currentGame) {
            if (!canLeaveGame())
                return;
            switchToChapterGame(place.game, place.path, place.ply);
        } else if (place.path != m_session->path()) {
            m_session->goToLine(place.path, place.ply);
        }
        game = m_chapters.chapter().currentGame;
        paragraphPly = mainLinePly(m_session->game(), place.path, place.ply);
    } else if (place.isComment()) {
        // A comment is written in the game on the board: another game comes there first.
        if (place.game != m_chapters.chapter().currentGame) {
            if (!canLeaveGame())
                return;
            switchToChapterGame(place.game, place.path, GameVariations::branchPly(m_chapters.chapter().games.at(place.game).game, place.path));
            game = m_chapters.chapter().currentGame;
        }
        paragraphPly = mainLinePly(m_session->game(), place.path,
                                   GameVariations::branchPly(m_session->game(), place.path) + place.comment);
    } else if (place.game < 0) {
        // Nowhere in particular: where the board is.
        paragraphPly = mainLinePly(m_session->game(), m_session->path(), m_session->ply());
    }
    // Everything that can be inserted, in one menu: text between the moves,
    // and a new game.
    QMenu *insert = menu.addMenu(tr("&Insert"));
    const auto insertText = [this, game, paragraphPly, after = place.paragraph](Paragraph::Kind kind) {
        const int index = m_chapters.insertParagraph(game, paragraphPly, after, kind);
        chapterChanged();
        m_moveView->editParagraph(game, index);
    };
    insert->addAction(tr("&Title"), this, [insertText] { insertText(Paragraph::Kind::Title); })
        ->setToolTip(tr("A heading in bold, centred"));
    insert->addAction(tr("&Subtitle"), this, [insertText] { insertText(Paragraph::Kind::Subtitle); })
        ->setToolTip(tr("A smaller heading in bold, centred"));
    insert->addAction(tr("&Paragraph"), this, [insertText] { insertText(Paragraph::Kind::Text); });
    insert->addSeparator();
    QAction *gameBreak = insert->addAction(tr("&Game Break"), this, [this, game] { insertGameBreak(game); });
    gameBreak->setToolTip(tr("A new game after this one"));
    gameBreak->setEnabled(!m_onlinePlay);
    insert->setToolTipsVisible(true);
    if (place.isParagraph()) {
        // A title, a subtitle or a paragraph, by its name.
        const Paragraph::Kind kind = m_chapters.chapter().games.at(game).paragraphs.at(place.paragraph).kind;
        const bool title = kind == Paragraph::Kind::Title;
        const bool subtitle = kind == Paragraph::Kind::Subtitle;
        menu.addAction(title ? tr("&Edit Title") : subtitle ? tr("&Edit Subtitle") : tr("&Edit Paragraph"), this,
                       [this, game, index = place.paragraph] { m_moveView->editParagraph(game, index); });
        menu.addAction(title ? tr("&Delete Title") : subtitle ? tr("&Delete Subtitle") : tr("&Delete Paragraph"), this,
                       [this, game, index = place.paragraph] {
                           m_chapters.removeParagraph(game, index);
                           chapterChanged();
                       });
    }
    if (place.game >= 0 && !place.isBreak()) {
        // The game the click is in: a whole game, or a line that starts from
        // a later move (a position set up from a game).
        const bool line = chapterGameStartNumber(place.game) != 1;
        if (place.moveNumber || place.start)
            menu.addAction(tr("Change Move &Number…"), this, [this, g = place.game] { changeMoveNumber(g); })
                ->setEnabled(!m_onlinePlay);
        menu.addAction(line ? tr("Delete &Line") : tr("Delete &Game"), this,
                       [this, g = place.game, line] { deleteChapterGame(g, line ? tr("Delete Line") : tr("Delete Game")); })
            ->setEnabled(!m_onlinePlay);
        menu.addSeparator();
    }
    // One Move for what was clicked: a title, a subtitle or a paragraph moves
    // along its game, anything else moves the whole game past the one above
    // or below it (the breaks between games stay where games meet).
    QMenu *move = menu.addMenu(tr("&Move"));
    if (place.isParagraph()) {
        // Along the game, a half-move at a time: after White's move, Down
        // takes it after Black's, which comes back on White's row.
        const int at = m_chapters.chapter().games.at(game).paragraphs.at(place.paragraph).ply;
        const int last = int(m_chapters.chapter().games.at(game).game.moves.size());
        const auto moveTo = [this, game, index = place.paragraph](int ply, bool first) {
            m_chapters.moveParagraph(game, index, ply, first);
            chapterChanged();
        };
        move->addAction(tr("To the &Top"), this, [moveTo] { moveTo(0, true); })->setEnabled(at > 0);
        move->addAction(tr("&Up"), this, [moveTo, at] { moveTo(at - 1, false); })->setEnabled(at > 0);
        move->addAction(tr("&Down"), this, [moveTo, at] { moveTo(at + 1, true); })->setEnabled(at < last);
        move->addAction(tr("To the &Bottom"), this, [moveTo, last] { moveTo(last, false); })->setEnabled(at < last);
    } else {
        const int games = int(m_chapters.chapter().games.size());
        move->setEnabled(games > 1);
        const auto gameTo = [this, game](int to) {
            m_chapters.moveGame(game, to);
            chapterChanged();
        };
        move->addAction(tr("To the &Top"), this, [gameTo] { gameTo(0); })->setEnabled(game > 0);
        move->addAction(tr("&Up"), this, [gameTo, game] { gameTo(game - 1); })->setEnabled(game > 0);
        move->addAction(tr("&Down"), this, [gameTo, game] { gameTo(game + 1); })->setEnabled(game < games - 1);
        move->addAction(tr("To the &Bottom"), this, [gameTo, games] { gameTo(games - 1); })->setEnabled(game < games - 1);
    }
    if (place.isComment()) {
        menu.addSeparator();
        menu.addAction(tr("&Edit Comment"), this, [this, path = place.path, index = place.comment] {
            m_moveView->editComment(path, index);
        });
        menu.addAction(tr("&Delete Comment"), this, [this, path = place.path, index = place.comment] {
            writeComment(path, index, QString());
        });
    }
    if (!place.isMove()) {
        menu.exec(m_moveView->viewport()->mapToGlobal(position));
        return;
    }
    menu.addSeparator();
    const int ply = place.ply;
    // The comment after the move, written under it.
    const int ownMove = ply - GameVariations::branchPly(m_session->game(), place.path);
    const bool commented = !MoveComment::displayText(MoveComment::at(m_session->game(), place.path, ownMove)).isEmpty();
    menu.addAction(commented ? tr("&Edit Comment") : tr("Add &Comment"), this, [this, path = place.path, ownMove] {
        m_moveView->editComment(path, ownMove);
    });

    QMenu *copy = menu.addMenu(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("&Copy"));
    copy->addAction(tr("Copy &Move"), this, [this, ply] { copyText(moveText(ply), tr("Move copied")); });
    copy->addAction(tr("Copy &Line up to Here"), this, [this, ply] {
        copyText(Pgn::moveText(m_session->game(), ply), tr("Line copied"));
    });

    // The lines of the game: a variation promoted or deleted, a line cut short.
    QMenu *lines = menu.addMenu(tr("&Variations"));
    lines->setEnabled(!m_onlinePlay);
    const QList<int> path = place.path;
    if (!path.isEmpty()) {
        lines->addAction(tr("&Promote Variation"), this, [this, path, ply] {
            applyGameEdit(GameVariations::promote(m_session->game(), path, ply), QString());
        });
        lines->addAction(tr("&Delete Variation"), this, [this, path] {
            applyGameEdit(GameVariations::removeVariation(m_session->game(), path),
                          tr("Delete this variation, with the variations inside it?"));
        });
    }
    lines->addAction(tr("Delete from &Here"), this, [this, path, ply] {
        applyGameEdit(GameVariations::truncate(m_session->game(), path, ply),
                      tr("Delete this move and the ones after it on this line, with their variations?"));
    });

    // Every glyph with what it means; choosing the one the move has takes it off.
    QMenu *annotations = menu.addMenu(tr("&Annotations"));
    const QList<int> current = m_session->moveAt(ply).nags;
    MoveAnnotation::Kind kind = MoveAnnotation::Kind::Move;
    for (const MoveAnnotation::Glyph &glyph : MoveAnnotation::glyphs()) {
        if (glyph.kind != kind)
            annotations->addSeparator();
        kind = glyph.kind;
        auto *action = new GlyphMenuAction(glyph.symbol, MoveAnnotation::meaning(glyph.nag), annotations);
        action->setCheckable(true);
        action->setChecked(current.contains(glyph.nag));
        connect(action, &QAction::triggered, this, [this, ply, current, nag = glyph.nag] {
            annotateMove(ply, MoveAnnotation::toggled(current, nag));
        });
        annotations->addAction(action);
    }
    annotations->addSeparator();
    auto *none = new GlyphMenuAction(QString(), tr("No Annotation"), annotations);
    none->setEnabled(!current.isEmpty());
    connect(none, &QAction::triggered, this, [this, ply] { annotateMove(ply, {}); });
    annotations->addAction(none);

    menu.exec(m_moveView->viewport()->mapToGlobal(position));
}

void MainWindow::writeComment(const QList<int> &path, int index, const QString &text)
{
    // What a person reads is replaced; the commands for programs stay.
    const QString before = MoveComment::at(m_session->game(), path, index);
    const QString comment = MoveComment::withText(before, text);
    if (comment == before)
        return;
    m_session->setComment(path, index, comment);
    QString error;
    if (!storeOpenGame(&error)) {
        m_session->setComment(path, index, before);
        QMessageBox::warning(this, tr("Comment"), tr("Could not save the comment: %1").arg(error));
    }
}

void MainWindow::playCommentLine(int game, const QList<int> &path, int basePly, const QStringList &uci)
{
    // The line written in a comment becomes a variation of the game, or the
    // one it already is is taken: from there it is followed as any other.
    if (m_onlinePlay) {
        statusBar()->showMessage(tr("The board stays on the online game until it ends."), 5000);
        return;
    }
    if (game != m_chapters.chapter().currentGame) {
        if (!canLeaveGame())
            return;
        switchToChapterGame(game, path, basePly);
        if (game != m_chapters.chapter().currentGame)
            return;
    } else {
        m_session->goToLine(path, basePly);
    }
    if (m_session->ply() != basePly)
        return;
    // Studied, not played: the engine must not answer in it.
    m_trainingModeAction->setChecked(false);
    bool adds = false;
    for (const QString &move : uci) {
        const std::optional<ChessMove> legal = m_session->position().moveFromUci(move);
        if (!legal)
            break;
        adds = adds || !m_session->isNextMove(*legal);
        m_session->playMove(*legal);
    }
    playMoveSound();
    QString error;
    if (adds && !storeOpenGame(&error))
        statusBar()->showMessage(tr("The move could not be saved in the database: %1").arg(error), 8000);
}

void MainWindow::annotateMove(int ply, const QList<int> &nags)
{
    if (ply < 1 || ply > m_session->plyCount())
        return;
    const QList<int> before = m_session->moveAt(ply).nags;
    m_session->setAnnotations(ply, nags);
    QString error;
    if (!storeOpenGame(&error)) {
        m_session->setAnnotations(ply, before);
        QMessageBox::warning(this, tr("Annotations"), tr("Could not save the annotation: %1").arg(error));
    }
}

void MainWindow::applyGameEdit(const std::optional<GameVariations::Edit> &edit, const QString &question)
{
    if (!edit)
        return;
    // Deleting moves cannot be undone: asked first.
    if (!question.isEmpty()
        && QMessageBox::question(this, tr("Delete Moves"), question, QMessageBox::Yes | QMessageBox::Cancel,
                                 QMessageBox::Cancel)
               != QMessageBox::Yes)
        return;
    const GameRecord before = m_session->game();
    const QList<int> path = m_session->path();
    const int ply = m_session->ply();
    m_session->setGame(edit->game);
    m_session->goToLine(edit->path, edit->ply);
    QString error;
    if (!storeOpenGame(&error)) {
        m_session->setGame(before);
        m_session->goToLine(path, ply);
        QMessageBox::warning(this, tr("Variations"), tr("Could not save the game: %1").arg(error));
    }
}

bool MainWindow::storeOpenGame(QString *error)
{
    if (m_openGameIndex < 0) {
        scheduleSaveSession(); // Not in the database yet: the project keeps it.
        return true;
    }
    // A stored game keeps its moves, variations and annotations in the
    // database at once, as its header does.
    GameRecord game = m_session->game();
    game.modified.clear(); // Changed now.
    if (!m_database || m_openGameIndex >= m_database->gameCount()) {
        *error = tr("The game is no longer in the database.");
        return false;
    }
    if (m_database->header(m_openGameIndex).plyCount > game.moves.size()) {
        *error = tr("Some moves of the stored game cannot be replayed."); // Never cut a stored game short.
        return false;
    }
    if (!m_database->replaceGame(m_openGameIndex, game, error))
        return false;
    m_gameListModel->refreshRow(int(m_openGameIndex));
    m_session->setHeader(m_database->header(m_openGameIndex));
    rebuildPositionIndex(); // The main line may have grown.
    return true;
}

qint64 MainWindow::gameIndexOf(const QString &uid) const
{
    for (qint64 index = 0; m_database && !uid.isEmpty() && index < m_database->gameCount(); ++index) {
        if (m_database->header(index).uid == uid)
            return index;
    }
    return -1;
}

void MainWindow::setGameState(const QStringList &uids, GameState state)
{
    int changed = 0;
    for (const QString &uid : uids) {
        const qint64 index = gameIndexOf(uid);
        if (index < 0)
            continue;
        QString error;
        if (!m_database->setGameState(index, state, &error)) {
            QMessageBox::warning(this, tr("Trash"), tr("Could not move the game: %1").arg(error));
            break;
        }
        ++changed;
    }
    if (changed == 0)
        return;
    showCategory(m_category); // The games leave the list they were in.
    m_databaseTree->refresh();
    m_positionIndexTimer->start(); // Only the games in the lists are searched.
    switch (state) {
    case GameState::Trashed:
        statusBar()->showMessage(tr("%n game(s) moved to the trash", nullptr, changed), 3000);
        break;
    case GameState::Live:
        statusBar()->showMessage(tr("%n game(s) restored", nullptr, changed), 3000);
        break;
    default:
        statusBar()->showMessage(tr("%n game(s) deleted", nullptr, changed), 3000);
        break;
    }
}

void MainWindow::deleteGames(const QStringList &uids)
{
    if (uids.isEmpty())
        return;
    QString what;
    if (uids.size() == 1) {
        const qint64 index = gameIndexOf(uids.first());
        if (index < 0)
            return;
        const GameRecord header = m_database->header(index);
        what = tr("Delete “%1” from the trash?").arg(tr("%1 – %2").arg(header.white, header.black));
    } else {
        what = tr("Delete %n game(s) from the trash?", nullptr, int(uids.size()));
    }
    const auto answer = QMessageBox::question(
        this, uids.size() > 1 ? tr("Delete Games") : tr("Delete Game"),
        what + QStringLiteral("\n\n")
            + tr("They will not be listed anywhere any more. They stay in the file until the database is optimized "
                 "(Database ▸ Database Settings…)."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        setGameState(uids, GameState::Deleted);
}

void MainWindow::optimizeDatabase(QWidget *dialog)
{
    if (!m_database)
        return;
    const QString title = tr("Optimize Database");
    if (m_syncPipeline->isRunning()) {
        QMessageBox::information(dialog, title, tr("A sync is running: optimize the database when it has finished."));
        return;
    }
    const int deleted = int(m_database->countGames(GameState::Deleted));
    if (deleted > 0) {
        const auto answer = QMessageBox::question(
            dialog, title,
            tr("Optimize “%1”?\n\n%n deleted game(s) will be removed for good, here and on every device this "
               "database is synced with.", nullptr, deleted)
                .arg(m_database->name()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QString openUid = m_openGameIndex >= 0 && m_openGameIndex < m_database->gameCount()
        ? m_database->header(m_openGameIndex).uid : QString();
    const bool wasInDatabase = m_openGameIndex >= 0;
    const qint64 sizeBefore = QFileInfo(m_database->location()).size();
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const int removed = m_database->optimize(&error);
    QGuiApplication::restoreOverrideCursor();

    // The games have other indexes now: everything that holds one starts over.
    m_gameListModel->setDatabase(m_database.get());
    if (wasInDatabase)
        m_openGameIndex = gameIndexOf(openUid);
    showCategory(m_category);
    if (m_openGameIndex >= 0) {
        const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(m_openGameIndex), 0));
        if (proxyIndex.isValid())
            m_gameView->selectRow(proxyIndex.row());
    }
    m_databaseTree->refresh();
    rebuildPositionIndex();
    updateGameActions();
    updateGameHeader();

    if (removed < 0) {
        QMessageBox::warning(dialog, title, tr("Could not optimize the database: %1").arg(error));
        return;
    }
    const QLocale locale;
    QMessageBox::information(
        dialog, title,
        tr("%n deleted game(s) removed.", nullptr, removed) + QLatin1Char(' ')
            + tr("The file went from %1 to %2.")
                  .arg(locale.formattedDataSize(sizeBefore),
                       locale.formattedDataSize(QFileInfo(m_database->location()).size())));
}

void MainWindow::setPlayerRole(const QString &player, PlayerRole role)
{
    if (!m_database)
        return;
    QString error;
    if (!m_database->setPlayerRole(player, role, &error)) {
        QMessageBox::warning(this, tr("Who Is This?"), tr("Could not save who %1 is: %2").arg(player, error));
        return;
    }
    m_databaseTree->refresh();
    m_gameListModel->refreshRoles(); // "Me" is shown in bold.
    showCategory(m_category); // A filter on roles now shows other games.
    if (role == PlayerRole::Me)
        orientBoardForMe(m_session->game());
}

void MainWindow::peekAtEngineLine(bool held)
{
    if (!held) {
        m_board->endPeek();
        m_peekArrows.clear();
        return;
    }
    // Only a line about the position on the board: the board clears it when it moves.
    if (m_engineLine.isEmpty() || m_lastEvaluation.pv.isEmpty())
        return;
    ChessPosition position = m_session->position();
    int from = -1;
    int to = -1;
    for (const QString &uci : m_lastEvaluation.pv) {
        const std::optional<ChessMove> move = position.moveFromUci(uci);
        if (!move)
            break;
        position.play(*move);
        from = move->from;
        to = move->to;
    }
    if (from < 0)
        return;
    // The plans the line holds, INSIGHT.smart's: how the pieces got there.
    m_peekArrows = lineInsight(m_session->position(), m_lastEvaluation.pv).arrows;
    m_board->peek(frameFor(position, from, to), m_peekArrows);
}

void MainWindow::syncBoard()
{
    if (m_mergingLobbyMoves)
        return; // Moves arriving in the lobby game: drawn once they are in.
    m_enginePanel->cancelPeek(); // The end of the line belonged to the position before.
    const BoardFrame frame = frameFor(m_session->position(), m_session->lastMoveFrom(), m_session->lastMoveTo());
    if (m_animateNextBoard) {
        m_animateNextBoard = false;
        m_board->setBoardAnimated(frame, kEngineMoveMs);
    } else {
        m_board->setBoard(frame);
    }
    const PieceCounts captured = m_session->position().capturedSince(m_session->initialPosition());
    m_capturedPieces->setCaptured(captured);
    m_boardSideColumn->setCaptured(captured);
    m_boardSideColumn->setSideToMove(m_session->position().sideToMove());
    QMultiHash<int, int> legalMoves;
    // In training the user only moves their own colour; the engine answers by
    // itself. Online the opponent does, and only the live position is played.
    if (!isEngineTurn() && !isOpponentTurn() && !(m_onlinePlay && m_session->ply() != m_session->plyCount())) {
        for (const ChessMove &move : m_session->position().legalMoves())
            legalMoves.insert(move.from, move.to);
    }
    m_board->setLegalMoves(legalMoves);
    m_engineLine.clear();
    // An explanation belongs to one move: moving on turns it off until asked again.
    m_explainAction->setChecked(false);
    updateExplainer();
    updateBoardBorder(); // A checkmate on the board turns it red.
    updateCommentMarks();

    updateNavigationActions();
}

void MainWindow::updateCommentMarks()
{
    // The comment of the position on the board: after the move that led
    // there, or before the first move.
    const int ply = m_session->ply();
    const QString &comment = ply > 0 ? m_session->moveAt(ply).comment : m_session->game().startComment;
    m_board->setMarks(MoveComment::marks(comment));
}

void MainWindow::updateNavigationActions()
{
    const bool atStart = m_session->ply() == 0;
    const bool atEnd = m_session->ply() == m_session->plyCount();
    m_firstMoveAction->setEnabled(!atStart);
    m_previousMoveAction->setEnabled(!atStart);
    m_nextMoveAction->setEnabled(!atEnd);
    m_lastMoveAction->setEnabled(!atEnd);
}

void MainWindow::updateGameCount()
{
    if (!m_database) {
        m_gameCountLabel->setText(tr("No database"));
        return;
    }
    // The trash is counted apart: its games are in no other list.
    const bool trash = m_gameListProxy->state() == GameState::Trashed;
    const qint64 total = m_database->countGames(m_gameListProxy->state());
    const qint64 shown = m_gameListProxy->rowCount();
    const QLocale locale;
    QString count = m_gameListProxy->isFiltered()
        ? tr("%1 of %2 games").arg(locale.toString(shown), locale.toString(total))
        : total == 1 ? tr("1 game") : tr("%1 games").arg(locale.toString(total));
    if (trash)
        count = tr("Trash: %1").arg(count);
    m_gameCountLabel->setText(tr("%1 — %2").arg(m_database->name(), count));
}

void MainWindow::showCategory(const GameCategory &category)
{
    using Kind = GameCategory::Kind;
    m_category = category;
    m_filterSource = category.kind == Kind::Source ? category.sourceId : 0;
    GameFilterProxyModel::Predicate predicate;
    GameState state = GameState::Live;
    const QString value = category.value;
    switch (category.kind) {
    case Kind::All:
        break;
    case Kind::Trash:
        state = GameState::Trashed;
        break;
    case Kind::TrashRecent:
    case Kind::TrashOld:
        state = GameState::Trashed;
        predicate = [recent = category.kind == Kind::TrashRecent,
                     now = QDateTime::currentDateTimeUtc()](const GameRecord &game) {
            return GameStates::isRecent(game.stateModified, now) == recent;
        };
        break;
    case Kind::Position:
    case Kind::Variant: {
        // No games until the index is ready; updateBoardFilters() comes back then.
        QSet<qint64> ids;
        if (const PositionIndex *index = m_positionIndex->index()) {
            ids = category.kind == Kind::Position
                ? index->gamesWithPosition(m_session->position())
                : index->gamesWithLine(m_session->initialPosition(), m_session->movesToHere());
        }
        predicate = [ids](const GameRecord &game) { return ids.contains(game.id); };
        break;
    }
    case Kind::Role:
        if (m_database) {
            predicate = [roles = m_database->playerRoles(), role = playerRoleFromKey(value)](const GameRecord &game) {
                return DatabaseOutline::hasRole(game, roles, role);
            };
        }
        break;
    case Kind::Player:
        predicate = [value](const GameRecord &game) { return game.white == value || game.black == value; };
        break;
    case Kind::EcoLetter:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::ecoCode(game.eco).startsWith(value); };
        break;
    case Kind::Eco:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::ecoCode(game.eco) == value; };
        break;
    case Kind::Event:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::eventName(game.event) == value; };
        break;
    case Kind::Year:
        predicate = [year = value.toInt()](const GameRecord &game) { return DatabaseOutline::year(game.date) == year; };
        break;
    case Kind::TimeControl:
        predicate = [value](const GameRecord &game) { return TimeControl::of(game) == value; };
        break;
    case Kind::Study:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::studyKey(game) == value; };
        break;
    case Kind::StudyChapter:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::chapterKey(game) == value; };
        break;
    case Kind::EndgameFamily:
        predicate = [value](const GameRecord &game) {
            const QString endgame = TrainingSets::endgameOf(game);
            return !endgame.isEmpty() && TrainingSets::endgameFamily(endgame) == value;
        };
        break;
    case Kind::Endgame:
        predicate = [value](const GameRecord &game) { return TrainingSets::endgameOf(game) == value; };
        break;
    case Kind::Tactic:
        predicate = [value](const GameRecord &game) { return TrainingSets::tacticsOf(game).contains(value); };
        break;
    case Kind::Source:
        if (m_database) {
            predicate = [ids = m_database->sourceGameIds(category.sourceId)](const GameRecord &game) {
                return ids.contains(game.id);
            };
        }
        break;
    }
    m_gameListProxy->setPredicate(predicate, state);
    updateGameCount();
}

void MainWindow::newDatabase()
{
    if (!UserFolders::ensureDatabasesDir()) {
        QMessageBox::warning(this, tr("New Database"),
                             tr("Could not create the folder “%1”.").arg(UserFolders::databasesDir()));
        return;
    }

    QString name = tr("New Database");
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("New Database"), tr("Database name:"),
                                     QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.startsWith(QLatin1Char('.'))) {
            QMessageBox::warning(this, tr("New Database"), tr("“%1” is not a valid name.").arg(name));
            continue;
        }
        const QString path = QDir(UserFolders::databasesDir())
                                 .filePath(name + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
        if (QFileInfo::exists(path)) {
            QMessageBox::warning(this, tr("New Database"),
                                 tr("A database named “%1” already exists.").arg(name));
            continue;
        }

        QString error;
        std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, {}, &error);
        if (!database) {
            QMessageBox::warning(this, tr("New Database"),
                                 tr("Could not create the database: %1").arg(error));
            return;
        }
        setDatabase(std::move(database));
        if (!keepOnlineGame()) {
            relinkChapterGame(); // The chapter goes on; its games are not in the new database.
            statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(path)), 5000);
        }
        return;
    }
}

void MainWindow::openDatabase()
{
    UserFolders::ensureDatabasesDir();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Database"), UserFolders::databasesDir(),
        tr("Pragma Chess databases (*.pdb);;All files (*)"));
    if (!path.isEmpty())
        openDatabaseFile(path);
}

bool MainWindow::keepOnlineGame()
{
    // A game being played online stays on the board whatever database is
    // open: it is saved, when it ends, to the one open then.
    if (!m_onlinePlay)
        return false;
    statusBar()->showMessage(tr("The online game goes on; when it ends it is saved to “%1”.")
                                 .arg(m_database ? m_database->name() : QString()),
                             8000);
    return true;
}

bool MainWindow::openDatabaseFile(const QString &path)
{
    if (m_database && m_database->location() == QFileInfo(path).absoluteFilePath())
        return true;

    QString error;
    std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    if (!database) {
        QMessageBox::warning(this, tr("Open Database"),
                             tr("Could not open the database: %1").arg(error));
        return false;
    }
    setDatabase(std::move(database));
    if (keepOnlineGame())
        return true;
    // The chapter goes on: its first game is opened only into an empty one.
    if (m_chapters.game().isEmpty() && m_gameListProxy->rowCount() > 0)
        openGame(m_gameListProxy->index(0, 0));
    else
        relinkChapterGame();
    return true;
}

namespace {

/// A database we ship is named in every language we have (DatabaseProperties'
/// name and name.<code>), not by its file: the file keeps one name on every
/// device, the interface shows the user's language. A name already given stays.
void nameShippedDatabase(GameDatabase &database, const QString &english, const QString &italian)
{
    DatabaseProperties properties = database.properties();
    if (properties.isDistributed())
        return;
    properties.localizedNames.insert(QStringLiteral("en"), english);
    properties.localizedNames.insert(QStringLiteral("it"), italian);
    database.setProperties(properties, nullptr);
}

} // namespace

void MainWindow::openInitialDatabase(const QString &preferredPath)
{
    if (m_database && !preferredPath.isEmpty() && m_database->location() == preferredPath)
        return;

    QString error;
    if (!preferredPath.isEmpty() && QFileInfo::exists(preferredPath)) {
        if (auto database = SqliteGameDatabase::open(preferredPath, &error)) {
            setDatabase(std::move(database));
            return;
        }
    }

    if (!UserFolders::ensureDatabasesDir()) {
        statusBar()->showMessage(tr("Could not create the folder %1")
                                     .arg(QDir::toNativeSeparators(UserFolders::databasesDir())));
        setDatabase(nullptr);
        return;
    }

    const QDir folder(UserFolders::databasesDir());
    const QStringList existing = folder.entryList(
        {QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files, QDir::Name);
    for (const QString &file : existing) {
        if (auto database = SqliteGameDatabase::open(folder.filePath(file), &error)) {
            setDatabase(std::move(database));
            return;
        }
    }

    // First run: seed the user's databases folder with a few classic games.
    const QString seed = folder.filePath(tr("Classic Games") + QLatin1Char('.')
                                         + QLatin1String(UserFolders::databaseSuffix));
    if (auto database = SqliteGameDatabase::create(seed, classicGames(), &error)) {
        // Every install's Classic Games is the same database (see GameIdentity).
        DatabaseProperties properties = database->properties();
        properties.id = GameIdentity::kClassicGamesLineage;
        database->setProperties(properties, nullptr);
        nameShippedDatabase(*database, QStringLiteral("Classic Games"), QStringLiteral("Partite classiche"));
        setDatabase(std::move(database));
        return;
    }
    statusBar()->showMessage(tr("Could not create %1: %2").arg(QDir::toNativeSeparators(seed), error));
    setDatabase(nullptr);
}

void MainWindow::saveDatabase()
{
    if (!m_database)
        return;
    if (m_database->location().isEmpty()) {
        saveDatabaseAs();
        return;
    }
    // Changes are written to the .pdb file as they happen; nothing pending.
    statusBar()->showMessage(tr("%1 is up to date").arg(m_database->name()), 3000);
}

void MainWindow::saveDatabaseAs()
{
    if (!m_database)
        return;
    UserFolders::ensureDatabasesDir();
    const QString suffix = QLatin1String(UserFolders::databaseSuffix);
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Database As"),
        QDir(UserFolders::databasesDir()).filePath(m_database->name() + QLatin1Char('.') + suffix),
        tr("Pragma Chess databases (*.pdb)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + suffix;

    QString error;
    if (!m_database->saveCopy(path, &error)) {
        QMessageBox::warning(this, tr("Save Database As"),
                             tr("Could not save the database: %1").arg(error));
        return;
    }

    // Continue working on the new file, at the same game and move.
    const qint64 gameIndex = m_openGameIndex;
    const GameRecord game = m_session->game();
    const int ply = m_session->ply();
    QString openError;
    std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &openError);
    if (!database) {
        QMessageBox::warning(this, tr("Save Database As"),
                             tr("The database was saved but could not be opened: %1").arg(openError));
        return;
    }
    // A copy saved as another file is a new database: two files with one
    // universal id on the same computer would be one database to a sync.
    DatabaseProperties properties = database->properties();
    properties.id = GameIdentity::newLineageId();
    database->setProperties(properties, nullptr);
    setDatabase(std::move(database));
    if (gameIndex >= 0 && gameIndex < m_database->gameCount()) {
        const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(gameIndex), 0));
        m_openGameIndex = gameIndex;
        if (proxyIndex.isValid())
            m_gameView->selectRow(proxyIndex.row());
    }
    m_session->setGame(game);
    m_session->goToPly(ply);
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)), 5000);
}

void MainWindow::rebuildDatabasesMenu()
{
    m_databasesMenu->clear();

    const QDir folder(UserFolders::databasesDir());
    const QFileInfoList files = folder.entryInfoList(
        {QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        // By the name given in Database Settings, with the file in brackets.
        const QString label = SqliteGameDatabase::readProperties(file.absoluteFilePath())
                                  .label(UiLanguage::effective(), file.absoluteFilePath());
        QAction *action = m_databasesMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_database && m_database->location() == file.absoluteFilePath());
        const QString path = file.absoluteFilePath();
        connect(action, &QAction::triggered, this, [this, path] { openDatabaseFile(path); });
    }
    if (files.isEmpty())
        m_databasesMenu->addAction(tr("No databases"))->setEnabled(false);

    m_databasesMenu->addSeparator();
    m_databasesMenu->addAction(m_showDatabasesFolderAction);
}

void MainWindow::restoreBook()
{
    QSettings settings;
    const QString key = QStringLiteral("book/path");
    if (!settings.contains(key)) {
        // First launch: seed the Books folder with the book built from named openings.
        const QString seed = QDir(UserFolders::booksDir())
                                 .filePath(QStringLiteral("Pragma Openings.") + QLatin1String(UserFolders::bookSuffix));
        if (!QFile::exists(seed) && UserFolders::ensureBooksDir()) {
            QFile::copy(QStringLiteral(":/books/pragma-openings.bin"), seed);
            QFile::setPermissions(seed, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        }
        chooseBook(QFile::exists(seed) ? seed : QString());
        return;
    }
    const QString path = settings.value(key).toString();
    if (path.isEmpty() || !QFile::exists(path)) {
        chooseBook(QString());
        return;
    }
    chooseBook(path);
}

void MainWindow::chooseBook(const QString &path)
{
    if (path.isEmpty()) {
        m_book.reset();
    } else {
        auto book = std::make_unique<PolyglotBook>();
        QString error;
        if (!book->open(path, &error)) {
            QMessageBox::warning(this, tr("Open Book"), tr("Could not open the book %1: %2")
                                                            .arg(QDir::toNativeSeparators(path), error));
            return;
        }
        m_book = std::move(book);
    }
    QSettings().setValue(QStringLiteral("book/path"), path);
    const QString bookName = path.isEmpty() ? QString() : QFileInfo(path).completeBaseName();
    m_bookPanel->setBookName(bookName);
    m_enginePanel->setBookName(bookName);
    updateResourceButtons();
    updateBookMoves();
}

void MainWindow::openBookFile()
{
    UserFolders::ensureBooksDir();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Book"), UserFolders::booksDir(),
                                                      tr("Polyglot opening books (*.bin);;All files (*)"));
    if (path.isEmpty())
        return;
    chooseBook(path);
    if (m_book)
        m_openingTreeDock->show();
}

void MainWindow::newBook()
{
    if (!UserFolders::ensureBooksDir()) {
        QMessageBox::warning(this, tr("New Book"), tr("Could not create the folder “%1”.").arg(UserFolders::booksDir()));
        return;
    }
    QString name = tr("New Book");
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("New Book"), tr("Book name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.startsWith(QLatin1Char('.'))) {
            QMessageBox::warning(this, tr("New Book"), tr("“%1” is not a valid name.").arg(name));
            continue;
        }
        const QString path = QDir(UserFolders::booksDir())
                                 .filePath(name + QLatin1Char('.') + QLatin1String(UserFolders::bookSuffix));
        if (QFileInfo::exists(path)) {
            QMessageBox::warning(this, tr("New Book"), tr("A book named “%1” already exists.").arg(name));
            continue;
        }
        // A clone of the book we ship: a starting point the user then makes their own.
        if (!QFile::copy(QStringLiteral(":/books/pragma-openings.bin"), path)) {
            QMessageBox::warning(this, tr("New Book"), tr("Could not create the book “%1”.").arg(name));
            return;
        }
        // A file copied out of a resource is read-only.
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        chooseBook(path);
        if (m_book)
            m_openingTreeDock->show();
        statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(path)), 5000);
        return;
    }
}

void MainWindow::rebuildBookMenu()
{
    m_switchBookMenu->clear();
    fillBookChoices(m_switchBookMenu);
    m_switchBookMenu->addSeparator();
    m_switchBookMenu->addAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon), tr("Show Books &Folder"), this, [] {
        UserFolders::ensureBooksDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::booksDir()));
    });
}

void MainWindow::fillBookChoices(QMenu *menu)
{
    const QString current = m_book ? QFileInfo(m_book->path()).absoluteFilePath() : QString();
    auto *group = new QActionGroup(menu);

    const QDir folder(UserFolders::booksDir());
    QFileInfoList files = folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::bookSuffix)},
                                               QDir::Files, QDir::Name);
    // A book chosen outside the folder is listed too.
    if (!current.isEmpty() && std::none_of(files.cbegin(), files.cend(), [&](const QFileInfo &file) {
            return file.absoluteFilePath() == current;
        }))
        files.prepend(QFileInfo(current));
    for (const QFileInfo &file : std::as_const(files)) {
        QAction *action = menu->addAction(file.completeBaseName());
        action->setCheckable(true);
        action->setActionGroup(group);
        action->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
        action->setChecked(file.absoluteFilePath() == current);
        const QString path = file.absoluteFilePath();
        connect(action, &QAction::triggered, this, [this, path] {
            chooseBook(path);
            if (m_book)
                m_openingTreeDock->show();
        });
    }
    QAction *none = menu->addAction(tr("&No Book"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(current.isEmpty());
    connect(none, &QAction::triggered, this, [this] { chooseBook(QString()); });
}

void MainWindow::updateBookMoves()
{
    reloadOpeningNamesIfChanged();
    const ChessPosition &position = m_session->position();
    const QList<PolyglotBook::Move> moves = m_book ? m_book->moves(position) : QList<PolyglotBook::Move>();
    QList<OpeningNames::Name> names;
    OpeningNames::Name opening;
    if (m_openingNames) {
        for (const PolyglotBook::Move &move : moves) {
            ChessPosition next = position;
            next.play(move.move);
            names << m_openingNames->name(next);
        }
        // The game is still in the last opening it passed through.
        for (int ply = m_session->ply(); ply >= 0 && opening.isEmpty(); --ply)
            opening = m_openingNames->name(m_session->positionAt(ply));
    }
    m_enginePanel->setOpening(opening);
    QString lastMove;
    if (const std::optional<ChessMove> move = m_session->lastMove()) {
        const ChessPosition &before = m_session->positionAt(m_session->ply() - 1);
        lastMove = before.moveNumberText() + figurineSan(before.san(*move));
    }
    m_bookPanel->setMoves(position, moves, names, lastMove);
    updateBookDatabaseStats();
}

void MainWindow::updateBookDatabaseStats()
{
    const PositionIndex *index = m_positionIndex->index();
    if (!m_database) {
        m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::NoDatabase, {});
        return;
    }
    if (!index) {
        m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::Indexing, {});
        return;
    }
    const ChessPosition &position = m_session->position();
    QList<PositionIndex::Stats> stats;
    for (const PolyglotBook::Move &move : m_book ? m_book->moves(position) : QList<PolyglotBook::Move>()) {
        ChessPosition next = position;
        next.play(move.move);
        stats << index->statsWithPosition(next);
    }
    m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::Ready, stats);
}

void MainWindow::restoreOpeningNames()
{
    // The shipped names, one per language, ready made so that the first launch
    // is not spent building them; the TSVs they were built from stay as the fallback.
    if (UserFolders::ensureOpeningNamesDir()) {
        const QDir folder(UserFolders::openingNamesDir());
        for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
            const QString seed = folder.filePath(names.fileName);
            if (QFile::exists(seed))
                continue;
            if (QFile::copy(names.resource, seed)) {
                // A file copied out of a resource is read-only, and this one is a
                // database the user may edit.
                QFile::setPermissions(seed, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
                SqliteGameDatabase::adoptLineage(seed, names.lineage);
                continue;
            }
            QFile tsv(names.tsv);
            QString error;
            std::unique_ptr<SqliteGameDatabase> built;
            if (tsv.open(QIODevice::ReadOnly))
                built = SqliteGameDatabase::create(seed, OpeningNames::gamesFromTsv(QString::fromUtf8(tsv.readAll())), &error);
            if (built) {
                DatabaseProperties properties = built->properties();
                properties.id = names.lineage;
                properties.type = DatabaseType::OpeningBook;
                names.applyNames(properties);
                built->setProperties(properties, nullptr);
            } else {
                statusBar()->showMessage(tr("Could not create %1: %2").arg(QDir::toNativeSeparators(seed), error));
            }
        }
    }

    QSettings settings;
    if (settings.value(QStringLiteral("book/openingNamesExplicit")).toBool()) {
        const QString path = settings.value(QStringLiteral("book/openingNames")).toString();
        chooseOpeningNames(path.isEmpty() || (QFile::exists(path) && markAsOpeningBook(path)) ? path : QString());
        return;
    }
    // Not chosen by the user: the names of the interface language.
    const ShippedOpeningNames::Names names = ShippedOpeningNames::forLanguage(UiLanguage::effective());
    const QString path = QDir(UserFolders::openingNamesDir()).filePath(names.fileName);
    chooseOpeningNames(QFile::exists(path) && markAsOpeningBook(path) ? path : QString());
}

void MainWindow::migrateOpeningNames()
{
    QSettings settings;
    const auto follow = [this, &settings](const QString &from, const QString &to) {
        m_movedDatabases.insert(from, to);
        if (settings.value(QStringLiteral("book/openingNames")).toString() == from)
            settings.setValue(QStringLiteral("book/openingNames"), to);
    };
    // Moves a shipped names database to its place in the Opening Names folder
    // (English.pdb, Italian.pdb). When that place already holds every game of
    // this copy, none older, the copy adds nothing and is removed instead of
    // piling up; any other copy is kept under Old/. Nothing is ever
    // overwritten. Either way the folder sync is told the old path went, so
    // it goes from the server and the other devices instead of being asked
    // about as a file deleted by hand.
    const auto relocate = [&](const QString &source, const ShippedOpeningNames::Names &shipped) {
        const QString place = QDir(UserFolders::openingNamesDir()).filePath(shipped.fileName);
        if (QFile::exists(place) && SqliteGameDatabase::readProperties(place).id == shipped.lineage
            && ShippedOpeningNames::addsNothing(SqliteGameDatabase::readRevisions(place),
                                                SqliteGameDatabase::readRevisions(source))) {
            if (QFile::remove(source)) {
                m_folderSync->noteRemoved(source); // The other devices drop their copy too.
                follow(source, place);
            }
            return;
        }
        const QString target = ShippedOpeningNames::moveTarget(UserFolders::openingNamesDir(), shipped,
                                                               [](const QString &path) { return QFile::exists(path); });
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::rename(source, target)) {
            statusBar()->showMessage(tr("Could not move %1 to %2").arg(QDir::toNativeSeparators(source),
                                                                          QDir::toNativeSeparators(target)), 8000);
            return;
        }
        m_folderSync->noteRemoved(source); // Moved: the old path goes on every device.
        SqliteGameDatabase::adoptLineage(target, shipped.lineage);
        markAsOpeningBook(target);
        follow(source, target);
    };

    if (!UserFolders::ensureOpeningNamesDir())
        return;
    // Shipped names seeded among the databases of games by older versions.
    const QString pattern = QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix);
    const QFileInfoList databases = QDir(UserFolders::databasesDir()).entryInfoList({pattern}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : databases) {
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
        if (!ShippedOpeningNames::shouldMove(file.fileName(), properties))
            continue;
        const ShippedOpeningNames::Names *names = ShippedOpeningNames::byLineage(properties.id);
        relocate(file.absoluteFilePath(), names ? *names : ShippedOpeningNames::all().first());
    }
    // Shipped names in the folder under the file names older versions gave them.
    const QFileInfoList kept = QDir(UserFolders::openingNamesDir()).entryInfoList({pattern}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : kept) {
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
        if (!ShippedOpeningNames::renamedFileName(file.fileName(), properties).isEmpty())
            relocate(file.absoluteFilePath(), *ShippedOpeningNames::byLineage(properties.id));
    }
    const QDir folder(UserFolders::openingNamesDir());
    for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
        const QString place = folder.filePath(names.fileName);
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(place);
        if (properties.id != names.lineage)
            continue;
        // Shipped names from before they had a display name get theirs.
        QString error;
        if (!properties.isDistributed()) {
            if (const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(place, &error)) {
                DatabaseProperties updated = database->properties();
                names.applyNames(updated);
                database->setProperties(updated, &error);
            }
        }
        // A project (or the names setting) on another copy that adds nothing to
        // the shipped one opens the shipped one; the copy itself is left alone.
        const QHash<QString, QString> revisions = SqliteGameDatabase::readRevisions(place);
        const QFileInfoList copies = folder.entryInfoList({pattern}, QDir::Files, QDir::Name);
        for (const QFileInfo &copy : copies) {
            const QString path = copy.absoluteFilePath();
            if (path != QFileInfo(place).absoluteFilePath()
                && SqliteGameDatabase::readProperties(path).id == names.lineage
                && ShippedOpeningNames::addsNothing(revisions, SqliteGameDatabase::readRevisions(path)))
                follow(path, place);
        }
    }
}

void MainWindow::rebuildOpeningNamesMenu()
{
    m_openingNamesMenu->clear();
    auto *group = new QActionGroup(m_openingNamesMenu);
    const QString current = m_openingNamesPath.isEmpty() ? QString() : QFileInfo(m_openingNamesPath).absoluteFilePath();
    const QString pattern = QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix);
    // The database's own name in the interface language, not its file name;
    // copies that would read the same say which file they are.
    struct Entry {
        QFileInfo file;
        QString name;
    };
    const auto opening = [&pattern](const QString &folder) {
        QList<Entry> entries;
        for (const QFileInfo &file : QDir(folder).entryInfoList({pattern}, QDir::Files, QDir::Name)) {
            const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
            if (properties.type == DatabaseType::OpeningBook)
                entries.append({file, properties.displayName(UiLanguage::effective(), file.completeBaseName())});
        }
        return entries;
    };
    const QList<Entry> kept = opening(UserFolders::openingNamesDir());
    const QList<Entry> databases = opening(UserFolders::databasesDir());
    QHash<QString, int> seen;
    for (const QList<Entry> *list : {&kept, &databases}) {
        for (const Entry &entry : *list)
            ++seen[entry.name];
    }
    const auto addEntries = [&](const QList<Entry> &entries) {
        for (const Entry &entry : entries) {
            const QFileInfo &file = entry.file;
            const QString label = seen.value(entry.name) > 1 && entry.name != file.completeBaseName()
                ? tr("%1 (%2)").arg(entry.name, file.completeBaseName())
                : entry.name;
            QAction *action = m_openingNamesMenu->addAction(label);
            action->setCheckable(true);
            action->setActionGroup(group);
            action->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
            const QString path = file.absoluteFilePath();
            action->setChecked(path == current);
            connect(action, &QAction::triggered, this, [this, path] { chooseOpeningNames(path, true); });
        }
        if (!entries.isEmpty())
            m_openingNamesMenu->addSeparator();
    };
    // The shipped names and any others kept with them, then opening books among the databases.
    addEntries(kept);
    addEntries(databases);
    QAction *none = m_openingNamesMenu->addAction(tr("No Names"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(current.isEmpty());
    connect(none, &QAction::triggered, this, [this] { chooseOpeningNames(QString(), true); });
    m_openingNamesMenu->addSeparator();
    m_openingNamesMenu->addAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon), tr("Show Opening Names &Folder"),
                                  this, [] {
                                      UserFolders::ensureOpeningNamesDir();
                                      QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::openingNamesDir()));
                                  });
}


void MainWindow::seedDistributedProjects()
{
    // Examples of what a project is — chapters, games, paragraphs, in every
    // language they are written in —, to open from File ▸ Open Project.
    QSettings settings;
    const QDir resources(QStringLiteral(":/projects"));
    for (const QString &file : resources.entryList({QStringLiteral("*.pch")}, QDir::Files)) {
        const QString key = QStringLiteral("distributed/project/%1/seeded").arg(QFileInfo(file).completeBaseName());
        if (settings.value(key, false).toBool() || !UserFolders::ensureProjectsDir())
            continue;
        const QString path = QDir(UserFolders::projectsDir()).filePath(file);
        if (!QFileInfo::exists(path)) {
            if (!QFile::copy(resources.filePath(file), path))
                continue;
            // A file copied out of the resources is read-only.
            QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
        }
        settings.setValue(key, true);
    }
}

void MainWindow::updateDistributedDatabases()
{
    // The databases we distribute are known by their lineage, whatever their
    // file is called on this device: a new version adds the games it brings
    // to the copy the user has (by uid, or by start position for the training
    // sets), never touching what the user changed, and never bringing back a
    // game the user threw away. The training sets are created when missing,
    // once: one the user deleted is not brought back. Classic Games is
    // created on the first run (openInitialDatabase).
    struct Distributed {
        const char *key;
        const char *file;
        QString lineage;
        const char *english;
        const char *italian;
        const char *description;
        DatabaseType type;
        bool createWhenMissing;
        std::function<QList<GameRecord>()> games;
    };
    const auto puzzles = [](const char *resource) {
        QFile file{QString::fromLatin1(resource)};
        return file.open(QIODevice::ReadOnly) ? TrainingSets::puzzleGames(QString::fromUtf8(file.readAll()))
                                              : QList<GameRecord>();
    };
    const std::vector<Distributed> distributed{
        {"classic", "Classic Games", GameIdentity::kClassicGamesLineage, "Classic Games", "Partite classiche",
         "Famous games of chess history.", DatabaseType::GameCollection, false, [] { return classicGames(); }},
        {"endgames", "Endgames", GameIdentity::kEndgamesLineage, "Endgame Training", "Finali per l'allenamento",
         "Theoretical endgames and endgame puzzles from the lichess.org puzzle database (CC0).",
         DatabaseType::Training, true,
         [puzzles] { return TrainingSets::theoryEndgames() + puzzles(":/training/endgames.tsv"); }},
        {"tactics", "Tactics", GameIdentity::kTacticsLineage, "Tactics Training", "Tattica per l'allenamento",
         "Tactical puzzles by theme from the lichess.org puzzle database (CC0).", DatabaseType::Training, true,
         [puzzles] { return puzzles(":/training/tactics.tsv"); }},
    };
    const QDir folder(UserFolders::databasesDir());
    QHash<QString, QString> byLineage;
    for (const QString &file : folder.entryList({QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files)) {
        const QString id = SqliteGameDatabase::readProperties(folder.filePath(file)).id;
        if (!id.isEmpty() && !byLineage.contains(id))
            byLineage.insert(id, folder.filePath(file));
    }
    QSettings settings;
    for (const Distributed &set : distributed) {
        const QString seededKey = QStringLiteral("distributed/%1/seeded").arg(QLatin1String(set.key));
        const QString contentKey = QStringLiteral("distributed/%1/content").arg(QLatin1String(set.key));
        const QString typedKey = QStringLiteral("distributed/%1/typed").arg(QLatin1String(set.key));
        const QList<GameRecord> games = set.games();
        // What this version distributes, to skip the work when nothing changed.
        QCryptographicHash hash(QCryptographicHash::Sha1);
        for (const GameRecord &game : games)
            hash.addData((game.uid.isEmpty() ? GameIdentity::uid(game) : game.uid).toUtf8());
        const QString content = QString::fromLatin1(hash.result().toHex());

        // Seeded under the earlier key: still seeded.
        if (settings.value(QStringLiteral("training/seeded/") + QLatin1String(set.key)).toBool())
            settings.setValue(seededKey, true);
        QString path = byLineage.value(set.lineage);
        if (path.isEmpty()) {
            if (!set.createWhenMissing || settings.value(seededKey, false).toBool() || !UserFolders::ensureDatabasesDir())
                continue;
            path = folder.filePath(QLatin1String(set.file) + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
            if (QFileInfo::exists(path))
                continue; // A file of the user's under that name: left alone.
            QString error;
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, games, &error);
            if (!database) {
                qWarning("Could not create %s: %s", qPrintable(path), qPrintable(error));
                continue;
            }
            DatabaseProperties properties = database->properties();
            properties.id = set.lineage;
            properties.description = QLatin1String(set.description);
            properties.type = set.type;
            database->setProperties(properties, nullptr);
            nameShippedDatabase(*database, QLatin1String(set.english), QString::fromUtf8(set.italian));
            settings.setValue(seededKey, true);
            settings.setValue(typedKey, true);
            settings.setValue(contentKey, content);
            continue;
        }
        settings.setValue(seededKey, true);
        // Its type was given after it was first distributed (the training
        // sets became Puzzles and Training): given once, so a type the user
        // chose afterwards in Database Settings stays.
        const bool retype = set.type != DatabaseType::GameCollection && !settings.value(typedKey, false).toBool();
        if (!retype && settings.value(contentKey).toString() == content)
            continue;
        // The open database is updated through itself, any other opened here.
        QString error;
        std::unique_ptr<SqliteGameDatabase> opened;
        GameDatabase *database = nullptr;
        if (m_database && QFileInfo(m_database->location()) == QFileInfo(path)) {
            database = m_database.get();
        } else {
            opened = SqliteGameDatabase::open(path, &error);
            database = opened.get();
        }
        if (!database)
            continue;
        if (retype) {
            DatabaseProperties properties = database->properties();
            properties.type = set.type;
            if (database->setProperties(properties, &error))
                settings.setValue(typedKey, true);
            if (database == m_database.get())
                m_gameListModel->refreshRoles();
            if (settings.value(contentKey).toString() == content)
                continue;
        }
        QSet<QString> present;
        QSet<QString> positions;
        for (qint64 i = 0; i < database->gameCount(); ++i) {
            const GameRecord header = database->header(i);
            present.insert(header.uid);
            if (!header.startFen.isEmpty())
                positions.insert(header.startFen);
        }
        for (const GameStateRecord &state : database->gameStates())
            present.insert(state.uid); // Thrown away, even for good: not brought back.
        int added = 0;
        for (const GameRecord &game : games) {
            const QString uid = game.uid.isEmpty() ? GameIdentity::uid(game) : game.uid;
            if (present.contains(uid) || (!game.startFen.isEmpty() && positions.contains(game.startFen)))
                continue;
            if (database->addGame(game, &error) >= 0)
                ++added;
        }
        nameShippedDatabase(*database, QLatin1String(set.english), QString::fromUtf8(set.italian));
        settings.setValue(contentKey, content);
        if (added > 0 && database == m_database.get()) {
            m_gameListModel->setDatabase(m_database.get());
            updateGameCount();
            m_databaseTree->scheduleRefresh();
            rebuildPositionIndex();
        }
    }
}

void MainWindow::adoptShippedLineages()
{
    // The databases we ship have fixed universal ids, so every install and
    // update of them is the same database when devices sync. Copies seeded
    // before ids existed get theirs here (a file that already has one keeps it).
    // The shipped opening names get theirs in migrateOpeningNames() and when seeded.
    const QString path = QDir(UserFolders::databasesDir())
                             .filePath(tr("Classic Games") + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
    if (QFile::exists(path)) {
        SqliteGameDatabase::adoptLineage(path, GameIdentity::kClassicGamesLineage);
        // Named in every language, like every database we ship.
        if (!SqliteGameDatabase::readProperties(path).isDistributed()) {
            QString error;
            if (m_database && QFileInfo(m_database->location()) == QFileInfo(path))
                nameShippedDatabase(*m_database, QStringLiteral("Classic Games"), QStringLiteral("Partite classiche"));
            else if (const std::unique_ptr<SqliteGameDatabase> classic = SqliteGameDatabase::open(path, &error))
                nameShippedDatabase(*classic, QStringLiteral("Classic Games"), QStringLiteral("Partite classiche"));
        }
    }
}

bool MainWindow::markAsOpeningBook(const QString &path)
{
    // Databases chosen for names before they had a type (and the seed, built
    // from the shipped file) are opening books.
    if (SqliteGameDatabase::readProperties(path).type == DatabaseType::OpeningBook)
        return true;
    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    if (!database)
        return false;
    DatabaseProperties properties = database->properties();
    properties.type = DatabaseType::OpeningBook;
    return database->setProperties(properties, &error);
}

void MainWindow::chooseOpeningNames(const QString &path, bool explicitly)
{
    QSettings settings;
    settings.setValue(QStringLiteral("book/openingNames"), path);
    if (explicitly)
        settings.setValue(QStringLiteral("book/openingNamesExplicit"), true);
    m_openingNamesPath = path;
    m_openingNames.reset();
    m_openingNamesSize = -1;
    updateBookMoves(); // Reads the names.
}

void MainWindow::reloadOpeningNamesIfChanged()
{
    if (m_openingNamesPath.isEmpty())
        return;
    const QFileInfo file(m_openingNamesPath);
    if (m_openingNames && file.lastModified() == m_openingNamesModified && file.size() == m_openingNamesSize)
        return;
    m_openingNamesModified = file.lastModified();
    m_openingNamesSize = file.size();

    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(m_openingNamesPath, &error);
    if (!database) {
        m_openingNames = std::make_unique<OpeningNames>();
        statusBar()->showMessage(tr("Could not read the opening names in %1: %2")
                                     .arg(QDir::toNativeSeparators(m_openingNamesPath), error), 8000);
        return;
    }
    QList<GameRecord> games;
    games.reserve(database->gameCount());
    for (qint64 index = 0; index < database->gameCount(); ++index) {
        if (database->header(index).state != GameState::Live)
            continue; // A line in the trash names nothing.
        if (std::optional<GameRecord> game = database->loadGame(index))
            games << *game;
    }
    m_openingNames = std::make_unique<OpeningNames>(games);
}

void MainWindow::updateDatabaseActions()
{
    const bool hasDatabase = m_database != nullptr;
    m_saveDatabaseAction->setEnabled(hasDatabase && (m_database->isModified() || m_database->location().isEmpty()));
    m_saveDatabaseAsAction->setEnabled(hasDatabase);
    m_connectSourceAction->setEnabled(hasDatabase);
    m_manageSourcesAction->setEnabled(hasDatabase);
    m_databaseSettingsAction->setEnabled(hasDatabase && !m_database->location().isEmpty());
}

void MainWindow::editPersonalSettings()
{
    const QString path = PersonalSettings::path();
    const PersonalSettings current = PersonalSettings::read(path);
    QString lobbyKey;
#ifdef PRAGMA_HAS_PHONE_LINK
    lobbyKey = LobbyIdentity::secretText();
#endif
    PersonalSettingsDialog dialog(current, lobbyKey, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
#ifdef PRAGMA_HAS_PHONE_LINK
    // A key brought from another computer: this one plays as that player now.
    if (dialog.lobbyKey() != lobbyKey) {
        if (LobbyIdentity::setSecretText(dialog.lobbyKey()))
            resetLobbyService();
        else
            QMessageBox::warning(this, tr("Lobby Key"), tr("That is not a valid lobby key: the key of this computer stays."));
    }
#endif
    if (dialog.settings() == current)
        return;
    applyBoardTheme(dialog.settings());
    QString error;
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!PersonalSettings::write(path, dialog.settings(), &error))
        QMessageBox::warning(this, tr("Personal Settings"),
                             tr("Could not save the personal settings in “%1”: %2")
                                 .arg(QDir::toNativeSeparators(path), error));
}

void MainWindow::applyBoardTheme(const PersonalSettings &settings)
{
    if (BoardTheme::byId(settings.boardTheme).id == BoardTheme::current().id)
        return;
    BoardTheme::setCurrent(settings.boardTheme);
    // The boards and the piece views read the style when they paint.
    for (QWidget *widget : QApplication::allWidgets())
        widget->update();
}

QString MainWindow::myName() const
{
    // Read each time: a sync may have brought another computer's version.
    return PersonalSettings::read(PersonalSettings::path()).nameIn(m_database ? m_database->playerRoles()
                                                                               : PlayerRoles());
}

void MainWindow::nameMe(GameRecord &game) const
{
    const QString me = myName();
    if (me.isEmpty())
        return;
    (m_flipBoardAction->isChecked() ? game.black : game.white) = me;
}

void MainWindow::editFolderSettings()
{
    const UserFolders::FolderChoice current = UserFolders::chosenFolders();
    FolderSettingsDialog dialog(current, this);
    if (dialog.exec() != QDialog::Accepted || dialog.choice() == current)
        return;
    UserFolders::setChosenFolders(dialog.choice());
    // The folders are read once: the sync, the phone link and the seeded
    // files would otherwise each be left with a different one.
    QMessageBox::information(this, tr("Folder Settings"),
                             tr("The new folders are used the next time Pragma Chess starts."));
}

void MainWindow::editGraphicsSettings()
{
    GraphicsSettingsDialog dialog(GraphicsSettings::load(), m_coordinatesAction->isChecked(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const GraphicsSettings settings = dialog.settings();
    settings.save();
    applyGraphicsSettings(settings);
    m_coordinatesAction->setChecked(dialog.showCoordinates());
}

void MainWindow::playMoveSound()
{
    if (m_moveSound)
        MoveSound::play();
}

void MainWindow::applyGraphicsSettings(const GraphicsSettings &settings)
{
    Appearance::apply(settings.appearance);
    m_moveSound = settings.moveSound;
    m_boardPanel->setCapturedPiecesBelow(settings.capturedPieces == CapturedPiecesPlacement::BelowBoard);
    m_boardSideColumn->setShowTurn(settings.showTurn);
}

void MainWindow::editDatabaseSettings()
{
    if (!m_database)
        return;
    DatabaseSettingsDialog dialog(m_database->name(), m_database->properties(), this);
    connect(&dialog, &DatabaseSettingsDialog::optimizeRequested, this, [this, &dialog] { optimizeDatabase(&dialog); });
    if (dialog.exec() != QDialog::Accepted || dialog.properties() == m_database->properties())
        return;
    QString error;
    if (!m_database->setProperties(dialog.properties(), &error)) {
        QMessageBox::warning(this, tr("Database Settings"), tr("Could not save the settings: %1").arg(error));
        return;
    }
    // A new name shows at once in Switch Database and over the tree, a new
    // type in the games list (a training database hides the moves).
    rebuildDatabasesMenu();
    m_databaseTree->refresh();
    m_gameListModel->refreshRoles();
    // Only opening books name openings: a database that is no longer one stops doing it.
    if (dialog.properties().type != DatabaseType::OpeningBook && !m_openingNamesPath.isEmpty()
        && QFileInfo(m_openingNamesPath) == QFileInfo(m_database->location()))
        chooseOpeningNames(QString());
}

void MainWindow::connectSource()
{
    if (!m_database)
        return;
    ConnectSourceWizard wizard(m_database->name(), this);
    if (wizard.exec() != QDialog::Accepted)
        return;
    GameSource source = wizard.source();
    QString error;
    if (!m_database->addSource(source, &error)) {
        QMessageBox::warning(this, tr("Connect Source"), tr("Could not connect the source: %1").arg(error));
        return;
    }
    m_sourceSync->syncSource(source.id);
    m_databaseTree->refresh();
    statusBar()->showMessage(tr("Connected %1 to %2").arg(SourceCatalog::displayName(source), m_database->name()), 5000);
}

void MainWindow::openSyncDialog()
{
    SyncDialog dialog(SyncSettings::load(), m_folderSync, this);
    connect(&dialog, &SyncDialog::syncRequested, this, [this](const SyncSettings &settings) {
        settings.save();
        applySyncSettings();
        syncNow(); // The whole thing, in order, as the toolbar button does.
    });
    connect(&dialog, &SyncDialog::manageFilesRequested, this, [this, &dialog](const SyncSettings &settings) {
        if (!(settings == SyncSettings::load())) {
            settings.save();
            applySyncSettings();
        }
        openManageSyncFiles(&dialog);
    });
    if (dialog.exec() != QDialog::Accepted)
        return;
    dialog.settings().save();
    applySyncSettings();
}

void MainWindow::openManageSyncFiles(QWidget *parent)
{
    if (!m_folderSync->hasStore()) {
        QMessageBox::information(parent, tr("Manage Files"), tr("Set up the server first."));
        return;
    }
    ManageSyncFilesDialog dialog(m_folderSync, parent);
    dialog.exec();
}

void MainWindow::applySyncSettings()
{
    const SyncSettings settings = SyncSettings::load();
    RemoteStore *store = settings.createStore(this);
    m_folderSync->setStore(store);
    if (m_syncStore)
        m_syncStore->deleteLater();
    m_syncStore = store;
    m_folderSyncLabel->hide();
    if (!store) {
        m_syncTimer->stop();
        return;
    }
    m_syncTimer->start();
    QTimer::singleShot(1500, m_folderSync, &FolderSync::sync);
}

void MainWindow::syncNow(std::function<void()> then)
{
    if (m_syncPipeline->isRunning()) {
        if (then)
            connect(m_syncPipeline, &SyncPipeline::finished, this, [then](const QStringList &) { then(); },
                    Qt::SingleShotConnection);
        return;
    }

    m_syncPipeline->clear();
    // The order is the point: games arrive first, the project is written with
    // them, and only a folder holding both goes to the server.
    m_syncPipeline->addTask(new SourceSyncTask(m_sourceSync));
    if (!m_projectPath.isEmpty() && isWindowModified()) {
        m_syncPipeline->addTask(new SyncStepTask(tr("Project"), [this] {
            const Project project = captureProject();
            QString error;
            if (!project.saveToFile(m_projectPath, &error))
                return error;
            m_savedProjectYaml = project.toYaml();
            addRecentProject(m_projectPath);
            updateProjectModified();
            return QString();
        }));
    }
    // The session is what a sync without a project file has to offer.
    m_syncPipeline->addTask(new SyncStepTask(tr("Session"), [this] {
        saveSession();
        return QString();
    }));
    // Duplicates merged before the folder goes to the server (the folder sync
    // does it too, for the syncs it starts by itself).
    m_syncPipeline->addTask(new SyncStepTask(tr("Duplicates"), [this] {
        m_folderSync->mergeDuplicates();
        return QString();
    }));
    m_syncPipeline->addTask(new FolderSyncTask(m_folderSync));

    if (then)
        connect(m_syncPipeline, &SyncPipeline::finished, this, [then](const QStringList &) { then(); },
                Qt::SingleShotConnection);
    m_syncPipeline->run();
}

void MainWindow::updateSyncActions()
{
    m_syncNowAction->setEnabled(!m_syncPipeline->isRunning());
}

void MainWindow::reportUnavailableSource(const GameSource &source)
{
    // Once per source while the application runs: the sync comes round every
    // twenty minutes, and the answer was given.
    if (m_unavailableSourcesReported.contains(source.uuid))
        return;
    m_unavailableSourcesReported.insert(source.uuid);
    QMessageBox box(QMessageBox::Warning, tr("Source Not Found"),
                    tr("The source “%1” cannot be found: the file\n%2\nis not on this computer.")
                        .arg(SourceCatalog::displayName(source), QDir::toNativeSeparators(SourceCatalog::localPath(source))),
                    QMessageBox::NoButton, this);
    box.setInformativeText(tr("To fix it, open Database ▸ Manage Sources… and choose the file again, or remove "
                              "the source."));
    QPushButton *ignore = box.addButton(tr("Ignore"), QMessageBox::RejectRole);
    QPushButton *ignoreHere = box.addButton(tr("Ignore on This Computer"), QMessageBox::AcceptRole);
    ignoreHere->setToolTip(tr("This computer stops looking for the file; the source stays in the database for "
                              "the computers that have it"));
    box.setDefaultButton(ignore);
    box.exec();
    if (box.clickedButton() == ignoreHere) {
        SourceCredentials::setIgnoredHere(source.uuid, true);
        m_databaseTree->scheduleRefresh();
    }
}

void MainWindow::manageSources()
{
    if (!m_database)
        return;
    ManageSourcesDialog dialog(m_database.get(), m_sourceSync, this);
    connect(&dialog, &ManageSourcesDialog::connectRequested, this, &MainWindow::connectSource);
    dialog.exec();
    m_databaseTree->refresh();
}

void MainWindow::setAnalysisEnabled(bool enabled)
{
    m_startEngineAction->setText(enabled ? tr("Stop &Analysis") : tr("&Analyze"));
    m_startEngineAction->setIcon(enabled ? themeIcon("media-playback-stop", QStyle::SP_MediaStop)
                                         : themeIcon("media-playback-start", QStyle::SP_MediaPlay));

    if (!enabled) {
        m_explainAction->setChecked(false); // Explaining needs the analysis.
        m_engine->stopAnalysis();
        m_evaluationBar->setEvaluation(std::nullopt);
        m_enginePanel->setEvaluation(std::nullopt);
        m_enginePanel->setStatus(QString());
        return;
    }

    if (!m_engine->isRunning()) {
        const EngineProfile &profile = m_engines.resolve(m_engineId, m_engineName);
        const QString executable = EngineCatalog::executableFor(profile);
        m_engine->clearOptions();
        // The Computing Power: the share of the machine the engine is given
        // (threads, unless the user set them, a cap and a priority), never a
        // weaker search.
        const int cores = QThread::idealThreadCount();
        const EnginePower power = EnginePower::forLevel(profile.power, cores);
        m_engine->setOption(QStringLiteral("Threads"),
                            QString::number(profile.threads > 0 ? profile.threads : power.threads));
        m_engine->setLowPriority(power.lowPriority);
        m_engine->setCpuLimit(power.cpuPercent, power.quotaOfOneCore(cores));
        if (profile.hashMb > 0)
            m_engine->setOption(QStringLiteral("Hash"), QString::number(profile.hashMb));
        if (executable.isEmpty() || !m_engine->start(executable)) {
            const QString message = tr("The engine “%1” was not found. Choose another one or set its "
                                       "executable in Engine ▸ Manage Engines….").arg(profile.name);
            m_enginePanel->setStatus(message);
            // Defer so the action's toggle finishes before being reverted. Turning
            // the analysis off clears the status: the message is written again after.
            QMetaObject::invokeMethod(m_startEngineAction, [this, message] {
                m_startEngineAction->setChecked(false);
                m_enginePanel->setStatus(message);
            }, Qt::QueuedConnection);
            return;
        }
        m_engineId = profile.id;
        m_engineName = profile.name;
        m_engineExecutable = executable;
    }
    m_enginePanel->setStatus(tr("Analyzing…"));
    analyzeCurrentPosition();
}

void MainWindow::manageEngines()
{
    const QString current = m_engines.resolve(m_engineId, m_engineName).id;
    ManageEnginesDialog dialog(m_engines, current, UserFolders::pragmaDir(), this);
    const auto apply = [this, &dialog] {
        m_engines = dialog.catalog();
        {
            QSettings settings;
            m_engines.save(settings);
        }
        // Restart on the engine in use as edited (its executable or parameters
        // may have changed), or on the one chosen with "Use This Engine". The
        // engine belongs to the project: selectEngine() has it saved.
        m_engineId.clear();
        selectEngine(dialog.activeId());
    };
    // "Use This Engine" switches at once, without waiting for OK.
    connect(&dialog, &ManageEnginesDialog::useEngineRequested, this, apply);
    if (dialog.exec() == QDialog::Accepted)
        apply();
}

void MainWindow::selectEngine(const QString &id)
{
    const EngineProfile &profile = m_engines.resolve(id);
    if (profile.id == m_engineId)
        return;
    // The names the training game may show for the engine, before they change:
    // the one it gives itself, or that of its profile.
    const QStringList oldNames{m_engine->name(), m_engineName, tr("Engine")};
    m_engineId = profile.id;
    m_engineName = profile.name;
    m_enginePanel->setEngineName(profile.name);
    updateResourceButtons();
    if (m_trainingModeAction->isChecked()) {
        // The engine plays the other side of the training game: it is the new one now.
        GameRecord header = m_session->game();
        QString &opponent = m_trainingSide == Side::White ? header.black : header.white;
        if (oldNames.contains(opponent)) {
            opponent = profile.name;
            m_session->setHeader(header);
        }
    }
    const bool analyzing = m_startEngineAction->isChecked();
    m_trainingThinking = false; // A search in progress dies with the old engine.
    m_startEngineAction->setChecked(false);
    m_engine->shutdown();
    if (analyzing)
        m_startEngineAction->setChecked(true);
    scheduleSaveSession();
}

void MainWindow::rebuildEngineChoiceMenu()
{
    m_engineChoiceMenu->clear();
    auto *group = new QActionGroup(m_engineChoiceMenu);
    const QString current = m_engines.resolve(m_engineId, m_engineName).id;
    for (const EngineProfile &profile : m_engines.engines()) {
        QAction *action = m_engineChoiceMenu->addAction(profile.name);
        action->setCheckable(true);
        action->setChecked(profile.id == current);
        action->setActionGroup(group);
        const QString id = profile.id;
        connect(action, &QAction::triggered, this, [this, id] { selectEngine(id); });
    }
}

void MainWindow::analyzeCurrentPosition()
{
    m_explainingBefore = false;
    if (!m_startEngineAction->isChecked() || !m_engine->isRunning())
        return;
    if (m_trainingThinking) // The engine is searching the move it will play.
        return;
    // Explain judges the move against the position before it: never searched
    // (the board came straight here), it is searched first, for a moment,
    // then the analysis goes on with the position on the board.
    if (const std::optional<ChessPosition> before = m_explainer->unjudgedBefore(kExplainBeforeDepth)) {
        m_explainingBefore = true;
        m_enginePanel->setStatus(tr("Explain: looking at the position before the move…"));
        SearchLimit limit;
        limit.depth = kExplainBeforeDepth;
        m_engine->analyze(before->fen(), {}, before->sideToMove(), limit);
        return;
    }
    const GameRecord &game = m_session->game();
    QStringList moves;
    moves.reserve(m_session->ply());
    for (int i = 1; i <= m_session->ply(); ++i)
        moves << m_session->moveAt(i).uci; // The line on the board, variations included.
    m_engine->analyze(game.startFen, moves, m_session->position().sideToMove());
}

void MainWindow::setExplainEnabled(bool enabled)
{
    if (enabled) {
        // Explaining starts the engine if it is off.
        m_startEngineAction->setChecked(true);
        if (!m_engine->isRunning()) {
            QMetaObject::invokeMethod(m_explainAction, [this] { m_explainAction->setChecked(false); },
                                      Qt::QueuedConnection);
            return;
        }
        m_engineDock->show();
        m_explainBorder = BoardBorder::Thinking; // Until the engine answers.
        updateBoardBorder();
        m_explainer->setEnabled(true);
        analyzeCurrentPosition(); // The position before the move first, if it was never searched.
        return;
    }
    m_explainer->setEnabled(false);
    m_explanationPlayback.clear();
    m_explanation = MoveExplanation();
    m_explainBorder = BoardBorder::Plain;
    updateBoardBorder();
    m_board->stopSequence();
    m_board->setExplanation({}, {});
    m_enginePanel->setExplanation(QString());
    m_explanationText.clear();
}

void MainWindow::updateBoardBorder()
{
    // Explain speaks first, while it is on; under it the tutor's alert keeps
    // the border red until the user has chosen what to do with the move, and
    // a checkmate on the board is red too.
    if (m_explainBorder != BoardBorder::Plain)
        m_board->setBorder(m_explainBorder);
    else if (m_tutorReply || m_session->position().isCheckmate())
        m_board->setBorder(BoardBorder::Alert);
    else
        m_board->setBorder(BoardBorder::Plain);
}

void MainWindow::updateExplainer()
{
    // Who asks: the side played in training, otherwise the one seen from below.
    m_explainer->setViewer(m_trainingModeAction->isChecked() ? m_trainingSide
                           : m_flipBoardAction->isChecked() ? Side::Black
                                                            : Side::White);
    const int ply = m_session->ply();
    // A null move is no move to explain: the position is explained as a start.
    const std::optional<ChessMove> played = m_session->lastMove();
    const bool moved = played && !played->isNull();
    m_explainer->setPosition(m_session->position(),
                             moved ? std::optional<ChessPosition>(m_session->positionAt(ply - 1)) : std::nullopt,
                             moved ? played : std::nullopt);
}

void MainWindow::newGame()
{
    // While playing online "new game" may mean either: a new online game (as
    // the toolbar's New Online Game) or one to analyse. Either way the game in
    // progress is kept or resigned first.
    if (m_onlinePlay) {
        using Choice = NewGameChoiceDialog::Choice;
        Choice choice = m_rememberedNewGame.value_or(Choice::Online);
        if (!m_rememberedNewGame) {
            NewGameChoiceDialog dialog(choice, false, this);
            if (dialog.exec() != QDialog::Accepted)
                return;
            choice = dialog.choice();
            if (dialog.remember())
                m_rememberedNewGame = choice; // For this session only, never saved.
        }
        if (choice == Choice::Online)
            playOnline(false);
        else
            leaveOnlineThen([this] { newGame(); });
        return;
    }
    if (!mayReplaceBoardGame()) // A game not saved anywhere: saved, let go, or kept.
        return;
    m_trainingModeAction->setChecked(false); // A plain new game is not a training one.
    GameRecord game;
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    nameMe(game);
    startGame(game);
    statusBar()->showMessage(tr("New game: enter the moves on the board"), 5000);
}

void MainWindow::setUpPosition()
{
    // An online game in progress is kept, or resigned and saved, first.
    if (m_onlinePlay) {
        leaveOnlineThen([this] { setUpPosition(); });
        return;
    }
    PositionSetupDialog dialog(m_session->position().fen(), m_flipBoardAction->isChecked(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!mayReplaceBoardGame()) // A game not saved anywhere: saved, let go, or kept.
        return;
    m_trainingModeAction->setChecked(false); // A position set up is studied, not played against the engine.
    GameRecord game;
    const QString fen = dialog.fen();
    if (fen != ChessPosition::startingPosition().fen())
        game.startFen = fen;
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    nameMe(game);
    startGame(game);
    statusBar()->showMessage(tr("Position set up: enter the moves on the board"), 5000);
}

bool MainWindow::saveGameToAnotherDatabase()
{
    UserFolders::ensureDatabasesDir();
    const QString path = QFileDialog::getOpenFileName(this, tr("Save to Another Database"),
                                                      UserFolders::databasesDir(),
                                                      tr("Pragma databases (*.%1)").arg(QLatin1String(UserFolders::databaseSuffix)));
    if (path.isEmpty())
        return false;
    // The open one, chosen from the file dialog: saved as usual, its list follows.
    if (m_database && QFileInfo(path) == QFileInfo(m_database->location())) {
        saveGameToDatabase();
        return m_openGameIndex >= 0;
    }
    QString error;
    const std::unique_ptr<SqliteGameDatabase> other = SqliteGameDatabase::open(path, &error);
    if (!other || other->addGame(m_session->game(), &error) < 0) {
        QMessageBox::warning(this, tr("Save Game"), tr("Could not save the game: %1").arg(error));
        return false;
    }
    statusBar()->showMessage(tr("Game saved to %1").arg(other->name()), 3000);
    return true;
}

bool MainWindow::canLeaveGame()
{
    // Online, the board belongs to the game being played.
    if (!m_onlinePlay)
        return true;
    statusBar()->showMessage(tr("The board stays on the online game until it ends."), 5000);
    return false;
}

void MainWindow::syncChapterGame()
{
    m_chapters.game().game = m_session->game();
    m_chapters.settle(); // Something on the board: a project without chapters has its first.
}

void MainWindow::rememberPlace()
{
    m_chapters.chapter().ply = m_session->ply();
    m_chapters.chapter().path = m_session->path();
}

void MainWindow::returnToPlace()
{
    m_session->goToLine(m_chapters.chapter().path, m_chapters.chapter().ply);
}

void MainWindow::loadChapterGame()
{
    const ChapterGame &entry = m_chapters.game();
    const qint64 index = gameIndexOf(entry.game.uid);
    const std::optional<GameRecord> stored = index >= 0 ? m_database->loadGame(index) : std::nullopt;
    m_openGameIndex = stored ? index : -1;
    m_gameView->clearSelection();
    if (stored) {
        const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
        if (proxyIndex.isValid())
            m_gameView->selectRow(proxyIndex.row());
        orientBoardForMe(*stored);
    }
    m_session->setGame(stored.value_or(entry.game));
}

void MainWindow::switchToChapterGame(int game, const QList<int> &path, int ply)
{
    if (game < 0 || game >= m_chapters.chapter().games.size())
        return;
    if (game != m_chapters.chapter().currentGame) {
        if (!canLeaveGame())
            return;
        // Another game is studied, not played: training goes off first.
        m_trainingModeAction->setChecked(false);
        m_chapters.chapter().currentGame = game;
        loadChapterGame();
        chapterChanged();
    }
    m_session->goToLine(path, ply);
}

void MainWindow::relinkChapterGame()
{
    const qint64 index = gameIndexOf(m_chapters.game().game.uid);
    m_openGameIndex = index;
    m_gameView->clearSelection();
    const QModelIndex proxyIndex =
        index >= 0 ? m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0)) : QModelIndex();
    if (proxyIndex.isValid())
        m_gameView->selectRow(proxyIndex.row());
    updateGameActions();
}

void MainWindow::chapterChanged()
{
    m_chapters.settle();
    m_moveView->refresh();
    updateWindowTitle(); // The chapter's title may be in it.
    scheduleSaveSession();
}

QString MainWindow::contentLanguage()
{
    return LocalizedText::supported(UiLanguage::effective());
}

void MainWindow::editProjectSettings()
{
    const QString fileName = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    ProjectSettingsDialog dialog(m_projectName, m_projectPath.isEmpty() ? QString() : QFileInfo(m_projectPath).absoluteFilePath(),
                                 fileName, m_multilingual, m_chapters.language, contentLanguage(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const bool changed = dialog.name() != m_projectName || dialog.isMultilingual() != m_multilingual;
    const bool relanguaged = dialog.language() != m_chapters.language;
    m_projectName = dialog.name();
    m_multilingual = dialog.isMultilingual();
    // The texts are shown and written in another language: the chapters stay, their words change.
    m_chapters.language = m_multilingual ? dialog.language() : contentLanguage();
    if (relanguaged) {
        m_moveView->refresh();
    }
    updateWindowTitle();
    if (!changed)
        return;
    scheduleSaveSession(); // The project has changes: its name is saved with it.
}

int MainWindow::chapterGameStartNumber(int index) const
{
    const GameRecord &game = index == m_chapters.chapter().currentGame ? m_session->game()
                                                                      : m_chapters.chapter().games.at(index).game;
    if (game.startFen.isEmpty())
        return 1;
    const std::optional<ChessPosition> start = ChessPosition::fromFen(game.startFen, ChessPosition::Kings::Optional);
    return start ? start->fullMoveNumber() : 1;
}

void MainWindow::changeMoveNumber(int index)
{
    if (index < 0 || index >= m_chapters.chapter().games.size())
        return;
    // The number is the start position's: the board goes to that game first.
    if (index != m_chapters.chapter().currentGame) {
        switchToChapterGame(index, {}, 0);
        if (index != m_chapters.chapter().currentGame)
            return;
    }
    bool ok = false;
    const int number = QInputDialog::getInt(this, tr("Change Move Number"),
                                            tr("Number of the first move:"), chapterGameStartNumber(index), 1, 9999, 1, &ok);
    if (!ok || number == chapterGameStartNumber(index))
        return;
    // As PGN has it: the move number of the start position's FEN; the
    // standard position numbered from 1 needs none.
    GameRecord game = m_session->game();
    const std::optional<ChessPosition> start =
        game.startFen.isEmpty() ? ChessPosition::startingPosition() : ChessPosition::fromFen(game.startFen, ChessPosition::Kings::Optional);
    if (!start)
        return;
    QStringList fields = start->fen().split(QLatin1Char(' '));
    fields.last() = QString::number(number);
    const QString fen = fields.join(QLatin1Char(' '));
    const QString before = game.startFen;
    game.startFen = fen == ChessPosition::startingPosition().fen() ? QString() : fen;
    const QList<int> path = m_session->path();
    const int ply = m_session->ply();
    m_session->setGame(game);
    m_session->goToLine(path, ply);
    QString error;
    if (!storeOpenGame(&error)) {
        game.startFen = before;
        m_session->setGame(game);
        m_session->goToLine(path, ply);
        QMessageBox::warning(this, tr("Change Move Number"), tr("Could not save the game: %1").arg(error));
    }
    chapterChanged();
}

void MainWindow::deleteChapterGame(int index, const QString &title)
{
    if (index < 0 || index >= m_chapters.chapter().games.size())
        return;
    syncChapterGame();
    const bool current = index == m_chapters.chapter().currentGame;
    if (current && !canLeaveGame())
        return;
    // What would be lost: moves not saved in a database, or text written
    // around them. A game of the database stays there.
    const ChapterGame &doomed = m_chapters.chapter().games.at(index);
    const bool unsaved = doomed.game.uid.isEmpty() && (!doomed.game.moves.isEmpty() || !doomed.game.startFen.isEmpty());
    if (unsaved || !doomed.paragraphs.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, title,
            unsaved ? tr("This game is not saved in a database: its moves will be lost. Delete it?")
                    : tr("The text written around this game will be lost; the game stays in the database. "
                         "Delete it from the chapter?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }
    if (current) {
        m_trainingModeAction->setChecked(false);
        rememberPlace();
    }
    if (!m_chapters.removeGame(index))
        return;
    if (current) {
        // The board goes to the game before (or, for the first, the next).
        loadChapterGame();
        returnToPlace();
    }
    chapterChanged();
}

void MainWindow::insertGameBreak(int after)
{
    if (!canLeaveGame())
        return;
    m_trainingModeAction->setChecked(false);
    // Always a new game, right under the one the user is in; it stays,
    // empty, until something is entered in it or it is deleted.
    syncChapterGame();
    rememberPlace();
    m_chapters.insertGame(after < 0 ? m_chapters.chapter().currentGame : after);
    GameRecord game;
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    nameMe(game);
    m_chapters.game().game = game;
    loadChapterGame();
    chapterChanged();
    statusBar()->showMessage(tr("Game break: a new game, from the starting position"), 4000);
}

void MainWindow::newChapter()
{
    if (!canLeaveGame())
        return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, tr("New Chapter"), tr("Title of the chapter:"), QLineEdit::Normal,
                                                ChapterBook::defaultTitle(m_chapters.hasChapters() ? int(m_chapters.chapters.size()) + 1 : 1),
                                                &ok);
    if (!ok)
        return;
    m_trainingModeAction->setChecked(false);
    rememberPlace();
    m_chapters.addChapter(title);
    loadChapterGame();
    chapterChanged();
}

void MainWindow::switchChapter(int index)
{
    if (index == m_chapters.current || index < 0 || index >= m_chapters.chapters.size() || !canLeaveGame())
        return;
    m_trainingModeAction->setChecked(false);
    rememberPlace();
    m_chapters.current = index;
    loadChapterGame();
    returnToPlace();
    chapterChanged();
}

void MainWindow::fillChapterMenu()
{
    m_switchChapterMenu->clear();
    if (!m_chapters.hasChapters()) {
        m_switchChapterMenu->addAction(tr("(No Chapter)"))->setEnabled(false);
    } else {
        auto *group = new QActionGroup(m_switchChapterMenu);
        for (int i = 0; i < m_chapters.chapters.size(); ++i) {
            QAction *action = m_switchChapterMenu->addAction(m_chapters.chapters.at(i).title.text(m_chapters.language));
            action->setCheckable(true);
            action->setChecked(i == m_chapters.current);
            action->setActionGroup(group);
            connect(action, &QAction::triggered, this, [this, i] { switchChapter(i); });
        }
    }
    // Managing them goes with the list, under it.
    m_switchChapterMenu->addSeparator();
    m_switchChapterMenu->addAction(tr("&Manage Chapters…"), this, &MainWindow::manageChapters);
}

void MainWindow::manageChapters()
{
    QList<ManageChaptersDialog::Entry> entries;
    for (int i = 0; m_chapters.hasChapters() && i < m_chapters.chapters.size(); ++i) {
        const Chapter &chapter = m_chapters.chapters.at(i);
        int games = 0;
        for (const ChapterGame &game : chapter.games)
            games += game.isEmpty() ? 0 : 1;
        entries << ManageChaptersDialog::Entry{i, chapter.title, games};
    }
    ManageChaptersDialog dialog(entries, m_chapters.current, m_chapters.language, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QList<ManageChaptersDialog::Entry> chosen = dialog.entries();
    rememberPlace();
    syncChapterGame();
    QList<Chapter> chapters;
    int current = -1;
    for (const ManageChaptersDialog::Entry &entry : chosen) {
        Chapter chapter = entry.source >= 0 ? m_chapters.chapters.at(entry.source) : Chapter();
        chapter.title = entry.title.isEmpty()
            ? LocalizedText(m_chapters.language, ChapterBook::defaultTitle(int(chapters.size()) + 1))
            : entry.title;
        if (m_chapters.hasChapters() && entry.source == m_chapters.current)
            current = int(chapters.size());
        chapters << chapter;
    }
    // Every chapter deleted: the project is without chapters, an empty game on the board.
    const bool sameChapter = current >= 0;
    if (!sameChapter && !canLeaveGame())
        return;
    // Chapters that came by themselves stay so unless the user changed them
    // here (renamed, added, deleted, reordered): those are chapters asked for.
    bool unchanged = m_chapters.hasChapters() && chosen.size() == m_chapters.chapters.size();
    for (int i = 0; unchanged && i < chosen.size(); ++i)
        unchanged = chosen.at(i).source == i && chapters.at(i).title == m_chapters.chapters.at(i).title;
    m_chapters.setChapters(chapters, sameChapter ? current : 0, false, m_chapters.isAutomatic() && unchanged);
    if (!sameChapter) {
        m_trainingModeAction->setChecked(false);
        loadChapterGame();
        returnToPlace();
    }
    chapterChanged();
}

bool MainWindow::mayReplaceBoardGame()
{
    // With chapters nothing is replaced: the new game comes after.
    syncChapterGame();
    if (m_chapters.hasChapters() || !m_chapters.game().holdsWork())
        return true;
    QMessageBox box(QMessageBox::Question, tr("Game Not Saved"),
                    tr("The game on the board is not saved in a database."),
                    QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setInformativeText(m_database ? tr("Save it to %1 before it is replaced?").arg(m_database->name())
                                      : tr("It will be replaced and lost."));
    if (m_database)
        box.addButton(QMessageBox::Save);
    box.setDefaultButton(m_database ? QMessageBox::Save : QMessageBox::Cancel);
    box.button(QMessageBox::Discard)->setText(tr("Don't Save"));
    const int answer = box.exec();
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Save) {
        saveGameToDatabase();
        syncChapterGame();
        return !m_chapters.game().holdsWork(); // Not saved after all: nothing is replaced.
    }
    m_replaceBoardGame = true; // Let go: the next game takes its place.
    return true;
}

bool MainWindow::keepsBoardGame()
{
    // A new game goes at the end of the chapter: the games before it stay.
    // Without chapters it takes the place of the game on the board, unless
    // that one holds work kept nowhere else and the user did not let it go
    // (mayReplaceBoardGame): it then becomes a chapter, rather than be lost.
    syncChapterGame();
    const bool letGo = std::exchange(m_replaceBoardGame, false);
    return !m_chapters.game().isEmpty() && (m_chapters.hasChapters() || (m_chapters.game().holdsWork() && !letGo));
}

void MainWindow::startGame(const GameRecord &game)
{
    if (keepsBoardGame())
        m_chapters.breakGame();
    m_gameView->clearSelection();
    m_openGameIndex = -1;
    m_session->setGame(game);
    chapterChanged();
}

void MainWindow::newTraining(bool alwaysAsk)
{
    // Training is against the engine: an online game in progress is left first.
    if (m_onlinePlay) {
        leaveOnlineThen([this, alwaysAsk] { newTraining(alwaysAsk); });
        return;
    }
    using Choice = NewTrainingDialog::Choice;
    Choice choice = m_rememberedTraining.value_or(m_trainingSide == Side::White ? Choice::White : Choice::Black);
    if (alwaysAsk || !m_rememberedTraining) {
        NewTrainingDialog dialog(choice, m_rememberedTraining.has_value(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        choice = dialog.choice();
        // Unticking it forgets the choice: the toolbar asks again.
        m_rememberedTraining = dialog.remember() ? std::optional<Choice>(choice) : std::nullopt;
    }
    if (!mayReplaceBoardGame()) // A game not saved anywhere: saved, let go, or kept.
        return;
    m_trainingSide = NewTrainingDialog::sideFor(choice);
    const GameRecord game = trainingHeader();
    const QString engineName = m_trainingSide == Side::White ? game.black : game.white;

    m_flipBoardAction->setChecked(m_trainingSide == Side::Black); // Play from the bottom.
    m_startEngineAction->setChecked(true); // The score stays visible throughout.
    startGame(game);
    m_trainingModeAction->setChecked(true);
    updateTraining();                      // setChecked() is silent when the flag was already on.
    statusBar()->showMessage(m_trainingSide == Side::White
                                 ? tr("Training: you play White against %1").arg(engineName)
                                 : tr("Training: you play Black against %1").arg(engineName),
                             5000);
}

GameRecord MainWindow::trainingHeader() const
{
    // Who the user is: as the database knows them from "Who Is This?", else
    // as the personal settings say.
    QString me = myName();
    if (me.isEmpty())
        me = tr("Me");
    const QString engineName = m_engine->name().isEmpty()
        ? (m_engineName.isEmpty() ? tr("Engine") : m_engineName)
        : m_engine->name();

    GameRecord game;
    game.event = tr("Training");
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    game.white = m_trainingSide == Side::White ? me : engineName;
    game.black = m_trainingSide == Side::White ? engineName : me;
    return game;
}

void MainWindow::setTrainingMode(bool enabled)
{
    if (!enabled) {
        m_trainingThinking = false;
        m_enginePanel->setLineHidden(false);
        clearTutor();
    }
    syncBoard(); // The user may not move for the engine.
    updateTraining();
}

bool MainWindow::isEngineTurn() const
{
    return m_trainingModeAction->isChecked() && m_session->position().sideToMove() != m_trainingSide;
}

bool MainWindow::isOpponentTurn() const
{
    return m_onlinePlay && (!m_onlineSide || m_session->position().sideToMove() != *m_onlineSide);
}

void MainWindow::playOnline(bool alwaysAsk)
{
    if (m_online) {
        // A new online game: the current one is kept or resigned first.
        leaveOnlineThen([this, alwaysAsk] { playOnline(alwaysAsk); });
        return;
    }
    LichessBoardClient::Seek seek;
    if (alwaysAsk || !m_rememberedOnline) {
        PlayOnlineDialog dialog(m_rememberedOnline.has_value(), this);
        if (dialog.exec() != QDialog::Accepted || dialog.account().id.isEmpty())
            return;
        m_onlineAccount = dialog.account();
        seek = dialog.seek();
        // Unticking it forgets the choice: the toolbar asks again.
        m_rememberedOnline = dialog.remember() ? std::optional<LichessBoardClient::Seek>(seek) : std::nullopt;
    } else {
        seek = *m_rememberedOnline;
    }
    if (!startOnlineClient())
        return;
    setOnlinePlay(true);
    m_onlineSide.reset();
    m_enginePanel->setStatus(tr("Looking for an opponent on %1 (%2+%3, %4)…")
                                 .arg(OnlineAccounts::platformName(m_onlineAccount.platform))
                                 .arg(seek.minutes)
                                 .arg(seek.increment)
                                 .arg(seek.rated ? tr("rated") : tr("casual")));
    m_online->seek(seek);
}

void MainWindow::leaveOnlineThen(std::function<void()> next)
{
    if (!m_online) {
        next();
        return;
    }
    if (!m_online->isPlaying()) {
        // Only looking for an opponent: nothing to lose.
        m_online->cancelSeek();
        m_online.reset();
        setOnlinePlay(false);
        next();
        return;
    }
    QMessageBox box(QMessageBox::Question, tr("Game in Progress"),
                    tr("You are playing an online game against %1. Keep playing it, or resign it and start a new game?")
                        .arg(m_onlineSide == Side::White ? m_session->game().black : m_session->game().white),
                    QMessageBox::NoButton, this);
    QPushButton *keep = box.addButton(tr("Keep Playing"), QMessageBox::RejectRole);
    QPushButton *resign = box.addButton(tr("Resign"), QMessageBox::DestructiveRole);
    box.setDefaultButton(keep);
    box.setEscapeButton(keep);
    box.exec();
    if (box.clickedButton() != resign || !m_online)
        return;
    // The new game starts once the end has come back and the game is saved:
    // started now, the end would be written into it.
    m_afterOnlineGame = std::move(next);
    m_online->resign();
}

bool MainWindow::startOnlineClient()
{
    const QString token = SourceCredentials::token(m_onlineAccount.id);
    if (token.isEmpty()) {
        QMessageBox::warning(this, tr("Play Online"), tr("The account %1 has no sign-in on this computer: sign in again.").arg(m_onlineAccount.username));
        return false;
    }
    m_online = std::make_unique<LichessBoardClient>(token);
    connect(m_online.get(), &LichessBoardClient::gameStarted, this, &MainWindow::onlineGameStarted);
    connect(m_online.get(), &LichessBoardClient::gameUpdated, this, &MainWindow::onlineGameUpdated);
    connect(m_online.get(), &LichessBoardClient::gameFinished, this, &MainWindow::onlineGameFinished);
    connect(m_online.get(), &LichessBoardClient::failed, this, &MainWindow::onlineFailed);
    return true;
}

void MainWindow::resumeOnlineGame()
{
    // The game being played when the application was closed (or crashed):
    // the platform kept it going, so it is followed again where it is now.
    QSettings settings;
    const QString gameId = settings.value(QLatin1String(kActiveGameKey)).toString();
    const QString accountId = settings.value(QLatin1String(kActiveAccountKey)).toString();
    if (gameId.isEmpty())
        return;
    // Kept in a variable: find() points into it.
    const OnlineAccounts accounts = OnlineAccounts::load(settings);
    const OnlineAccount *account = accounts.find(accountId);
    if (!account || SourceCredentials::token(account->id).isEmpty()) {
        forgetActiveOnlineGame();
        return;
    }
    m_onlineAccount = *account;
    if (!startOnlineClient())
        return;
    m_resumingOnline = true;
    setOnlinePlay(true);
    m_onlineSide.reset();
    m_enginePanel->setStatus(tr("Reconnecting to your game on %1…").arg(OnlineAccounts::platformName(account->platform)));
    m_online->resume(gameId);
}

void MainWindow::forgetActiveOnlineGame()
{
    QSettings settings;
    settings.remove(QLatin1String(kActiveGameKey));
    settings.remove(QLatin1String(kActiveAccountKey));
}

void MainWindow::stopOnline()
{
    if (!m_online)
        return;
    if (m_online->isPlaying()) {
        const auto answer = QMessageBox::question(this, tr("Stop Playing Online"),
                                                  tr("Resign the game against %1?").arg(m_onlineSide == Side::White ? m_session->game().black : m_session->game().white),
                                                  QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        m_online->resign(); // The stream brings the end, and the game is saved then.
        return;
    }
    m_online->cancelSeek();
    m_online.reset();
    setOnlinePlay(false);
    statusBar()->showMessage(tr("No longer looking for an opponent."), 5000);
}

void MainWindow::setOnlinePlay(bool on)
{
    if (m_onlinePlay == on)
        return;
    m_onlinePlay = on;
    m_onlineModeAction->setChecked(on);
    // Against cheating: nothing that thinks for the user runs while they play.
    if (on) {
        m_trainingModeAction->setChecked(false);
        m_startEngineAction->setChecked(false);
        m_explainAction->setChecked(false);
    }
    // New Training stays available: like New Game, it asks about the game in progress first.
    for (QAction *action : {m_startEngineAction, m_explainAction, m_trainingModeAction})
        action->setEnabled(!on);
    // The panels are the user's (and the project's): the Opening Tree stays
    // where it is, its moves covered while the game lasts.
    m_bookPanel->setCensored(on ? tr("The Opening Tree cannot be used while playing online.") : QString());
    if (!on) {
        m_onlineSide.reset();
        m_enginePanel->setStatus(QString());
    }
    syncBoard();
}

void MainWindow::onlineGameStarted(const OnlineGame &game)
{
    // Remembered until the game ends, so a restart follows it again.
    m_resumingOnline = false;
    QSettings settings;
    settings.setValue(QLatin1String(kActiveGameKey), game.id);
    settings.setValue(QLatin1String(kActiveAccountKey), m_onlineAccount.id);
    const bool white = game.white.compare(m_onlineAccount.username, Qt::CaseInsensitive) == 0;
    m_onlineSide = white ? Side::White : Side::Black;
    GameRecord record;
    record.white = game.white;
    record.black = game.black;
    record.whiteElo = game.whiteRating;
    record.blackElo = game.blackRating;
    record.event = tr("%1 %2 game").arg(OnlineAccounts::platformName(m_onlineAccount.platform), game.rated ? tr("rated") : tr("casual"));
    record.site = QStringLiteral("https://lichess.org/%1").arg(game.id);
    record.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    record.result = QStringLiteral("*");
    record.startFen = game.initialFen;
    startGame(record);
    m_flipBoardAction->setChecked(!white);
    m_engineDock->show();
    statusBar()->showMessage(tr("Playing %1 as %2.").arg(white ? game.black : game.white, white ? tr("White") : tr("Black")), 8000);
}

void MainWindow::onlineGameUpdated(const OnlineGame &game)
{
    // The platform's moves are the truth: ours are played ahead and confirmed
    // here, the opponent's arrive here, and a refused move is taken back.
    QStringList ours;
    for (const MoveRecord &move : m_session->game().moves)
        ours << move.uci;
    const bool extends = game.moves.size() >= ours.size()
                         && std::equal(ours.cbegin(), ours.cend(), game.moves.cbegin());
    // Several moves at once (a game followed again after a restart) are set
    // up at once rather than slid one after the other.
    if (!extends || game.moves.size() - ours.size() > 1) {
        GameRecord record = m_session->game();
        record.moves.clear();
        record.variations.clear();
        ChessPosition position = record.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                           : ChessPosition::fromFen(record.startFen, ChessPosition::Kings::Optional).value_or(ChessPosition::startingPosition());
        for (const QString &uci : game.moves) {
            const std::optional<ChessMove> move = position.moveFromUci(uci);
            if (!move)
                break;
            record.moves << MoveRecord{position.san(*move), uci, {}};
            position.play(*move);
        }
        record.plyCount = int(record.moves.size());
        m_session->setGame(record);
        m_session->goToEnd();
    } else {
        for (qsizetype i = ours.size(); i < game.moves.size(); ++i) {
            m_session->goToEnd();
            const std::optional<ChessMove> move = m_session->position().moveFromUci(game.moves.at(i));
            if (!move)
                break;
            m_animateNextBoard = true; // The opponent's move slides across the board, as the engine's does.
            m_session->playMove(*move);
        }
    }
    updateOnlineStatus(game);
}

void MainWindow::updateOnlineStatus(const OnlineGame &game)
{
    const auto clock = [](int ms) {
        const int seconds = qMax(0, ms / 1000);
        return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    };
    const QString white = QStringLiteral("%1 (%2) %3").arg(game.white).arg(game.whiteRating).arg(clock(game.whiteTimeMs));
    const QString black = QStringLiteral("%1 (%2) %3").arg(game.black).arg(game.blackRating).arg(clock(game.blackTimeMs));
    QString status = tr("Online: %1 – %2").arg(white, black);
    if (game.isOver())
        status = game.endText() + QLatin1Char(' ') + status;
    else if (isOpponentTurn())
        status += QStringLiteral(" — ") + tr("waiting for the opponent…");
    else
        status += QStringLiteral(" — ") + tr("your move");
    m_enginePanel->setStatus(status);
}

void MainWindow::onlineGameFinished(const OnlineGame &game)
{
    forgetActiveOnlineGame();
    GameRecord record = m_session->game();
    record.result = game.result();
    m_session->setHeader(record);
    if (!game.timeControl.isEmpty()) {
        TimeControl::set(record, game.timeControl);
        m_session->setTags(record.tags);
    }
    if (record.result != QLatin1String("*"))
        saveGameToDatabase(); // The game goes to the open database, like a finished training game.
    statusBar()->showMessage(game.endText(), 10000);
    updateOnlineStatus(game);
    // The client is in the middle of its signal: let go of it afterwards.
    QTimer::singleShot(0, this, [this, text = game.endText()] {
        m_online.reset();
        setOnlinePlay(false);
        m_enginePanel->setStatus(text);
        // What was waiting for this game to end: a new game, online or not.
        if (std::function<void()> next = std::exchange(m_afterOnlineGame, {}))
            next();
    });
}

void MainWindow::onlineFailed(const QString &message)
{
    // A game that cannot be followed again is forgotten; one whose connection
    // was lost while playing is kept, and the next start tries again.
    if (m_resumingOnline) {
        m_resumingOnline = false;
        forgetActiveOnlineGame();
    }
    QMessageBox::warning(this, tr("Play Online"), message);
    QTimer::singleShot(0, this, [this] {
        m_online.reset();
        setOnlinePlay(false);
        m_afterOnlineGame = {}; // The game did not end as asked: nothing starts by itself.
    });
}

void MainWindow::updateTraining()
{
    const bool training = m_trainingModeAction->isChecked();
    // The tutor's alert is about one move: leaving it takes the alert away.
    if (m_tutorReply && m_session->ply() != m_tutorPly)
        clearTutor();
    // The best line is the user's move: it may only be shown once they played.
    m_enginePanel->setLineHidden(training && !isEngineTurn());
    if (!training || m_tutorReply) // With the alert up the engine waits for the user's choice.
        return;
    // Looking back at an earlier move is not a turn to answer; a move the
    // user plays on the board is, even where the game goes on after it
    // (a game played before, tried again): the engine answers and the tutor
    // judges, and an answer other than the next move starts a variation.
    const bool playedNow = std::exchange(m_trainingMovePlayed, false);
    if (m_session->ply() != m_session->plyCount() && !playedNow)
        return;
    const ChessPosition &position = m_session->position();
    if (position.isCheckmate() || position.isStalemate()) {
        recordTrainingResult();
        return;
    }
    if (isEngineTurn())
        playEngineMove();
}

void MainWindow::playEngineMove()
{
    if (m_trainingThinking)
        return;
    if (!m_engine->isRunning()) {
        m_startEngineAction->setChecked(true); // Starts the engine, or reports why it cannot.
        if (!m_engine->isRunning())
            return;
    }
    const GameRecord &game = m_session->game();
    QStringList moves;
    moves.reserve(m_session->ply());
    for (int i = 1; i <= m_session->ply(); ++i)
        moves << m_session->moveAt(i).uci; // The line on the board, variations included.
    m_trainingThinking = true;
    m_lastEvaluation = {};
    m_enginePanel->setStatus(tr("Thinking…"));
    // The search is still run while the book answers: the tutor judges the
    // user's move against it.
    m_engine->analyze(game.startFen, moves, m_session->position().sideToMove(), {kTrainingDepth, 0, false});
}

std::optional<ChessMove> MainWindow::bookReply() const
{
    if (!m_book)
        return std::nullopt;
    const QList<PolyglotBook::Move> moves = m_book->moves(m_session->position());
    const quint32 total = PolyglotBook::totalWeight(moves);
    if (total == 0)
        return std::nullopt;
    const int index = PolyglotBook::pick(moves, QRandomGenerator::global()->bounded(total));
    return index < 0 ? std::nullopt : std::optional<ChessMove>(moves.at(index).move);
}

void MainWindow::finishEngineMove()
{
    if (!m_trainingThinking)
        return;
    m_trainingThinking = false;
    // In the opening the engine plays the book, each move as often as its
    // weight says — that is what the weights of the Opening Tree are for;
    // out of the book, its own best move.
    std::optional<ChessMove> move = bookReply();
    if (!move && !m_lastEvaluation.pv.isEmpty())
        move = m_session->position().moveFromUci(m_lastEvaluation.pv.constFirst());
    if (!move) {
        m_enginePanel->setStatus(tr("The engine found no move to play."));
        return;
    }
    // The tutor: a jump from the evaluation the user moved from to the one
    // of this search says their move was an error, with no analysis of its own.
    const EngineEvaluation evaluation = m_lastEvaluation;
    const std::optional<ChessMove> played = m_session->lastMove();
    if (played && m_trainingBaseline.depth >= kTutorMinDepth
        && m_trainingBaselineFen == m_session->positionAt(m_session->ply() - 1).fen()) {
        const TrainingTutor::Alert alert = TrainingTutor::judge(m_trainingBaseline, evaluation, m_trainingSide, *played,
                                                                m_session->positionAt(m_session->ply() - 1));
        if (alert != TrainingTutor::Alert::None) {
            holdEngineReply(*move, evaluation, alert);
            return;
        }
    }
    playEngineReply(*move, evaluation);
}

void MainWindow::playEngineReply(const ChessMove &move, const EngineEvaluation &evaluation)
{
    // What the engine expects after its move: the user's next one is judged
    // against it, unless the analysis goes deeper while they think.
    m_trainingBaseline = evaluation;
    m_trainingBaseline.pv = evaluation.pv.mid(1);
    m_trainingBaseline.depth = qMax(0, evaluation.depth - 1);
    if (evaluation.pv.isEmpty() || evaluation.pv.constFirst() != move.uci())
        m_trainingBaseline.depth = 0; // A book move the search did not look at: the analysis will tell.
    ChessPosition next = m_session->position();
    next.play(move);
    m_trainingBaselineFen = next.fen();
    m_animateNextBoard = true; // syncBoard() slides it across the board.
    playMove(move);
}

void MainWindow::holdEngineReply(const ChessMove &reply, const EngineEvaluation &evaluation, TrainingTutor::Alert alert)
{
    if (alert == TrainingTutor::Alert::None)
        return;
    m_tutorReply = reply;
    m_tutorEvaluation = evaluation;
    m_tutorPly = m_session->ply();
    m_tutorAlert = alert;
    const QString move = m_session->positionAt(m_tutorPly - 1)
                             .lineText({m_session->moveAt(m_tutorPly).uci}, 1, SanStyle::Figurines);
    const QString before = m_trainingBaseline.text();
    const QString after = evaluation.text();
    QString message;
    switch (alert) {
    case TrainingTutor::Alert::Blunder: message = tr("Blunder: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::Mistake: message = tr("Mistake: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::Inaccuracy: message = tr("Inaccuracy: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::MissedChance:
        message = tr("Missed chance: %1 lets your advantage go (%2 → %3).").arg(move, before, after);
        break;
    case TrainingTutor::Alert::None: return;
    }
    m_enginePanel->setTutorAlert(message);
    m_engineDock->show();
    updateBoardBorder(); // Red: the game stopped on this move.
    scheduleSaveSession(); // A client closed now opens with the alert up.
}

void MainWindow::clearTutor()
{
    if (!m_tutorReply)
        return;
    m_tutorReply.reset();
    m_tutorAlert = TrainingTutor::Alert::None;
    m_enginePanel->setTutorAlert(QString());
    updateBoardBorder();
    scheduleSaveSession();
}

namespace {

const std::pair<TrainingTutor::Alert, QLatin1String> kTutorAlertNames[] = {
    {TrainingTutor::Alert::MissedChance, QLatin1String("missed-chance")},
    {TrainingTutor::Alert::Inaccuracy, QLatin1String("inaccuracy")},
    {TrainingTutor::Alert::Mistake, QLatin1String("mistake")},
    {TrainingTutor::Alert::Blunder, QLatin1String("blunder")},
};

} // namespace

void MainWindow::restoreTutorHold(const Project &project)
{
    if (!project.tutorHold || project.tutorHold->ply != m_session->ply() || m_session->ply() < 1)
        return;
    const Project::TutorHold &hold = *project.tutorHold;
    TrainingTutor::Alert alert = TrainingTutor::Alert::None;
    for (const auto &[value, name] : kTutorAlertNames) {
        if (hold.alert == name)
            alert = value;
    }
    const std::optional<ChessMove> reply = m_session->position().moveFromUci(hold.reply);
    if (!reply || alert == TrainingTutor::Alert::None)
        return;
    // Judged from the position the move was played from: Take Back judges the next try against it too.
    m_trainingBaseline = hold.before;
    m_trainingBaselineFen = m_session->positionAt(m_session->ply() - 1).fen();
    holdEngineReply(*reply, hold.after, alert);
}

void MainWindow::takeBackTutorMove()
{
    if (!m_tutorReply)
        return;
    // Back where the move was played from: the evaluation it was judged
    // against is still that position's, so the next try is judged too.
    clearTutor();
    m_session->goBack();
}

void MainWindow::ignoreTutorAlert()
{
    if (!m_tutorReply)
        return;
    const ChessMove reply = *m_tutorReply;
    const EngineEvaluation evaluation = m_tutorEvaluation;
    clearTutor();
    playEngineReply(reply, evaluation);
}

void MainWindow::recordTrainingResult()
{
    const ChessPosition &position = m_session->position();
    GameRecord game = m_session->game();
    QString message;
    if (position.isStalemate()) {
        game.result = QStringLiteral("1/2-1/2");
        message = tr("Stalemate: the training game is a draw.");
    } else {
        const bool whiteWon = position.sideToMove() == Side::Black;
        game.result = whiteWon ? QStringLiteral("1-0") : QStringLiteral("0-1");
        const bool userWon = (whiteWon ? Side::White : Side::Black) == m_trainingSide;
        message = userWon ? tr("Checkmate: you won the training game.")
                          : tr("Checkmate: the engine won the training game.");
    }
    m_session->setHeader(game);
    saveGameToDatabase(); // Does nothing once the game is in the database.
    statusBar()->showMessage(message, 8000);
}

void MainWindow::playBoardMove(int from, int to, const QPoint &globalPosition)
{
    QList<ChessMove> candidates;
    for (const ChessMove &move : m_session->position().legalMoves()) {
        if (move.from == from && move.to == to)
            candidates << move;
    }
    if (candidates.isEmpty())
        return;

    ChessMove move = candidates.first();
    if (candidates.size() > 1) {
        QMenu menu(this);
        menu.setAccessibleName(tr("Promote to"));
        const std::pair<PieceType, QString> pieces[] = {{PieceType::Queen, tr("Queen")},
                                                        {PieceType::Rook, tr("Rook")},
                                                        {PieceType::Bishop, tr("Bishop")},
                                                        {PieceType::Knight, tr("Knight")}};
        for (const auto &[type, name] : pieces)
            menu.addAction(name)->setData(int(type));
        const QAction *chosen = menu.exec(globalPosition);
        if (!chosen)
            return;
        move.promotion = PieceType(chosen->data().toInt());
    }

    playUserMove(move);
}

void MainWindow::playUserMove(const ChessMove &move)
{
    // A move other than the one the game goes on with, in the middle of a
    // line: the user says whether it is a variation or replaces the rest.
    // Not while playing (training, online): the game goes on as it is.
    // Nor in Lobby Mode: another move of the opponent's is another case of the plan.
    if (m_onlinePlay || m_trainingModeAction->isChecked() || m_lobbyGame || !m_session->wouldBranch(move)) {
        playMove(move);
        return;
    }
    const bool atBranch = !m_session->path().isEmpty() && m_session->ply() == m_session->branchPly();
    const bool mainLine = m_session->path().size() - (atBranch ? 1 : 0) == 0;
    QMessageBox box(QMessageBox::Question, tr("New Move"),
                    tr("%1 is not the move the game goes on with.")
                        .arg(m_session->position().moveNumberText() + m_session->position().san(move)),
                    QMessageBox::NoButton, this);
    QPushButton *variation = box.addButton(tr("Insert as &Variation"), QMessageBox::AcceptRole);
    QPushButton *replace =
        box.addButton(mainLine ? tr("&Replace Main Line") : tr("&Replace Line"), QMessageBox::DestructiveRole);
    replace->setToolTip(tr("The moves after it are deleted"));
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(variation);
    box.exec();
    if (box.clickedButton() == variation) {
        playMove(move);
    } else if (box.clickedButton() == replace) {
        if (!m_session->replaceLine(move))
            return;
        playMoveSound();
        QString error;
        if (!storeOpenGame(&error))
            statusBar()->showMessage(tr("The move could not be saved in the database: %1").arg(error), 8000);
    }
    // Cancel: nothing was played, the board never moved the piece.
}

void MainWindow::playMove(const ChessMove &move)
{
    // Online, the move goes to the platform; the board follows at once and
    // the game's stream confirms it (or takes it back).
    if (m_onlinePlay && m_online && m_online->isPlaying() && !isOpponentTurn()
        && m_session->ply() == m_session->plyCount())
        m_online->move(move.uci());
    // The next move of the line only steps forward; anything else adds to the
    // game — at its end, or as a variation — and a stored game is saved at once.
    const bool adds = !m_session->isNextMove(move);
    const bool animated = m_animateNextBoard; // Heard when it lands, not now.
    // In training a move of the user's own colour is their turn (updateTraining).
    m_trainingMovePlayed = m_trainingModeAction->isChecked() && !isEngineTurn();
    if (!m_session->playMove(move)) {
        m_trainingMovePlayed = false;
        return;
    }
    if (!animated)
        playMoveSound();
    if (!adds)
        return;
    QString error;
    if (!storeOpenGame(&error))
        statusBar()->showMessage(tr("The move could not be saved in the database: %1").arg(error), 8000);
}

void MainWindow::saveGameToDatabase()
{
    if (!m_database || m_openGameIndex >= 0)
        return;
    QString error;
    const qint64 index = m_database->addGame(m_session->game(), &error);
    if (index < 0) {
        QMessageBox::warning(this, tr("Save Game"), tr("Could not save the game: %1").arg(error));
        return;
    }

    m_gameListModel->setDatabase(m_database.get());
    m_openGameIndex = index;
    const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
    if (proxyIndex.isValid()) {
        m_gameView->selectRow(proxyIndex.row());
        m_gameView->scrollTo(proxyIndex);
    }
    m_session->setHeader(m_database->header(index));
    updateGameCount();
    updateGameActions();
    m_databaseTree->scheduleRefresh();
    rebuildPositionIndex();
    scheduleSaveSession();
    statusBar()->showMessage(tr("Game saved to %1").arg(m_database->name()), 3000);
}

void MainWindow::updateGameActions()
{
    m_saveGameAction->setEnabled(m_database && m_openGameIndex < 0 && m_session->plyCount() > 0);
}

void MainWindow::updateGameHeader()
{
    // Games not in the database (new games, pasted positions) are edited in memory.
    const bool editable = m_openGameIndex < 0 || (m_database && m_openGameIndex < m_database->gameCount());
    m_gameHeader->setGame(m_session->game(), editable);
}

void MainWindow::editGameInfo()
{
    const bool inDatabase = m_openGameIndex >= 0;
    if (inDatabase && (!m_database || m_openGameIndex >= m_database->gameCount()))
        return;

    GameInfoDialog dialog(m_session->game(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const GameRecord edited = dialog.game();
    if (!inDatabase) {
        m_session->setHeader(edited);
        m_session->setTags(edited.tags); // The time control is a tag.
        return;
    }
    QString error;
    if (!m_database->updateHeader(m_openGameIndex, edited, &error)) {
        QMessageBox::warning(this, tr("Game Information"),
                             tr("Could not save the game information: %1").arg(error));
        return;
    }
    m_gameListModel->refreshRow(int(m_openGameIndex));
    m_session->setHeader(m_database->header(m_openGameIndex));
    m_session->setTags(edited.tags);
    m_databaseTree->scheduleRefresh(); // A new time control, or one no game has any more.
    m_sourceSync->scheduleWrite();
}

void MainWindow::copyFen()
{
    QGuiApplication::clipboard()->setText(m_session->position().fen());
    statusBar()->showMessage(tr("FEN copied to clipboard"), 3000);
}

void MainWindow::copyText(const QString &text, const QString &message)
{
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage(message, 3000);
}

void MainWindow::pasteFen()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (!ChessPosition::fromFen(text)) {
        statusBar()->showMessage(tr("The clipboard does not contain a valid FEN"), 5000);
        return;
    }
    if (!mayReplaceBoardGame()) // A game not saved anywhere: saved, let go, or kept.
        return;
    GameRecord game;
    game.startFen = text;
    startGame(game);
}

void MainWindow::pasteLine()
{
    QString error;
    const std::optional<Pgn::ParsedLine> line =
        Pgn::parseLine(QGuiApplication::clipboard()->text().trimmed(), QString(), &error);
    if (!line || line->moves.isEmpty()) {
        statusBar()->showMessage(line ? tr("The clipboard does not contain moves")
                                      : tr("The clipboard does not contain a valid line: %1").arg(error),
                                 5000);
        return;
    }
    if (!mayReplaceBoardGame()) // A game not saved anywhere: saved, let go, or kept.
        return;
    GameRecord game;
    game.startFen = line->startFen;
    game.moves = line->moves;
    game.variations = line->variations;
    game.startComment = line->startComment;
    startGame(game);
    m_session->goToEnd();
}

void MainWindow::pasteLineFromCurrentPosition()
{
    if (m_onlinePlay)
        return;
    QString error;
    const std::optional<Pgn::ParsedLine> line =
        Pgn::parseLine(QGuiApplication::clipboard()->text().trimmed(), m_session->position().fen(), &error);
    if (!line || line->moves.isEmpty()) {
        statusBar()->showMessage(line ? tr("The clipboard does not contain moves")
                                      : tr("The moves on the clipboard cannot be played from this position: %1").arg(error),
                                 5000);
        return;
    }
    if (!line->startFen.isEmpty() && line->startFen != m_session->position().fen()) {
        statusBar()->showMessage(tr("The line on the clipboard starts from another position"), 5000);
        return;
    }
    // As if played on the board: steps along moves the game has, adds the others.
    bool added = false;
    for (const MoveRecord &record : line->moves) {
        const std::optional<ChessMove> move = m_session->position().moveFromUci(record.uci, ChessPosition::NullMoves::Allowed);
        if (!move)
            break;
        added = !m_session->isNextMove(*move) || added;
        if (!m_session->playMove(*move))
            break;
    }
    if (!added)
        return;
    if (!storeOpenGame(&error))
        statusBar()->showMessage(tr("The moves could not be saved in the database: %1").arg(error), 8000);
}

void MainWindow::applyDefaultLayout()
{
    applyLayout(WorkspaceLayout{});
}

void MainWindow::showAbout()
{
    AboutDialog dialog(QString::fromLatin1(APP_VERSION), this);
    dialog.exec();
}

void MainWindow::showGuide()
{
    // One window, kept open beside the application: a guide is read while trying.
    if (!m_guideDialog)
        m_guideDialog = new HelpDialog(UiLanguage::effective(), this);
    m_guideDialog->show();
    m_guideDialog->raise();
    m_guideDialog->activateWindow();
}

void MainWindow::manageDrawers()
{
    // Read each time: a sync may have brought another computer's drawers.
    const QString path = PersonalSettings::path();
    DrawersDialog dialog(Drawers::read(path), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString error;
    if (!Drawers::write(path, dialog.drawers(), &error))
        QMessageBox::warning(this, tr("Drawers"), tr("The drawers could not be saved: %1").arg(error));
}

LobbyService *MainWindow::lobbyService()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    if (!m_lobbyService) {
        // The ledger and the plans are this computer's, beside its other state.
        const QDir dir(lobbyDirectory());
        auto *node = new LobbyNode(LobbyIdentity::key(), dir.filePath(QStringLiteral("ledger.jsonl")),
                                   dir.filePath(QStringLiteral("plans.json")), this);
        m_lobbyService = node;
        // On the network as long as the client runs: the opponents' moves come
        // in and the user's plans answer them, with the lobby window closed.
        m_lobbyNetwork = new LobbyNetwork(node, this);
        const QString relays = qEnvironmentVariable("PRAGMA_LOBBY_RELAYS"); // Tests: a relay of their own.
        if (!relays.isEmpty())
            m_lobbyNetwork->setRelays(relays.split(QLatin1Char(','), Qt::SkipEmptyParts));
        m_lobbyNetwork->start();
        updateLobbyNetwork();
        connect(node, &LobbyService::changed, this, &MainWindow::lobbyChanged);
        connect(node, &LobbyService::networkChanged, this, &MainWindow::updateLobbyNetwork);
        // Do not disturb: what happens in the lobby is told by its window only,
        // and the board follows the game it holds; no message pops up.
    }
    if (auto *node = qobject_cast<LobbyNode *>(m_lobbyService))
        node->setName(lobbyName()); // The name goes in the events that seat the user.
#endif
    return m_lobbyService;
}

QString MainWindow::lobbyDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("lobby"));
}

void MainWindow::resetLobbyService()
{
    leaveLobbyGame();
    delete m_lobbyDialog; // It holds the node.
    m_lobbyDialog = nullptr;
#ifdef PRAGMA_HAS_PHONE_LINK
    delete m_lobbyNetwork;
    m_lobbyNetwork = nullptr;
#endif
    delete m_lobbyService;
    m_lobbyService = nullptr;
    lobbyService();
    updateLobbyNetwork();
}

QString MainWindow::lobbyName() const
{
    const QString me = myName();
    return me.isEmpty() ? tr("Me") : me;
}

const LobbyRoom *MainWindow::linkedRoom()
{
    if (!m_lobbyGame || !lobbyService())
        return nullptr;
    const int index = m_lobbyService->lobby().indexOfRoom(m_lobbyGame->room);
    return index < 0 ? nullptr : &m_lobbyService->lobby().rooms().at(index);
}

const LobbyGame *MainWindow::linkedGame()
{
    const LobbyRoom *room = linkedRoom();
    const int index = room ? room->indexOfGame(m_lobbyGame->white, m_lobbyGame->black) : -1;
    return index < 0 ? nullptr : &room->games.at(index);
}

void MainWindow::restoreLobbyLink(const Project &project)
{
    if (project.lobbyRoom.isEmpty() || !lobbyService())
        return;
    // Only if the board holds that game: the project may have moved on.
    const QString uid = LobbyPlans::uid(project.lobbyRoom, project.lobbyWhite, project.lobbyBlack);
    if (m_session->game().uid != uid)
        return;
    const QString me = m_lobbyService->me();
    m_lobbyGame = LobbyLink{project.lobbyRoom, project.lobbyWhite, project.lobbyBlack,
                            project.lobbyWhite == me ? Side::White : Side::Black, uid, 0};
    // The board has the moves the project kept; the ledger may have more.
    const LobbyGame *game = linkedGame();
    int shown = 0;
    while (game && shown < game->moves.size() && shown < m_session->game().moves.size()
           && m_session->game().moves.at(shown).uci == game->moves.at(shown))
        ++shown;
    m_lobbyGame->plies = shown;
    m_lobbyModeAction->setEnabled(true);
    m_lobbyModeAction->setChecked(project.lobbyMode);
    lobbyChanged();
}

void MainWindow::playLobbyGame(const QString &roomId, const QString &white, const QString &black)
{
    // An online game in progress is kept or resigned first, as for any new game.
    if (m_onlinePlay) {
        leaveOnlineThen([this, roomId, white, black] { playLobbyGame(roomId, white, black); });
        return;
    }
    if (!lobbyService())
        return;
    const int roomIndex = m_lobbyService->lobby().indexOfRoom(roomId);
    if (roomIndex < 0)
        return;
    const LobbyRoom &room = m_lobbyService->lobby().rooms().at(roomIndex);
    const int gameIndex = room.indexOfGame(white, black);
    if (gameIndex < 0)
        return;
    // A game not saved anywhere: saved, let go, or the lobby game is not brought.
    if (!mayReplaceBoardGame())
        return;
    const Side side = white == m_lobbyService->me() ? Side::White : Side::Black;
    const GameRecord record = LobbyPlans::record(room, gameIndex);
    const int plies = int(room.games.at(gameIndex).moves.size());
    m_trainingModeAction->setChecked(false); // A person is on the other side, not the engine.
    m_flipBoardAction->setChecked(side == Side::Black); // Played from the bottom.
    m_settingLobbyGame = true;
    startGame(record);
    m_session->goToEnd(); // Where the game stands: the move to play.
    m_settingLobbyGame = false;
    m_lobbyGame = LobbyLink{roomId, white, black, side, record.uid, plies};
    m_lobbyModeAction->setEnabled(true);
    m_lobbyModeAction->setChecked(true);
    updateLobbyPanel();
    m_engineDock->show();
    if (m_lobbyDialog)
        m_lobbyDialog->hide(); // The board is where the move is played; the lobby opens again as it was.
    const QString opponent = side == Side::White ? record.black : record.white;
    statusBar()->showMessage(tr("%1: your game against %2").arg(record.event, opponent), 8000);
}

void MainWindow::leaveLobbyGame()
{
    m_lobbyGame.reset();
    m_lobbyModeAction->setChecked(false);
    m_lobbyModeAction->setEnabled(false);
    updateLobbyPanel();
}

void MainWindow::lobbyChanged()
{
    const LobbyGame *game = linkedGame();
    if (!game || m_settingLobbyGame) {
        updateLobbyPanel();
        return;
    }
    if (game->moves.size() > m_lobbyGame->plies)
        mergeLobbyMoves(*game);
    updateLobbyPanel();
}

void MainWindow::mergeLobbyMoves(const LobbyGame &game)
{
    // The new moves go into the board's own tree — through the moves the
    // user prepared when they are there, as a new variation otherwise —, so
    // nothing they prepared is lost. The board moves to the game's new
    // position only if it stood where the game stood; a user looking
    // elsewhere (preparing a plan, a piece in the hand) stays there.
    const int from = m_lobbyGame->plies;
    const QList<int> path = m_session->path();
    const int ply = m_session->ply();
    bool atGame = ply == from;
    for (int i = 1; atGame && i <= from; ++i)
        atGame = m_session->moveAt(i).uci == game.moves.at(i - 1);

    m_settingLobbyGame = true;
    m_mergingLobbyMoves = true; // The board is drawn once, at the end.
    m_session->goToLine({}, 0);
    bool merged = true;
    for (int i = 0; merged && i < game.moves.size(); ++i) {
        const std::optional<ChessMove> move = m_session->position().moveFromUci(game.moves.at(i));
        merged = move && m_session->playMove(*move); // Steps forward when the tree has it.
    }
    const QList<int> gamePath = m_session->path();
    m_mergingLobbyMoves = false;
    if (!merged) {
        m_settingLobbyGame = false;
        showLobbyGameOnBoard(from); // A tree that cannot hold them: the game's moves alone.
        return;
    }
    m_lobbyGame->plies = int(game.moves.size());
    if (atGame) {
        // At the game's position: on to the new one, the last move sliding in.
        m_session->goToLine(gamePath, int(game.moves.size()) - 1);
        m_animateNextBoard = true;
        m_session->goToLine(gamePath, int(game.moves.size()));
    } else {
        m_session->goToLine(path, ply);
    }
    m_settingLobbyGame = false;
    syncBoard();
}

void MainWindow::updateLobbyPanel()
{
    const LobbyGame *game = m_lobbyModeAction->isChecked() ? linkedGame() : nullptr;
    if (!game) {
        m_lobbyStatus.clear();
        m_lobbyCanSend = {false, false};
        m_enginePanel->setLobby(false);
        return;
    }
    const QString me = m_lobbyService->me();
    const QString opponent = linkedRoom()->displayName(game->opponentOf(me));
    const int official = int(game->moves.size());

    // The line on the board must go through the game's moves.
    bool follows = m_session->plyCount() >= official;
    for (int ply = 1; follows && ply <= official; ++ply)
        follows = m_session->moveAt(ply).uci == game->moves.at(ply - 1);
    const LobbyPlans::Prepared prepared = LobbyPlans::prepared(m_session->game(), official, m_lobbyGame->side);
    const bool myTurn = game->waitsFor(me);
    const bool moveReady = follows && myTurn && m_session->plyCount() > official;

    QString status;
    if (game->isOver()) {
        status = tr("The game is over: %1.").arg(game->result);
    } else if (!follows && m_session->plyCount() >= official) {
        status = tr("The line on the board leaves the game's moves: go back to it to send a move.");
    } else if (myTurn) {
        status = moveReady
            ? tr("Your move: %1 is ready to send.")
                  .arg(m_session->positionAt(official).lineText({m_session->moveAt(official + 1).uci}, 1,
                                                                 SanStyle::Figurines))
            : tr("Your move: play it on the board. You can move %1's pieces too, to prepare your answers.")
                  .arg(opponent);
    } else {
        status = tr("Waiting for %1's move. You can prepare your answers on the board, moving their pieces too.")
                     .arg(opponent);
    }
    // What the board holds is sent already when every answer on it is one the
    // node has: nothing to send until the user's turn, or a change of plan.
    const LobbyPlan sent = m_lobbyService->plan(m_lobbyGame->room, m_lobbyGame->white, m_lobbyGame->black);
    bool planSent = !prepared.plan.isEmpty();
    for (auto answer = prepared.plan.cbegin(); planSent && answer != prepared.plan.cend(); ++answer)
        planSent = sent.value(answer.key()) == answer.value();
    if (!game->isOver() && planSent && !myTurn) {
        status += QLatin1Char(' ') + tr("Plan sent: %n answer(s) ready, played as soon as the game reaches them.",
                                        nullptr, int(prepared.plan.size()));
    } else if (!game->isOver() && !prepared.plan.isEmpty()) {
        status += QLatin1Char(' ') + tr("Your plan: %n answer(s) prepared.", nullptr, int(prepared.plan.size()));
        if (prepared.ignored > 0)
            status += QLatin1Char(' ')
                + tr("%n other move(s) of yours left out: a plan has one answer for each position, the line's.",
                     nullptr, prepared.ignored);
    }
    // On the user's turn both are offered, ready or not: that they light up
    // says the turn came back (a click with nothing ready says what to do).
    // While waiting, only a plan that changed since it was sent.
    const bool canSendPlan = !game->isOver() && (myTurn || (!prepared.plan.isEmpty() && !planSent));
    m_lobbyCanSend = {!game->isOver() && (myTurn || moveReady), canSendPlan};
    // What a click with nothing to send asked for, while the board stays where it was.
    if (!m_lobbyHint.isEmpty() && m_lobbyHintAt == qMakePair(m_session->ply(), m_session->plyCount()))
        status += QLatin1Char(' ') + m_lobbyHint;
    else
        m_lobbyHint.clear();
    m_lobbyStatus = status;
    m_enginePanel->setLobby(true, status, m_lobbyCanSend.first, m_lobbyCanSend.second);
}

void MainWindow::sendLobby(bool plan)
{
    const LobbyGame *game = m_lobbyModeAction->isChecked() ? linkedGame() : nullptr;
    // As the panel offers them: a send with nothing to send does nothing.
    if (!game || game->isOver())
        return;
    const LobbyLink link = *m_lobbyGame;
    const int from = int(game->moves.size());
    const QString first = m_session->plyCount() > from ? m_session->moveAt(from + 1).uci : QString();
    const QString firstText = first.isEmpty()
        ? QString()
        : m_session->positionAt(from).lineText({first}, 1, SanStyle::Letters);
    if (plan) {
        // The plan replaces the one sent before; this computer plays it, the
        // move of now at once, the others as the opponent's replies come.
        const LobbyPlan prepared = LobbyPlans::prepared(m_session->game(), from, link.side).plan;
        if (prepared.isEmpty()) {
            showLobbyHint(tr("Nothing to send yet: play your move on the board, and your answers to your "
                             "opponent's replies after it, then Send Plan."));
            return;
        }
        m_lobbyService->setPlan(link.room, link.white, link.black, prepared);
        statusBar()->showMessage(tr("Plan sent: %n answer(s), played as the game reaches them.", nullptr,
                                    int(prepared.size())),
                                 8000);
    } else {
        bool follows = !first.isEmpty();
        for (int ply = 1; follows && ply <= from; ++ply)
            follows = m_session->moveAt(ply).uci == game->moves.at(ply - 1);
        if (first.isEmpty()) {
            showLobbyHint(tr("Nothing to send yet: play your move on the board first, then Send Move."));
            return;
        }
        if (!follows) {
            showLobbyHint(tr("The move on the board is not after where the game stands: go back to the game's line."));
            return;
        }
        if (!m_lobbyService->sendMove(link.room, link.white, link.black, first))
            return;
        statusBar()->showMessage(tr("Move sent: %1").arg(firstText), 8000);
    }
    lobbyChanged();
}

void MainWindow::showLobbyHint(const QString &hint)
{
    m_lobbyHint = hint;
    m_lobbyHintAt = qMakePair(m_session->ply(), m_session->plyCount());
    updateLobbyPanel();
}

void MainWindow::showLobbyGameOnBoard(int from)
{
    const LobbyRoom *room = linkedRoom();
    const int index = room ? room->indexOfGame(m_lobbyGame->white, m_lobbyGame->black) : -1;
    if (index < 0)
        return;
    const GameRecord record = LobbyPlans::record(*room, index);
    m_lobbyGame->plies = int(record.moves.size());
    // The game's moves are the truth: the board shows them, the last one sliding in.
    GameRecord before = record;
    std::optional<MoveRecord> last;
    if (before.moves.size() > from)
        last = before.moves.takeLast();
    m_settingLobbyGame = true;
    m_session->setGame(before);
    m_session->goToEnd();
    if (last) {
        if (const std::optional<ChessMove> move = m_session->position().moveFromUci(last->uci)) {
            m_animateNextBoard = true;
            m_session->playMove(*move);
        }
    }
    m_settingLobbyGame = false;
    updateLobbyPanel();
}

void MainWindow::showLobby()
{
    if (!lobbyService())
        return;
    // Not modal: the lobby stays open beside the board.
    if (!m_lobbyDialog) {
        m_lobbyDialog = new LobbyDialog(m_lobbyService, this);
        connect(m_lobbyDialog, &LobbyDialog::playRequested, this, &MainWindow::playLobbyGame);
    }
    // Entering the lobby starts from the list of rooms, wherever it was left
    // (Play takes it into a room and away).
    m_lobbyDialog->showLobbyPage();
    m_lobbyDialog->show();
    m_lobbyDialog->raise();
    m_lobbyDialog->activateWindow();
}

void MainWindow::restoreSession()
{
    m_restoringSession = true;
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        resize(1280, 820);
    } else {
        // The saved size is the whole window, frame included (WindowChrome),
        // and the screen may be smaller than the one it was saved on.
        WindowChrome::markFramed(this);
        if (const QScreen *screen = this->screen())
            resize(size().boundedTo(screen->availableGeometry().size()));
    }
    // A maximized state set before the window is mapped does not survive
    // mapping (Qt's xcb plugin reads back the state the window manager has not
    // applied yet): start normal and maximize once shown (showEvent).
    m_restoredWindowState = windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    if (m_restoredWindowState != Qt::WindowNoState)
        setWindowState(windowState() & ~m_restoredWindowState);

    QHeaderView *gameHeader = m_gameView->horizontalHeader();
    if (gameHeader->restoreState(settings.value(QStringLiteral("games/header")).toByteArray()))
        m_gameView->sortByColumn(gameHeader->sortIndicatorSection(), gameHeader->sortIndicatorOrder());
    applyGameColumns(); // Which columns are shown is the database's, not the session's.

    const QString yaml = settings.value(QStringLiteral("session/project")).toString();
    std::optional<Project> project = yaml.isEmpty() ? std::nullopt : Project::fromYaml(yaml, QDir(), nullptr, contentLanguage());
    if (project) {
        applyProject(*project, false);
    } else {
        Project first; // First launch: default layout, first game of the default database.
        applyProject(first, true);
    }

    // Reattach to the project file the session belonged to, if it still exists.
    m_projectPath = settings.value(QStringLiteral("session/projectPath")).toString();
    if (!m_projectPath.isEmpty()) {
        if (std::optional<Project> saved = Project::loadFromFile(m_projectPath, nullptr, contentLanguage()))
            m_savedProjectYaml = saved->toYaml();
        else
            m_projectPath.clear();
    }

    m_restoringSession = false;
    // The development API, when make start asks for it (PRAGMA_DEV_API=1).
    m_api = new DesktopApi(this);
    QString apiError;
    if (!m_api->startIfAsked(&apiError))
        qWarning("Development API: %s", qPrintable(apiError));
    else if (m_api->isListening())
        qInfo("Development API on http://127.0.0.1:%d", m_api->port());
    updateWindowTitle();
    updateProjectModified();
}

void MainWindow::saveSession()
{
    if (m_restoringSession)
        return;
    m_saveTimer->stop();

    QSettings settings;
    // Once closed, the window manager may have dropped the maximized state:
    // keep the geometry saved while the window was still shown.
    if (isVisible())
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("games/header"), m_gameView->horizontalHeader()->saveState());
    settings.setValue(QStringLiteral("session/project"), captureProject().toYaml());
    settings.setValue(QStringLiteral("session/projectPath"), m_projectPath);
    updateProjectModified(); // The panels may have moved: Save Project comes back if they did.
    // Superseded by session/project.
    // "workspaces" and "games/splitter" belong to the project (.pch) now.
    for (const char *key : {"window/state", "board", "games/search", "session/databasePath",
                            "session/gameIndex", "session/startFen", "session/ply",
                            "games/splitter", "workspaces"})
        settings.remove(QString::fromLatin1(key));

    updateProjectModified();
}

Project MainWindow::captureProject()
{
    Project project;
    project.name = m_projectName;
    project.multilingual = m_multilingual;
    if (m_database)
        project.databasePath = m_database->location();
    // The chapters, with the game on the board as it is and where the user is in it.
    project.chapters = m_chapters.chapters;
    project.chapter = m_chapters.current;
    Chapter &open = project.chapters[m_chapters.current];
    open.games[open.currentGame].game = m_session->game();
    open.ply = m_session->ply();
    open.path = m_session->path(); // A move inside a variation is where the project opens again.
    project.noChapters = !m_chapters.hasChapters();
    project.automaticChapters = m_chapters.isAutomatic();
    project.boardFlipped = m_flipBoardAction->isChecked();
    project.showCoordinates = m_coordinatesAction->isChecked();
    project.engineId = m_engineId;
    project.engineName = m_engineName;
    project.engineAnalyzing = m_startEngineAction->isChecked();
    project.training = m_trainingModeAction->isChecked();
    project.trainingSide = m_trainingSide;
    if (project.training && m_tutorReply) {
        Project::TutorHold hold;
        hold.ply = m_tutorPly;
        hold.reply = m_tutorReply->uci();
        for (const auto &[value, name] : kTutorAlertNames) {
            if (value == m_tutorAlert)
                hold.alert = name;
        }
        hold.before = m_trainingBaseline;
        hold.after = m_tutorEvaluation;
        project.tutorHold = hold;
    }
    project.explain = m_explainAction->isChecked();
    if (m_lobbyGame) {
        project.lobbyRoom = m_lobbyGame->room;
        project.lobbyWhite = m_lobbyGame->white;
        project.lobbyBlack = m_lobbyGame->black;
        project.lobbyMode = m_lobbyModeAction->isChecked();
    }
    project.workspace = captureLayout();
    return project;
}

void MainWindow::applyProject(const Project &project, bool openFirstGameIfNone)
{
    const bool wasRestoring = m_restoringSession;
    m_restoringSession = true;
    // Off while the game changes, or the engine would answer in the one being opened.
    m_trainingModeAction->setChecked(false);
    leaveLobbyGame(); // The project says whether a lobby game is on the board.

    if (!project.legacyLayout.isEmpty()) {
        restoreLegacyLayout(project.legacyLayout); // Written before the shares: honoured once, saved as shares.
        m_layout = project.workspace;
        m_layoutPending = false;
    } else {
        applyLayout(project.workspace);
    }
    m_flipBoardAction->setChecked(project.boardFlipped);
    m_coordinatesAction->setChecked(project.showCoordinates);
    m_engineId = m_engines.resolve(project.engineId, project.engineName).id;
    m_engineName = m_engines.resolve(m_engineId).name;
    m_enginePanel->setEngineName(m_engineName);
    updateResourceButtons();

    // A database moved since the project was saved (migrateOpeningNames) is opened where it is now.
    const QString databasePath = m_movedDatabases.value(project.databasePath, project.databasePath);
    // A project that names no database (one we distribute) keeps the one open.
    if (!databasePath.isEmpty() || !m_database)
        openInitialDatabase(databasePath);

    m_projectName = project.name;
    m_multilingual = project.multilingual;
    // A project shows its texts in the interface's language, whatever language it was edited in last.
    m_chapters.language = contentLanguage();
    if (!project.chapters.isEmpty()) {
        m_chapters.setChapters(project.chapters, project.chapter, project.noChapters, project.automaticChapters);
        // Games stored in the database are shown as it has them now; the
        // others as the project kept them, their moves replayed.
        for (Chapter &chapter : m_chapters.chapters) {
            for (ChapterGame &entry : chapter.games) {
                const qint64 index = gameIndexOf(entry.game.uid);
                const std::optional<GameRecord> stored = index >= 0 ? m_database->loadGame(index) : std::nullopt;
                entry.game = GameSession::resolved(stored.value_or(entry.game));
            }
        }
        m_moveView->refresh();
        loadChapterGame();
        returnToPlace();
        m_startEngineAction->setChecked(project.engineAnalyzing);
        if (project.training) {
            m_trainingSide = project.trainingSide;
            restoreTutorHold(project);
            m_trainingModeAction->setChecked(true);
        }
        // Explain comes back on for the move it was explaining (moving to it turned it off).
        m_explainAction->setChecked(project.explain);
        restoreLobbyLink(project);
        m_restoringSession = wasRestoring;
        return;
    }
    // A project without chapters, or from before them: its one game, if it
    // has one, becomes the first chapter.
    m_chapters = ChapterBook();
    m_moveView->refresh();

    bool opened = false;
    // A project without a database path refers to whichever default database was opened.
    const bool sameDatabase = m_database
        && (databasePath.isEmpty() || m_database->location() == databasePath);
    if (sameDatabase && project.gameId >= 0) {
        for (qint64 index = 0; index < m_database->gameCount() && !opened; ++index) {
            if (m_database->header(index).id != project.gameId)
                continue;
            const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
            if (proxyIndex.isValid()) {
                openGame(proxyIndex);
            } else if (std::optional<GameRecord> game = m_database->loadGame(index)) {
                // Not shown in the games list: open it anyway.
                m_gameView->clearSelection();
                m_openGameIndex = index;
                m_session->setGame(*game);
            }
            opened = true;
        }
    }
    const bool unsavedGame = project.startFen.isEmpty() ? !project.moves.isEmpty()
                                                        : ChessPosition::fromFen(project.startFen, ChessPosition::Kings::Optional).has_value();
    if (!opened && project.gameId < 0 && unsavedGame) {
        GameRecord game;
        game.startFen = project.startFen;
        game.result = QStringLiteral("*");
        for (const QString &uci : project.moves)
            game.moves << MoveRecord{QString(), uci, {}}; // SAN is filled in when the game is opened.
        for (const QString &annotation : project.annotations) {
            const int ply = annotation.section(QLatin1Char(':'), 0, 0).toInt();
            if (ply >= 1 && ply <= game.moves.size())
                MoveAnnotation::split(annotation.section(QLatin1Char(':'), 1), &game.moves[ply - 1].nags);
        }
        game.variations = GameVariations::fromText(project.variations);
        m_gameView->clearSelection();
        m_openGameIndex = -1;
        m_session->setGame(game);
        opened = true;
    }
    if (!opened) {
        m_gameView->clearSelection();
        m_openGameIndex = -1;
        if (openFirstGameIfNone && m_gameListProxy->rowCount() > 0)
            openGame(m_gameListProxy->index(0, 0));
        else
            m_session->setGame(GameRecord());
    }
    m_session->goToPly(project.ply);
    m_startEngineAction->setChecked(project.engineAnalyzing);
    // A project closed while training goes on training: the engine answers
    // at once if the move is its own.
    if (project.training) {
        m_trainingSide = project.trainingSide;
        // An unsaved game comes back as its moves only: say again who plays.
        if (m_openGameIndex < 0)
            m_session->setHeader(trainingHeader());
        restoreTutorHold(project);
        m_trainingModeAction->setChecked(true);
    }
    m_explainAction->setChecked(project.explain);
    restoreLobbyLink(project);

    m_restoringSession = wasRestoring;
}

void MainWindow::newProject()
{
    if (!maybeSaveProject())
        return;

    // What is on screen goes on — database, engine, book, panels — so
    // nothing jumps: only the game is new, empty, seen from White's side,
    // and training is off. Reset Panel Layout is there for the default
    // arrangement.
    Project project = captureProject();
    project.chapters.clear(); // No chapter, an empty game.
    project.name = LocalizedText();
    project.multilingual = false;
    project.gameId = -1;
    project.ply = 0;
    project.startFen.clear();
    project.moves.clear();
    project.annotations.clear();
    project.variations.clear();
    project.boardFlipped = false;
    project.training = false;
    project.explain = false;
    project.lobbyRoom.clear();
    applyProject(project, false);

    m_projectPath.clear();
    m_savedProjectYaml.clear();
    updateWindowTitle();
    updateProjectModified();
    scheduleSaveSession();
}

void MainWindow::openProject()
{
    if (!maybeSaveProject())
        return;
    UserFolders::ensureProjectsDir();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), UserFolders::projectsDir(),
        tr("Pragma Chess projects (*.pch);;All files (*)"));
    if (!path.isEmpty())
        openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString &path)
{
    QString error;
    std::optional<Project> project = Project::loadFromFile(path, &error, contentLanguage());
    if (!project) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("Could not open “%1”: %2").arg(QFileInfo(path).fileName(), error));
        return false;
    }

    applyProject(*project, false);
    if (!project->databasePath.isEmpty() && (!m_database || m_database->location() != project->databasePath)) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("The database “%1” used by this project could not be opened.")
                                 .arg(QDir::toNativeSeparators(project->databasePath)));
    }

    m_projectPath = QFileInfo(path).absoluteFilePath();
    // Compare against what was actually applied, so the project doesn't show
    // as modified just because the layout was normalized on restore.
    m_savedProjectYaml = captureProject().toYaml();
    addRecentProject(m_projectPath);
    updateWindowTitle();
    updateProjectModified();
    scheduleSaveSession();
    return true;
}

bool MainWindow::saveProject()
{
    if (m_projectPath.isEmpty())
        return saveProjectAs();

    const Project project = captureProject();
    QString error;
    if (!project.saveToFile(m_projectPath, &error)) {
        QMessageBox::warning(this, tr("Save Project"),
                             tr("Could not save “%1”: %2").arg(QFileInfo(m_projectPath).fileName(), error));
        return false;
    }
    m_savedProjectYaml = project.toYaml();
    addRecentProject(m_projectPath);
    updateProjectModified();
    scheduleSaveSession();
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(m_projectPath)), 3000);
    return true;
}

bool MainWindow::saveProjectAs()
{
    UserFolders::ensureProjectsDir();
    const QString suffix = QLatin1String(Project::fileSuffix);
    const QString name = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"),
        QDir(UserFolders::projectsDir()).filePath(name + QLatin1Char('.') + suffix),
        tr("Pragma Chess projects (*.pch)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + suffix;

    const QString previous = m_projectPath;
    m_projectPath = QFileInfo(path).absoluteFilePath();
    if (!saveProject()) {
        m_projectPath = previous;
        return false;
    }
    updateWindowTitle();
    return true;
}

bool MainWindow::maybeSaveProject()
{
    if (m_projectPath.isEmpty() || !isWindowModified())
        return true;
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Save Project"),
        tr("Save changes to “%1” before closing it?").arg(QFileInfo(m_projectPath).completeBaseName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    return answer == QMessageBox::Discard;
}

void MainWindow::addRecentProject(const QString &path)
{
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("recentProjects")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    settings.setValue(QStringLiteral("recentProjects"), recent);
}

void MainWindow::rebuildRecentProjectsMenu()
{
    m_recentProjectsMenu->clear();
    QSettings settings;
    const QStringList recent = settings.value(QStringLiteral("recentProjects")).toStringList();
    for (const QString &path : recent) {
        if (!QFileInfo::exists(path))
            continue;
        QAction *action = m_recentProjectsMenu->addAction(QFileInfo(path).completeBaseName());
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            if (path != m_projectPath && maybeSaveProject())
                openProjectFile(path);
        });
    }
    if (m_recentProjectsMenu->isEmpty()) {
        m_recentProjectsMenu->addAction(tr("No Recent Projects"))->setEnabled(false);
        return;
    }
    m_recentProjectsMenu->addSeparator();
    m_recentProjectsMenu->addAction(tr("Clear Recent Projects"), this, [] {
        QSettings().remove(QStringLiteral("recentProjects"));
    });
}

void MainWindow::updateWindowTitle()
{
    // Only the project, with the asterisk of unsaved changes, and the
    // application: "Untitled* - Pragma Chess". The database and the game are
    // on show in the window itself. Written in full, with a plain hyphen: the
    // application sets no display name, so Qt adds nothing to any title.
    // The project's own name wins over the file's; once the project has
    // chapters, the one open follows it: "Openings* - The Italian - Pragma
    // Chess", and it goes again when the project is back without chapters.
    const QString fileName = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    const QString name = m_projectName.isEmpty() ? fileName : m_projectName.text(m_chapters.language);
    if (m_chapters.hasChapters())
        setWindowTitle(QStringLiteral("%1[*] - %2 - %3")
                           .arg(name, m_chapters.chapter().title.text(m_chapters.language), QStringLiteral("Pragma Chess")));
    else
        setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, QStringLiteral("Pragma Chess")));
}

void MainWindow::updateProjectModified()
{
    const bool modified = !m_projectPath.isEmpty() && captureProject().toYaml() != m_savedProjectYaml;
    setWindowModified(m_projectPath.isEmpty() || modified); // A project never saved has the asterisk too.
    m_saveProjectAction->setEnabled(m_projectPath.isEmpty() || modified);
}

void MainWindow::scheduleSaveSession()
{
    if (m_saveTimer && !m_restoringSession)
        m_saveTimer->start();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    scheduleSaveSession();
}

void MainWindow::updateSeparatorStyle()
{
    // A light bar along the whole separator, highlighted while hovered. Its
    // ends carry the grey Fusion draws the panels' frames with (window darker
    // 140, lighter 108), so the frames' line runs on across the separator
    // instead of being cut by a bar of another shade.
    const QColor frame = palette().color(QPalette::Window).darker(140).lighter(108);
    const QColor bar = palette().color(QPalette::Window).darker(108);
    const QColor hover = palette().color(QPalette::Highlight);
    const auto rgba = [](const QColor &color) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    };
    const QString style = QStringLiteral("QMainWindow::separator { background: %1; width: 3px; height: 3px; }"
                                         "QMainWindow::separator:vertical { border-top: 1px solid %2; border-bottom: 1px solid %2; }"
                                         "QMainWindow::separator:horizontal { border-left: 1px solid %2; border-right: 1px solid %2; }"
                                         "QMainWindow::separator:hover { background: %3; }")
                              .arg(rgba(bar), rgba(frame), rgba(hover));
    for (QMainWindow *window : {static_cast<QMainWindow *>(this), m_sidebar}) {
        if (window->styleSheet() != style)
            window->setStyleSheet(style);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        updateSeparatorStyle();
    else if (event->type() == QEvent::WindowStateChange)
        scheduleSaveSession();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_layoutPending)
        QTimer::singleShot(0, this, &MainWindow::applyLayoutShares); // Now that the window has a size.
    if (m_restoredWindowState == Qt::WindowNoState)
        return;
    const Qt::WindowStates state = std::exchange(m_restoredWindowState, Qt::WindowNoState);
    // Once the window manager has mapped the window, so it keeps the state.
    QTimer::singleShot(kRestoreWindowStateDelayMs, this, [this, state] {
        if ((windowState() & state) != state)
            setWindowState(windowState() | state);
    });
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    QMainWindow::mouseReleaseEvent(event); // Ends a drag of the Games panel's separator.
    separatorReleased();
}

void MainWindow::separatorReleased()
{
    // Only a drag by the user changes the shares: a panel squeezed by a
    // small window is not measured, or the project would drift with it.
    if (isVisible() && !m_layoutPending)
        captureLayout();
    scheduleSaveSession();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Qt would give the new room to the board and keep the panels as they
    // were: the shares are what the project holds, so they are kept instead.
    m_layoutPending = true;
    QTimer::singleShot(0, this, &MainWindow::applyLayoutShares);
    scheduleSaveSession();
}

void MainWindow::quitWithoutAsking()
{
    m_forcedQuit = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_forcedQuit || m_closingAfterSync) { // Nothing left to ask.
        saveSession();
        event->accept();
        return;
    }

    SyncSettings settings = SyncSettings::load();
    if (!m_projectPath.isEmpty() && isWindowModified()) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Quit Pragma Chess"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("Save changes to “%1” before quitting?")
                        .arg(QFileInfo(m_projectPath).completeBaseName()));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Save);
        // Remembered, so the choice only has to be made once.
        auto *syncFirst = new QCheckBox(tr("Sync before closing"));
        syncFirst->setChecked(settings.syncBeforeClosing);
        box.setCheckBox(syncFirst);

        const int answer = box.exec();
        if (syncFirst->isChecked() != settings.syncBeforeClosing) {
            settings.syncBeforeClosing = syncFirst->isChecked();
            settings.save();
        }
        if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Save && !saveProject()) {
            event->ignore();
            return;
        }
    }

    if (!settings.syncBeforeClosing) {
        saveSession();
        event->accept();
        return;
    }

    // Close once everything is synced, and anyway if the server keeps us waiting.
    event->ignore();
    m_closingAfterSync = true;
    statusBar()->showMessage(tr("Syncing before closing…"));
    QTimer::singleShot(kCloseSyncTimeoutMs, this, [this] {
        if (m_closingAfterSync)
            close();
    });
    syncNow([this] { close(); });
}
