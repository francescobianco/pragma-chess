#include "MainWindow.h"

#include "app/ClassicGames.h"
#include "app/GameSession.h"
#include "app/Project.h"
#include "app/SqliteGameDatabase.h"
#include "app/UserFolders.h"
#include "models/GameListModel.h"
#include "models/MoveListModel.h"
#include "widgets/BoardPanel.h"
#include "widgets/BoardWidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QCloseEvent>
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
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const auto kLayoutsGroup = QStringLiteral("workspaces");

QIcon themeIcon(const char *name, QStyle::StandardPixmap fallback)
{
    return QIcon::fromTheme(QString::fromLatin1(name), qApp->style()->standardIcon(fallback));
}

QWidget *placeholder(const QString &text, QWidget *extra = nullptr)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    auto *label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setEnabled(false);
    layout->addStretch();
    layout->addWidget(label);
    if (extra)
        layout->addWidget(extra, 0, Qt::AlignHCenter);
    layout->addStretch();
    return widget;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_session(new GameSession(this))
    , m_gameListModel(new GameListModel(this))
    , m_gameListProxy(new QSortFilterProxyModel(this))
    , m_moveListModel(new MoveListModel(m_session, this))
    , m_board(new BoardWidget)
{
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);

    m_gameListProxy->setSourceModel(m_gameListModel);
    m_gameListProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_gameListProxy->setFilterKeyColumn(-1);
    m_gameListProxy->setSortRole(Qt::DisplayRole);

    createActions();
    setCentralWidget(new BoardPanel(m_board, {m_firstMoveAction, m_previousMoveAction, m_nextMoveAction,
                                              m_lastMoveAction, m_flipBoardAction}));
    createDocks();
    createMenus();
    createToolBar();
    createStatusBar();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::syncBoard);
    connect(m_board, &BoardWidget::navigateRequested, this,
            [this](int steps) { m_session->goToPly(m_session->ply() + steps); });

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveSession);
    // quit() (e.g. on SIGTERM) does not deliver closeEvent, so save here as well.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    applyWorkspace(Workspace::Database);
    restoreSession();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_searchField, &QLineEdit::textChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_flipBoardAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_coordinatesAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    const QHeaderView *gameHeader = m_gameView->horizontalHeader();
    connect(gameHeader, &QHeaderView::sectionMoved, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sectionResized, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sortIndicatorChanged, this, &MainWindow::scheduleSaveSession);
    for (QDockWidget *dock : findChildren<QDockWidget *>()) {
        connect(dock, &QDockWidget::visibilityChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::topLevelChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::dockLocationChanged, this, &MainWindow::scheduleSaveSession);
    }
}

MainWindow::~MainWindow() = default;

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

    m_quitAction = new QAction(themeIcon("application-exit", QStyle::SP_DialogCloseButton),
                               tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    m_quitAction->setMenuRole(QAction::QuitRole);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_copyFenAction = new QAction(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("&Copy FEN"), this);
    m_copyFenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_copyFenAction, &QAction::triggered, this, &MainWindow::copyFen);

    m_pasteFenAction = new QAction(themeIcon("edit-paste", QStyle::SP_FileIcon), tr("&Paste FEN"), this);
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

    m_coordinatesAction = new QAction(tr("Show &Coordinates"), this);
    m_coordinatesAction->setCheckable(true);
    m_coordinatesAction->setChecked(true);
    connect(m_coordinatesAction, &QAction::toggled, m_board, &BoardWidget::setShowCoordinates);

    m_findAction = new QAction(themeIcon("edit-find", QStyle::SP_FileDialogContentsView),
                               tr("&Find Games…"), this);
    m_findAction->setShortcut(QKeySequence::Find);
    connect(m_findAction, &QAction::triggered, this, [this] {
        m_gamesDock->show();
        m_searchField->setFocus(Qt::ShortcutFocusReason);
        m_searchField->selectAll();
    });

    m_startEngineAction = new QAction(themeIcon("system-run", QStyle::SP_MediaPlay),
                                      tr("&Start Analysis"), this);
    m_startEngineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    m_startEngineAction->setEnabled(false);
    m_startEngineAction->setToolTip(tr("UCI engine support is not available yet"));

    m_aboutAction = new QAction(themeIcon("help-about", QStyle::SP_MessageBoxInformation),
                                tr("&About Pragma Chess"), this);
    m_aboutAction->setMenuRole(QAction::AboutRole);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    m_aboutQtAction = new QAction(tr("About &Qt"), this);
    m_aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(m_aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newProjectAction);
    file->addAction(m_openProjectAction);
    m_recentProjectsMenu = file->addMenu(tr("Open &Recent"));
    connect(m_recentProjectsMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentProjectsMenu);
    file->addSeparator();
    file->addAction(m_saveProjectAction);
    file->addAction(m_saveProjectAsAction);
    file->addSeparator();
    file->addAction(m_quitAction);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_copyFenAction);
    edit->addAction(m_pasteFenAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_flipBoardAction);
    m_viewMenu->addAction(m_coordinatesAction);
    m_viewMenu->addSeparator();
    for (QDockWidget *dock : {m_gamesDock, m_movesDock, m_openingTreeDock, m_engineDock})
        m_viewMenu->addAction(dock->toggleViewAction());
    m_viewMenu->addSeparator();
    m_workspaceMenu = m_viewMenu->addMenu(tr("&Workspace"));
    rebuildWorkspaceMenu();

    QMenu *game = menuBar()->addMenu(tr("&Game"));
    game->addAction(m_firstMoveAction);
    game->addAction(m_previousMoveAction);
    game->addAction(m_nextMoveAction);
    game->addAction(m_lastMoveAction);

    QMenu *position = menuBar()->addMenu(tr("&Position"));
    position->addAction(m_flipBoardAction);
    position->addSeparator();
    position->addAction(m_copyFenAction);
    position->addAction(m_pasteFenAction);

    QMenu *database = menuBar()->addMenu(tr("&Database"));
    database->addAction(m_newDatabaseAction);
    database->addAction(m_openDatabaseAction);
    m_databasesMenu = database->addMenu(themeIcon("folder", QStyle::SP_DirIcon), tr("&Databases"));
    connect(m_databasesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildDatabasesMenu);
    database->addSeparator();
    database->addAction(m_saveDatabaseAction);
    database->addAction(m_saveDatabaseAsAction);
    database->addSeparator();
    database->addAction(m_findAction);

    QMenu *engine = menuBar()->addMenu(tr("E&ngine"));
    engine->addAction(m_startEngineAction);

    menuBar()->addMenu(tr("&Tools"))->setEnabled(false);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_aboutAction);
    help->addAction(m_aboutQtAction);
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->addAction(m_newDatabaseAction);
    toolBar->addAction(m_openDatabaseAction);
    toolBar->addSeparator();
    toolBar->addAction(m_startEngineAction);
}

QDockWidget *MainWindow::addDock(const QString &objectName, const QString &title, QWidget *widget,
                                 Qt::DockWidgetArea area)
{
    auto *dock = new QDockWidget(title, this);
    dock->setObjectName(objectName);
    dock->setWidget(widget);
    addDockWidget(area, dock);
    return dock;
}

void MainWindow::createDocks()
{
    m_moveView = new QTableView;
    m_moveView->setModel(m_moveListModel);
    m_moveView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_moveView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_moveView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_moveView->setShowGrid(false);
    m_moveView->setFocusPolicy(Qt::NoFocus);
    m_moveView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_moveView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(m_moveView, &QTableView::clicked, this, [this](const QModelIndex &index) {
        const int ply = m_moveListModel->plyForIndex(index);
        if (ply > 0)
            m_session->goToPly(ply);
    });
    m_movesDock = addDock(QStringLiteral("movesDock"), tr("Moves"), m_moveView, Qt::RightDockWidgetArea);

    auto *startEngine = new QPushButton(m_startEngineAction->text().remove(QLatin1Char('&')));
    startEngine->setEnabled(false);
    m_engineDock = addDock(QStringLiteral("engineDock"), tr("Engine"),
                           placeholder(tr("No UCI engine configured."), startEngine),
                           Qt::RightDockWidgetArea);

    m_openingTreeDock = addDock(QStringLiteral("openingTreeDock"), tr("Opening Tree"),
                                placeholder(tr("The opening tree is built from the position index "
                                               "of the database engine.")),
                                Qt::RightDockWidgetArea);

    m_gameView = new QTableView;
    m_gameView->setModel(m_gameListProxy);
    m_gameView->setSortingEnabled(true);
    m_gameView->sortByColumn(GameListModel::Number, Qt::AscendingOrder);
    m_gameView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_gameView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_gameView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gameView->setAlternatingRowColors(true);
    m_gameView->setShowGrid(false);
    m_gameView->setWordWrap(false);
    m_gameView->verticalHeader()->hide();
    m_gameView->horizontalHeader()->setStretchLastSection(true);
    m_gameView->horizontalHeader()->setSectionsMovable(true);
    connect(m_gameView, &QTableView::activated, this, &MainWindow::openGame);
    m_gamesDock = addDock(QStringLiteral("gamesDock"), tr("Games"), m_gameView, Qt::BottomDockWidgetArea);
}

void MainWindow::createStatusBar()
{
    m_searchField = new QLineEdit;
    m_searchField->setPlaceholderText(tr("Search games"));
    m_searchField->setClearButtonEnabled(true);
    m_searchField->addAction(themeIcon("edit-find", QStyle::SP_FileDialogContentsView),
                             QLineEdit::LeadingPosition);
    m_searchField->setMinimumWidth(260);
    connect(m_searchField, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_gameListProxy->setFilterFixedString(text);
        updateGameCount();
    });
    statusBar()->addWidget(m_searchField);

    m_gameCountLabel = new QLabel;
    statusBar()->addPermanentWidget(m_gameCountLabel);
}

void MainWindow::setDatabase(std::unique_ptr<GameDatabase> database)
{
    m_gameListModel->setDatabase(nullptr);
    m_database = std::move(database);
    m_gameListModel->setDatabase(m_database.get());
    m_gameView->resizeColumnsToContents();
    m_openGameIndex = -1;

    updateGameCount();
    updateDatabaseActions();
}

void MainWindow::openGame(const QModelIndex &proxyIndex)
{
    const QModelIndex source = m_gameListProxy->mapToSource(proxyIndex);
    if (!m_database || !source.isValid())
        return;
    if (std::optional<GameRecord> game = m_database->loadGame(source.row())) {
        m_gameView->selectRow(proxyIndex.row());
        m_openGameIndex = source.row();
        m_session->setGame(*game);
    }
}

void MainWindow::syncBoard()
{
    m_board->setBoard(m_session->board(), m_session->lastMoveFrom(), m_session->lastMoveTo());

    const QModelIndex current = m_moveListModel->indexForPly(m_session->ply());
    if (current.isValid()) {
        m_moveView->setCurrentIndex(current);
        m_moveView->scrollTo(current);
    } else {
        m_moveView->clearSelection();
        m_moveView->scrollToTop();
    }
    updateNavigationActions();
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
    const qint64 total = m_database->gameCount();
    const qint64 shown = m_gameListProxy->rowCount();
    const QLocale locale;
    const QString count = shown != total
        ? tr("%1 of %2 games").arg(locale.toString(shown), locale.toString(total))
        : total == 1 ? tr("1 game") : tr("%1 games").arg(locale.toString(total));
    m_gameCountLabel->setText(tr("%1 — %2").arg(m_database->name(), count));
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
        m_session->setGame(GameRecord());
        statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(path)), 5000);
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
    if (m_gameListProxy->rowCount() > 0)
        openGame(m_gameListProxy->index(0, 0));
    else
        m_session->setGame(GameRecord());
    return true;
}

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
        QAction *action = m_databasesMenu->addAction(file.completeBaseName());
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

void MainWindow::updateDatabaseActions()
{
    const bool hasDatabase = m_database != nullptr;
    m_saveDatabaseAction->setEnabled(hasDatabase && (m_database->isModified() || m_database->location().isEmpty()));
    m_saveDatabaseAsAction->setEnabled(hasDatabase);
}

void MainWindow::copyFen()
{
    const QString fen = m_session->board().fen();
    if (fen.isEmpty()) {
        statusBar()->showMessage(tr("FEN is only available for positions loaded from FEN "
                                    "until the database engine is connected."), 5000);
        return;
    }
    QGuiApplication::clipboard()->setText(fen);
    statusBar()->showMessage(tr("FEN copied to clipboard"), 3000);
}

void MainWindow::pasteFen()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (!BoardState::fromFen(text)) {
        statusBar()->showMessage(tr("The clipboard does not contain a valid FEN"), 5000);
        return;
    }
    GameRecord game;
    game.startFen = text;
    m_gameView->clearSelection();
    m_openGameIndex = -1;
    m_session->setGame(game);
}

void MainWindow::applyWorkspace(Workspace workspace)
{
    const QList<QDockWidget *> right{m_movesDock, m_openingTreeDock, m_engineDock};
    for (QDockWidget *dock : {m_movesDock, m_gamesDock, m_openingTreeDock, m_engineDock})
        dock->setFloating(false);
    addDockWidget(Qt::BottomDockWidgetArea, m_gamesDock);
    for (QDockWidget *dock : right)
        addDockWidget(Qt::RightDockWidgetArea, dock);

    switch (workspace) {
    case Workspace::Analysis:
        m_gamesDock->hide();
        m_openingTreeDock->hide();
        m_movesDock->show();
        m_engineDock->show();
        resizeDocks({m_movesDock, m_engineDock}, {3, 2}, Qt::Vertical);
        break;
    case Workspace::Database:
        m_openingTreeDock->hide();
        m_gamesDock->show();
        m_movesDock->show();
        m_engineDock->show();
        resizeDocks({m_movesDock, m_engineDock}, {3, 1}, Qt::Vertical);
        resizeDocks({m_gamesDock}, {220}, Qt::Vertical);
        break;
    case Workspace::OpeningPreparation:
        m_engineDock->hide();
        m_gamesDock->show();
        m_movesDock->show();
        m_openingTreeDock->show();
        resizeDocks({m_openingTreeDock, m_movesDock}, {3, 2}, Qt::Vertical);
        resizeDocks({m_gamesDock}, {220}, Qt::Vertical);
        break;
    }
    resizeDocks({m_movesDock}, {280}, Qt::Horizontal);
}

void MainWindow::saveWorkspaceAs()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save Workspace"), tr("Workspace name:"),
                                               QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    QSettings settings;
    settings.beginGroup(kLayoutsGroup);
    // Group keys can't contain slashes; keep the display name as the value's key.
    settings.setValue(QString(name).replace(QLatin1Char('/'), QLatin1Char('_')), saveState());
    settings.endGroup();
    rebuildWorkspaceMenu();
}

void MainWindow::rebuildWorkspaceMenu()
{
    m_workspaceMenu->clear();
    m_workspaceMenu->addAction(tr("&Analysis"), this, [this] { applyWorkspace(Workspace::Analysis); });
    m_workspaceMenu->addAction(tr("&Database"), this, [this] { applyWorkspace(Workspace::Database); });
    m_workspaceMenu->addAction(tr("&Opening Preparation"), this,
                               [this] { applyWorkspace(Workspace::OpeningPreparation); });

    QSettings settings;
    settings.beginGroup(kLayoutsGroup);
    const QStringList saved = settings.childKeys();
    settings.endGroup();
    if (!saved.isEmpty()) {
        m_workspaceMenu->addSeparator();
        for (const QString &name : saved) {
            m_workspaceMenu->addAction(name, this, [this, name] {
                QSettings s;
                restoreState(s.value(kLayoutsGroup + QLatin1Char('/') + name).toByteArray());
            });
        }
    }
    m_workspaceMenu->addSeparator();
    m_workspaceMenu->addAction(tr("&Save Current Workspace…"), this, &MainWindow::saveWorkspaceAs);
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Pragma Chess"),
        tr("<h3>Pragma Chess %1</h3>"
           "<p>An open source chess database engine with a native desktop client.</p>"
           "<p>Licensed under the MIT License.</p>").arg(QString::fromLatin1(APP_VERSION)));
}

void MainWindow::restoreSession()
{
    m_restoringSession = true;
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(1280, 820);

    QHeaderView *gameHeader = m_gameView->horizontalHeader();
    if (gameHeader->restoreState(settings.value(QStringLiteral("games/header")).toByteArray()))
        m_gameView->sortByColumn(gameHeader->sortIndicatorSection(), gameHeader->sortIndicatorOrder());

    const QString yaml = settings.value(QStringLiteral("session/project")).toString();
    std::optional<Project> project = yaml.isEmpty() ? std::nullopt : Project::fromYaml(yaml, QDir(), nullptr);
    if (project) {
        applyProject(*project, false);
    } else {
        Project first; // First launch: default layout, first game of the default database.
        first.layout = saveState();
        applyProject(first, true);
    }

    // Reattach to the project file the session belonged to, if it still exists.
    m_projectPath = settings.value(QStringLiteral("session/projectPath")).toString();
    if (!m_projectPath.isEmpty()) {
        if (std::optional<Project> saved = Project::loadFromFile(m_projectPath, nullptr))
            m_savedProjectYaml = saved->toYaml();
        else
            m_projectPath.clear();
    }

    m_restoringSession = false;
    updateWindowTitle();
    updateProjectModified();
}

void MainWindow::saveSession()
{
    if (m_restoringSession)
        return;
    m_saveTimer->stop();

    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("games/header"), m_gameView->horizontalHeader()->saveState());
    settings.setValue(QStringLiteral("session/project"), captureProject().toYaml());
    settings.setValue(QStringLiteral("session/projectPath"), m_projectPath);
    // Superseded by session/project.
    for (const char *key : {"window/state", "board", "games/search", "session/databasePath",
                            "session/gameIndex", "session/startFen", "session/ply"})
        settings.remove(QString::fromLatin1(key));

    updateProjectModified();
}

Project MainWindow::captureProject() const
{
    Project project;
    if (m_database) {
        project.databasePath = m_database->location();
        if (m_openGameIndex >= 0 && m_openGameIndex < m_database->gameCount())
            project.gameId = m_database->header(m_openGameIndex).id;
    }
    project.ply = m_session->ply();
    if (project.gameId < 0)
        project.startFen = m_session->game().startFen;
    project.boardFlipped = m_flipBoardAction->isChecked();
    project.showCoordinates = m_coordinatesAction->isChecked();
    project.gameSearch = m_searchField->text();
    project.engineName = m_engineName;
    project.engineAnalyzing = m_engineAnalyzing;
    project.layout = saveState();
    return project;
}

void MainWindow::applyProject(const Project &project, bool openFirstGameIfNone)
{
    const bool wasRestoring = m_restoringSession;
    m_restoringSession = true;

    if (!project.layout.isEmpty())
        restoreState(project.layout);
    m_flipBoardAction->setChecked(project.boardFlipped);
    m_coordinatesAction->setChecked(project.showCoordinates);
    m_searchField->setText(project.gameSearch);
    m_engineName = project.engineName;
    m_engineAnalyzing = project.engineAnalyzing;

    openInitialDatabase(project.databasePath);

    bool opened = false;
    if (m_database && project.gameId >= 0 && m_database->location() == project.databasePath) {
        for (qint64 index = 0; index < m_database->gameCount() && !opened; ++index) {
            if (m_database->header(index).id != project.gameId)
                continue;
            const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
            if (proxyIndex.isValid()) {
                openGame(proxyIndex);
            } else if (std::optional<GameRecord> game = m_database->loadGame(index)) {
                // Hidden by the search filter: open it anyway.
                m_gameView->clearSelection();
                m_openGameIndex = index;
                m_session->setGame(*game);
            }
            opened = true;
        }
    }
    if (!opened && project.gameId < 0 && BoardState::fromFen(project.startFen)) {
        GameRecord game;
        game.startFen = project.startFen;
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

    m_restoringSession = wasRestoring;
}

void MainWindow::newProject()
{
    if (!maybeSaveProject())
        return;

    Project project; // Same database, starting position, default workspace.
    if (m_database)
        project.databasePath = m_database->location();
    applyWorkspace(Workspace::Database);
    project.layout = saveState();
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
    std::optional<Project> project = Project::loadFromFile(path, &error);
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
    const GameRecord &game = m_session->game();
    const QString gameTitle = game.white.isEmpty() && game.black.isEmpty()
        ? tr("Position")
        : tr("%1 – %2").arg(game.white, game.black);
    if (m_projectPath.isEmpty())
        setWindowTitle(gameTitle);
    else
        setWindowTitle(tr("%1[*] — %2").arg(QFileInfo(m_projectPath).completeBaseName(), gameTitle));
}

void MainWindow::updateProjectModified()
{
    const bool modified = !m_projectPath.isEmpty() && captureProject().toYaml() != m_savedProjectYaml;
    setWindowModified(modified);
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    scheduleSaveSession();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    event->accept();
}
