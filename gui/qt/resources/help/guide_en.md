# Getting started {#start}

Pragma Chess keeps your games in **databases** and lets you study them on the board with an engine, an opening book and your own notes.

The window has the **board** in the middle and four panels around it:

- **Games**, at the bottom: the tree of the open database and the list of its games.
- **Moves**: the moves of the game on the board.
- **Opening Tree**: the moves of the opening book for the position on the board.
- **Engine**: the evaluation, the best line and the opening the game is in.

The **toolbar** has Sync Now, New Game, New Training and three icons to choose the opening book, the engine and the database in use.

Everything you see — database, game, move, panels — is a **project**: it comes back as you left it the next time you start Pragma Chess.

Your files live in the Pragma folder inside your chess folder (for example `Chess/Pragma` in your home), with `Databases`, `Projects` and `Books`. The first launch puts a database of classic games there.

# Databases {#databases}

A database is a `.pdb` file holding games. One database is open at a time; its name is in the tooltip of the database icon of the toolbar.

- **Database ▸ New Database…** creates an empty one in the Databases folder.
- **Database ▸ Open Database…** opens a file from anywhere.
- **Database ▸ Databases** lists the databases of the folder: choose one to open it. The database icon of the toolbar drops down the same list.
- **Database ▸ Database Settings…** edits the name and the description, says whether the database is a collection of games or an opening book, and has **Optimize Database**.
- **Database ▸ Save Database As…** writes a copy.
- **Database ▸ Databases ▸ Show Databases Folder** opens the folder in the file manager.

Changes to a database are written as you make them: there is nothing to save by hand.

# The games list {#games-list}

The list shows the games of the open database, one per row. Double-click a game to put it on the board.

- Click a column title to **sort** by it; click again to reverse.
- Drag a column title to **move** the column.
- Right-click a column title to **hide** that column, or to **show** a hidden one. Each database remembers its own columns.
- The last column, **Line**, shows how the game begins; it is cut with “…” where the column ends.

Right-click a player to say **who it is**: you, a friend or an opponent. The tree then lists those players under Me, Friends and Opponents, and a game where you play opens with the board turned to your side.

Right-click a game to move it to the **trash**.

To change players, event, date or result of the game on the board, click the header above the board.

# The database tree {#database-tree}

Left of the games list, the tree shows what the open database contains. Select a node and the list shows only those games; select the database itself to see them all.

- **Board ▸ Position**: the games in which the position on the board occurs, whatever the order of the moves.
- **Board ▸ Variant**: the games that begin with exactly the moves played on the board.
- **Me**, **Friends**, **Opponents**: the players you named with “Who Is This?”.
- **ECO**: the games by opening code.
- **Tournaments** and **Years**.
- **Sources**: the games that came from lichess.org, chess.com or torneionline.com.
- **Trash**: the games you threw away.

Position and Variant follow the board: move through a game and their counts change.

# The board {#board}

Move a piece by dragging it, or by clicking it and then its square. A pawn reaching the last rank asks what to promote to.

- **Left** and **Right** go one move back and forward, **Home** and **End** to the start and the end. Clicking a move in the Moves panel goes there.
- **View ▸ Flip Board** (Ctrl+R) turns the board around; **View ▸ Show Coordinates** shows or hides the letters and numbers.
- **Options ▸ Board Settings…** chooses where the captured pieces are shown and whether to show whose turn it is.

**Game ▸ New Game** (Ctrl+Shift+N) starts a game to enter move by move. **Game ▸ Save Game to Database** stores it in the open database.

Play a move that is not the next one of the game and it becomes a **variation**: the game keeps its line, and the new one appears in the Moves panel under the move it replaces. A game stored in the database is saved at once, variations included.

**Edit ▸ Copy** puts on the clipboard the moves, the game as PGN, the position as FEN (Ctrl+Shift+C), the engine line or the explanation. **Edit ▸ Paste FEN** (Ctrl+Shift+V) sets up the position on the clipboard.

# Moves and annotations {#moves}

The Moves panel lists the game on the board, with the pieces drawn as figurines. Click a move to go there.

The main line is a table, one move a cell: click the cell to go to the move. **Variations** take a row of their own under the move they replace, smaller, as text; a variation's own variations follow it in parentheses. Click a move of a variation to follow that line: the arrows then move along it, and clicking a move of the main line brings you back. At the start of a variation, playing the main line's move takes the main line again, and another move starts a sister variation. Variations are saved with the game and travel in the PGN you copy.

Right-click a move for its menu:

- **Copy ▸ Copy Move** and **Copy ▸ Copy Line up to Here** put the move, or the game up to it, on the clipboard.
- **Annotations** lists the symbols with what each one means.

A move can carry one judgement of the move and one assessment of the position:

- `!!` brilliant move, `!` good move, `!?` interesting move, `?!` dubious move, `?` mistake, `??` blunder, `□` only move.
- `+−` White is winning, `±` White is better, `⩲` White is slightly better, `=` equal, `∞` unclear, `⩱` `∓` `−+` the same for Black.

Choose the symbol a move already has to take it off, or **No Annotation** to clear them all. Annotations are saved with the game at once and are written when you copy the game as PGN.

# Explain {#explain}

**Explain** shows on the board why the last move is good or bad. Press the button between the arrows under the board, or the **E** key.

The engine looks at the position before and after the move and draws arrows:

- **red** arrows: material is about to fall, and the pieces that are lost are ringed;
- **blue** arrows: the reply that makes the difference, when nothing is lost yet.

A forced mate is played out on the board, inside a red frame. The Engine panel says it in words, for example “Blunder (+0.3 → −2.9). Black wins a knight”.

While the engine is searching, the border of the board breathes; it turns blue when the explanation is there. Explain is about one move: going to another move turns it off, and you ask again.

**Edit ▸ Copy ▸ Explanation** copies the text.

# Engines {#engine}

**Engine ▸ Analyze** (Ctrl+E) starts and stops the analysis of the position on the board. The Engine panel shows the score — always from White's side: positive is good for White —, the depth and the best line. The bar beside the board shows the same score.

Pragma Chess comes with Stockfish, and works with any UCI engine.

- **Engine ▸ Use Engine** chooses among the engines of this computer. The engine icon of the toolbar drops down the same list.
- **Engine ▸ Manage Engines…** adds, edits and removes engines. **Detect Engines** finds the ones installed on the computer; **Use This Engine** switches to the selected one.

The engine in use is part of the project.

# Training {#training}

**Game ▸ New Training…** (Ctrl+Shift+T) starts a game against the engine. Choose White, Black or Random and press Start.

While it is your move the engine hides its best line and shows only the score. When you have moved, it answers by itself, slowly, so that you see its move.

The **tutor** watches your moves. When one is an **inaccuracy**, a **mistake**, a **blunder** or a **missed chance**, the engine does not answer; the border of the board turns red, the Engine panel says what happened and offers:

- **Take Back**: return to the position and try another move;
- **Explain**: show on the board why it is an error;
- **Ignore**: keep the move, and the engine answers.

Checkmate or stalemate ends the game, which is saved in the open database.

Tick **Remember for this session** in the New Training window and the toolbar button will start the next trainings with the same choice, without asking. The menu always asks.

**Engine ▸ Training Mode** turns training on or off for the game on the board. If you close Pragma Chess while training, it starts again in training.

# Opening books {#books}

An opening book is a Polyglot `.bin` file: the moves known in each position, each with a weight. The **Book** menu lists the books of your Books folder; the book icon of the toolbar drops down the same list.

- **Book ▸ New Book…** and **Book ▸ Open Book…** create a book or open one from anywhere.
- **Book ▸ No Book** works without one.

The **Opening Tree** panel shows, for the position on the board, each move of the book with its share of the weight, the name of the opening it leads to and how the games of the open database went after it (games, then White wins / draws / Black wins). Click a move to play it; the first row takes the last move back.

**Your repertoire**: right-click a move of the Opening Tree and choose **Add to Repertoire**. Repertoire moves are listed first, in bold. The mark is stored in the book file and other programs ignore it.

**Weights**: the same menu has **Adjust Weight**, with +5%, +10%, +25%, +50%, +100%, the same downwards and **Zero Weight**. The percentage is of the move's own share, and the sum of the position always stays 100%: what a move gains the other moves give up in proportion to what they have — the heavy ones most — and what it loses goes back to them the same way. A move at 0%, or with less than 1%, cannot grow by a percentage of itself, so an increase first takes 1% from the others and grows from there; a decrease of a move at 0% does nothing. Zero Weight gives the move's whole share to the other moves that have some. The changes are written into the book in use. The list is sorted again, and the move you changed glows for a moment and is marked, left of its weight, with ↑ if it went up, ↓ if it went down, = if it stayed; the mark lasts until you leave the position.

# Opening names {#opening-names}

The Engine panel names the opening the game is in, and the Opening Tree names where each move leads.

The names come from a database of named lines. **Options ▸ Opening Names** chooses which: English, Italian or none. Until you choose, the names follow the language of the interface.

A names database is an ordinary database of type Opening Book: you can open it and add your own lines, giving each the name in the Event field and the code in ECO.

# Game sources {#sources}

A source brings your games from a website into the open database and keeps them up to date.

**Database ▸ Connect Source…** adds one:

- **lichess.org**: sign in with your account;
- **chess.com**: your user name;
- **torneionline.com**: your FIDE or FSI number, for the games of the tournaments you played.
- **ChessBase files**: a ChessBase database (`.cbh` and its files) on this computer. Choose the `.cbh` file: its games are copied in, the files stay where they are, and games added to them later arrive at the next sync. On another computer the file is not there: the sync says so and offers to ignore the source on that computer; *Database ▸ Manage Sources… ▸ Edit…* chooses the file again.

Sources are read when the database is opened and every twenty minutes. A game is never imported twice. **Database ▸ Manage Sources…** syncs a source now, changes it, signs in again or removes it; the games already imported stay.

The **Sources** node of the tree lists the games of each source.

# Trash {#trash}

Right-click a game of the list and choose **Move Game to Trash**. The game leaves every list and is no longer searched.

The **Trash** node, last in the tree, shows the trashed games: **Recent**, thrown away in the last seven days, and **Old**. There you can **Restore Game** or **Delete Game…**.

Deleting does not shrink the file yet. **Database ▸ Database Settings… ▸ Optimize Database** removes the deleted games for good and compacts the file. Until then nothing is lost.

# Projects {#projects}

A project is what you are looking at: the database, the game and the move, the side the board is seen from, the engine, the panels and whether you are training. The title bar shows its name, with an asterisk when it has changes not saved.

- **File ▸ New Project** starts from the default layout.
- **File ▸ Open Project…** and **File ▸ Open Recent** open a `.pch` file.
- **File ▸ Save Project** and **File ▸ Save Project As…** save it.

You do not have to save: Pragma Chess reopens as you closed it.

Panels can be dragged, resized and closed; the **View** menu shows them again, and **View ▸ Reset Panel Layout** puts them back where they start.

# Sync {#sync}

Sync keeps your Pragma folder — databases and projects — the same on several computers, through a folder on a server.

**File ▸ Sync…** sets it up: an **FTP** server (also with TLS), a **WebDAV** server or a **Git repository**. **Test Connection** checks it.

**Sync Now** (Ctrl+Y, first button of the toolbar) does everything in order: reads the sources, saves the project and exchanges the files with the server. **Sync before closing** does it every time you quit.

Sync never deletes: a database missing on one side is copied there, and a database changed on two computers is merged game by game. To get rid of a game use the trash, which the other computers follow.

**Options ▸ Connect Mobile App…** shows a code to scan with Pragma Chess on your phone, which then keeps a copy of your databases.

# Language {#language}

**Options ▸ Language** chooses the language of the interface. It is applied the next time Pragma Chess starts. This guide and the opening names follow it.

# Keyboard shortcuts {#shortcuts}

**Moving through a game**

- Left, Right — previous and next move
- Home, End — first and last move
- E — Explain
- Ctrl+E — start or stop the analysis
- Ctrl+R — flip the board

**Games**

- Ctrl+Shift+N — new game
- Ctrl+Shift+T — new training

**Clipboard**

- Ctrl+Alt+C — copy the moves up to the current position
- Ctrl+Shift+C — copy the position as FEN
- Ctrl+Shift+V — paste a FEN

**Projects and sync**

- Ctrl+N, Ctrl+O, Ctrl+S — new, open and save project
- Ctrl+Y — sync now
- F1 — this guide
