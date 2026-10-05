# Getting started {#start}

Pragma Chess keeps your games in **databases** and lets you study them on the board with an engine, an opening book and your own notes.

The window has the **board** in the middle and four panels around it:

- **Games**, at the bottom: the tree of the open database and the list of its games.
- **Moves**: the moves of the game on the board.
- **Opening Tree**: the moves of the opening book for the position on the board.
- **Engine**: the evaluation, the best line and the opening the game is in.

The **toolbar** has Sync Now, Save Project, New Game, New Training, New Online Game and three icons to choose the opening book, the engine and the database in use.

Everything you see — database, game, move, panels — is a **project**: it comes back as you left it the next time you start Pragma Chess.

Your files live in the Pragma folder inside your chess folder (for example `Chess/Pragma` in your home), with `Databases`, `Projects` and `Books`. The first launch puts a database of classic games there.

**Options ▸ Personal Settings…** says who you are: your name, your year of birth and your FIDE ID. Your name goes on your side of a new game, a game from Set Up Position (on the side at the bottom of the board) and a training game (on the colour you chose), unless the open database already knows you — a player marked as Me with **Who Is This?** wins. **Board style** chooses the colours of the squares and the pieces together: Pragma Classic (brown squares and the pieces of chess books) or Lichess Alpha (the green board and the Alpha pieces of lichess.org); every board changes as soon as you press OK. These settings are kept in `.pragma-chess.conf`, a YAML file in your Pragma folder, which Sync carries to your other computers although it is hidden; changed on two computers at once, the newer one wins.

**Options ▸ Folder Settings…** puts them somewhere else on this computer: the Pragma folder itself, or just the databases, the projects, the books or the opening names. Leave a folder empty to keep it in its usual place. The files already there are not moved, and the new folders are used the next time you start Pragma Chess. Sync keeps only what is inside the Pragma folder the same on your other computers.

# Databases {#databases}

A database is a `.pdb` file holding games. One database is open at a time; its name is in the tooltip of the database icon of the toolbar.

- **Database ▸ New Database…** creates an empty one in the Databases folder.
- **Database ▸ Open Database…** opens a file from anywhere.
- **Database ▸ Switch Database** lists the databases of the folder: choose one to open it. The database icon of the toolbar drops down the same list.
- **Database ▸ Database Settings…** edits the name and the description, says whether the database is a collection of games or an opening book, and has **Optimize Database**.
- **Database ▸ Save Database As…** writes a copy.
- **Database ▸ Switch Database ▸ Show Databases Folder** opens the folder in the file manager.

Changes to a database are written as you make them: there is nothing to save by hand.

# The games list {#games-list}

The list shows the games of the open database, one per row. Double-click a game to put it on the board.

- Click a column title to **sort** by it; click again to reverse.
- Drag a column title to **move** the column.
- Right-click a column title to **hide** that column, or to **show** a hidden one. Each database remembers its own columns.
- The last column, **Line**, shows how the game begins; it is cut with “…” where the column ends.

Right-click a player to say **who it is**: you, a friend or an opponent. The tree then lists those players under Me, Friends and Opponents, your name is in bold in the list, and a game where you play opens with the board turned to your side.

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
- **Options ▸ Graphics Settings…** chooses the **Appearance** — Follow the System (the default), or always Light or always Dark, whatever the system says —, where the captured pieces are shown, whether to show whose turn it is, and whether a move is heard: **Sound when a piece is moved** plays the knock of a piece set down on the board for your moves, the engine's in training and your opponent's online (the engine's and the opponent's when they land). These settings stay on this computer: each one can match its own desktop.
- When the position on the board is **checkmate**, the border of the board turns red.

**Game ▸ New Game** (Ctrl+Shift+N) starts a game to enter move by move. **Game ▸ Save Game to Database** stores it in the open database; **Game ▸ Save Game to Another Database…** stores it in a database you choose, which is not opened.

Play a move that is not the next one of the game and it becomes a **variation**: the game keeps its line, and the new one appears in the Moves panel under the move it replaces. A game stored in the database is saved at once, variations included.

**Game ▸ Set Up Position…** opens a board to draw a position on: choose a piece on the right and click the squares to put it down (a click on the same piece takes it off), drag a piece to move it anywhere on the board (let go off the board, it goes back), right-click a square to empty it. Set the side to move, castling, en passant and the move number, or type a FEN; **Starting Position**, **Clear Board** and **Flip Board** help. OK is enabled only for a position a game can start from, and the text below the board says what is wrong. The position starts a new game at the end of the chapter, so the game you were on stays as it is. An online game in progress is kept or resigned first.

**Edit ▸ Copy** puts on the clipboard the moves, the game as PGN, the position as FEN (Ctrl+Shift+C), the engine line or the explanation. **Edit ▸ Paste** takes what is on the clipboard: **Paste FEN** (Ctrl+Shift+V) sets up the position, **Paste Line** starts a new game with the moves (PGN, moves with or without numbers, or UCI), and **Paste Line from Current Position** (Ctrl+Alt+V) plays the moves from the position on the board, as if you played them: at the end of the game they are added, elsewhere they become a variation.

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

**Comments** — the text between the moves of a PGN game or of a lichess study — are shown in italics: under their move in the main line, between the moves in a variation. The commands programs put in comments (an evaluation `[%eval 0.18]`, a clock, arrows) are kept with the game but not shown. Comments are saved in the database with the game and written back when the game is written as PGN.

# Explain {#explain}

**Explain** shows on the board why the last move is good or bad. Press the button between the arrows under the board, or the **E** key.

The engine looks at the position before and after the move and draws arrows:

- **red** arrows: material is about to fall, and the pieces that are lost are ringed;
- **blue** arrows: the reply that makes the difference, when nothing is lost yet.

A forced mate is played out on the board, inside a red frame. The Engine panel says it in words, for example “Blunder (+0.3 → −2.9). Black wins a knight”.

While the engine is searching, the border of the board breathes; it turns blue when the explanation is there. Explain is about one move: going to another move turns it off, and you ask again.

**Edit ▸ Copy ▸ Explanation** copies the text.

# Engines {#engine}

**Engine ▸ Analysis** (Ctrl+E) turns on and off the analysis of the position on the board. The Engine panel shows the score — always from White's side: positive is good for White —, the depth and the best line. The bar beside the board shows the same score.

Pragma Chess comes with Stockfish, and works with any UCI engine.

- **Engine ▸ Switch Engine** chooses among the engines of this computer. The engine icon of the toolbar drops down the same list.
- **Engine ▸ Manage Engines…** adds, edits and removes engines. **Detect Engines** finds the ones installed on the computer; **Use This Engine** switches to the selected one.

The engine in use is part of the project.

# Training {#training}

**Game ▸ New Training…** (Ctrl+Shift+T) starts a game against the engine. Choose White, Black or Random and press Start.

While it is your move the engine hides its best line and shows only the score. When you have moved, it answers by itself, slowly, so that you see its move.

As long as the position is in the **opening book**, the engine answers with a book move, choosing each as often as its weight says: adjust the weights in the Opening Tree to train against the lines you want, as often as you want. Out of the book it plays its own best move.

The **tutor** watches your moves. When one is an **inaccuracy**, a **mistake**, a **blunder** or a **missed chance**, the engine does not answer; the border of the board turns red, the Engine panel says what happened and offers:

- **Take Back**: return to the position and try another move;
- **Explain**: show on the board why it is an error;
- **Ignore**: keep the move, and the engine answers.

Checkmate or stalemate ends the game, which is saved in the open database.

Tick **Remember for this session** in the New Training window and the toolbar button will start the next trainings with the same choice, without asking. The menu always asks.

**Engine ▸ Training Mode** turns training on or off for the game on the board. Opening a game from the games list turns it off. If you close Pragma Chess while training, it starts again in training.

# Playing online {#online}

**Game ▸ New Online Game…** plays a game against a person on lichess.org (more platforms will follow).

The window lists the **platforms you are connected to**, each with the account you play as. **Connect Platform…** asks which kind of platform, opens its own sign-in page in your browser and brings the connection here; **Disconnect** removes one. Connections are yours on this computer, kept with your settings, never in a project. Choose the connection, the **clock** (minutes and increment), the colour and whether the game is **rated**, then **Find an Opponent**.

While Pragma Chess looks for an opponent and while you play, it is **online play mode**, checked in **Engine ▸ Online Play Mode** (choosing it starts or stops playing online): the engine, Explain and Training Mode are off and cannot be turned on, and the Opening Tree stays where you put it but lists no moves, its first row saying why — it is you against your opponent. The Engine panel shows the names, the ratings, the clocks and whose move it is. Your moves go to the platform as you make them; your opponent's slide onto the board. Only the live position can be played: you may look back at earlier moves, and come back to the end to move. If Pragma Chess is closed during a game, it reconnects to it at the next start and the game goes on where it is (the clock kept running on the platform); a game that ended meanwhile is saved with its result. You can switch database while you play: the game stays on the board and, when it ends, is saved to the database open then.

Tick **Remember for this session** and the toolbar's New Online Game button looks for an opponent with the same choices without asking; the menu always asks.

**Game ▸ New Online Game…** during a game starts a new online game, **Game ▸ New Game** asks whether you want a new online game (as New Online Game in the toolbar) or a new game to analyse — with **Remember for this session** it does not ask again until Pragma Chess is closed —, and **Game ▸ New Training…** a game against the engine; each asks first whether to **keep playing** the current game or **resign** it (while still looking for an opponent, the search just stops). Choosing **Engine ▸ Online Play Mode** while it is on stops the search, or resigns the game after asking. When the game ends — checkmate, resignation, time, draw — the result is written and the game is saved in the open database, with the players, their ratings and a link to the game.

# Opening books {#books}

An opening book is a Polyglot `.bin` file: the moves known in each position, each with a weight. The book icon of the toolbar drops down the books of your Books folder.

- **Book ▸ New Book…** and **Book ▸ Open Book…** create a book or open one from anywhere.
- **Book ▸ Switch Book** lists the books of the folder: choose one to use it, or **No Book** to work without one. **Show Books Folder**, at its end, opens the folder in the file manager.

The **Opening Tree** panel shows, for the position on the board, each move of the book with its share of the weight, the name of the opening it leads to and how the games of the open database went after it (games, then White wins / draws / Black wins). Click a move to play it; the first row takes the last move back.

**Your repertoire**: right-click a move of the Opening Tree and choose **Add to Repertoire**. Repertoire moves are listed first, in bold. The mark is stored in the book file and other programs ignore it.

**Weights**: the same menu has **Adjust Weight**, with +5%, +10%, +25%, +50%, +100%, the same downwards and **Zero Weight**. The percentage is of the move's own share, and the sum of the position always stays 100%: what a move gains the other moves give up in proportion to what they have — the heavy ones most — and what it loses goes back to them the same way. A move at 0%, or with less than 1%, cannot grow by a percentage of itself, so an increase first takes 1% from the others and grows from there; a decrease of a move at 0% does nothing. Zero Weight gives the move's whole share to the other moves that have some. The changes are written into the book in use. The list is sorted again, and the move you changed glows for a moment and is marked, left of its weight, with ↑ if it went up, ↓ if it went down, = if it stayed; the mark lasts until you leave the position.

# Opening names {#opening-names}

The Engine panel names the opening the game is in, and the Opening Tree names where each move leads.

The names come from a database of named lines. **Options ▸ Switch Opening Names** chooses which: English, Italian or none. Until you choose, the names follow the language of the interface.

A names database is an ordinary database of type Opening Book: you can open it and add your own lines, giving each the name in the Event field and the code in ECO.

# Game sources {#sources}

A source brings your games from a website into the open database and keeps them up to date.

**Database ▸ Connect Source…** adds one:

- **lichess.org**: the account's public games; signing in is optional and downloads them faster;
- **chess.com**: your user name;
- **torneionline.com**: your FIDE or FSI number, for the games of the tournaments you played.
- **ChessBase files**: a ChessBase database (`.cbh` and its files) on this computer. Choose the `.cbh` file: its games are copied in, the files stay where they are, and games added to them later arrive at the next sync. On another computer the file is not there: the sync says so and offers to ignore the source on that computer; *Database ▸ Manage Sources… ▸ Edit…* chooses the file again.
- **Lichess Study**: a study on lichess.org. Paste the address of its page (or of one of its chapters) in **Study address**: each chapter becomes a game of the database, with its comments and variations, and its tags `StudyName`, `ChapterName` and `ChapterURL` say which study and chapter it is (the study's chapters are not the chapters of a project). A chapter changed on lichess.org replaces its game at the next sync, unless the game changed here too: then both are kept. A public study needs no account; for a private one, **Sign In with lichess.org…** with an account that can see it. For now the study is only read: writing to it is coming.
- **PGN file**: a `.pgn` file on this computer, kept in step with the database. Choose it with **Browse…**, or make an empty one with **New File…**, then the **Direction**: **Read and write** (the file's games come into the database and the database's games go into the file, so a game added or changed on either side reaches the other), **Read only** (the file is never written) or **Write only** (every game of the database goes into the file; the file's other games stay out of the database). A game you save, change or annotate is written to the file a few seconds later. Nothing removed on one side is removed on the other, and a game changed on both sides between two syncs is kept twice. Pragma Chess marks each game it ties to the database with a `PragmaUid` tag, and keeps an index beside the file in a hidden file, so a file that did not change is not read again.

Sources are read when the database is opened and every twenty minutes. A game is never imported twice. **Database ▸ Manage Sources…** syncs a source now, changes it, signs in again or removes it; the games already imported stay.

The **Sources** node of the tree lists the games of each source.

# Trash {#trash}

Right-click a game of the list and choose **Move Game to Trash**. The game leaves every list and is no longer searched.

The **Trash** node, last in the tree, shows the trashed games: **Recent**, thrown away in the last seven days, and **Old**. There you can **Restore Game** or **Delete Game…**.

Deleting does not shrink the file yet. **Database ▸ Database Settings… ▸ Optimize Database** removes the deleted games for good and compacts the file. Until then nothing is lost.

# Projects {#projects}

A project is what you are looking at: the database, the game and the move, the side the board is seen from, the engine, the panels and whether you are training. The title bar shows its name — the file's, or the one given in **File ▸ Project Settings…** — with an asterisk when it has changes not saved, and the chapter open when there are several: *Openings* - The Italian - Pragma Chess*.

- **File ▸ New Project** keeps what you see — database, engine, panels — and starts a new, empty game, with White below.
- **File ▸ Open Project…** and **File ▸ Open Recent Project** open a `.pch` file.
- **File ▸ Save Project** and **File ▸ Save Project As…** save it.

You do not have to save: Pragma Chess reopens as you closed it.

Panels can be resized and closed; the **View** menu (Moves, Opening Tree, Engine, Games List) shows them again, and **View ▸ Reset Panel Layout** puts them back where they start.

# Chapters and paragraphs {#chapters}

A project is a collection of **chapters**, as a study or a chess book is, and a chapter holds games one after the other, with text between their moves. The move list shows the whole chapter: a light rule marks where each game begins, and its numbering starts again; click a move of another game and the board goes there.

Right-click the move list — on a move, on a paragraph, or anywhere, even with no moves — for:

- **Insert Paragraph**: a paragraph after the move (or where the board is), written right there in the move list. Write as in a book: the text is justified, and each new line starts a new paragraph, indented. **Esc**, **Ctrl+Enter** or a click elsewhere ends; a paragraph left empty goes away. Click a paragraph to write in it again; **Edit Paragraph**, **Move Paragraph** and **Delete Paragraph** are on its menu: Move Paragraph takes it **To the Top** or **To the Bottom** of the game, or **Up** and **Down** a half-move at a time — after White's move, Down takes it after Black's, which comes back on White's row. Paragraphs go on the main line.
- **Move Game**: the game you clicked goes **To the Top** or **To the Bottom** of the chapter, or **Up** and **Down** one place; the board stays on the game it shows.
- **Insert Game Break**: a new game from the starting position, its numbering starting from 1 again. After every game but the last there is a break already, so the new game goes at the end of the chapter; a break left with no moves after it goes away when you move to another game.

New Game, New Training, Set Up Position, the pastes and online games all add their game at the end of the chapter, and so does a game opened from the games list (one the chapter has already is simply shown). Games stored in a database are saved there as they change; the others, and every paragraph, are saved with the project.

The **File** menu has the chapters: **New Chapter…**, **Switch Chapter** to choose the one open, and **Manage Chapters…** to reorder (drag, or Move Up and Move Down), rename, add and delete them. **Project Settings…** gives the project a name of its own.

# Sync {#sync}

Sync keeps your Pragma folder — databases and projects — the same on several computers, through a folder on a server.

**Options ▸ Sync Settings…** sets it up: an **FTP** server (also with TLS), a **WebDAV** server or a **Git repository**. **Test Connection** checks it.

**File ▸ Sync Now** (Ctrl+Y, also the first button of the toolbar) does everything in order: reads the sources, saves the project and exchanges the files with the server. **Sync before closing** does it every time you quit.

Sync deletes nothing on its own: a database missing on one side is copied there, and a database changed on two computers is merged game by game. To get rid of a game use the trash, which the other computers follow.

Each computer remembers what it synced last in a hidden file of its Pragma folder, `.pragma-chess.local`, which never goes to the server. So it can tell a file you deleted by hand — in the file manager, say — from one it has yet to receive: at the next sync it asks whether to **Delete Everywhere**, **Restore** it, or **Ask Me Later** (asked again the next time you start Pragma Chess). Deleted everywhere, the file is removed from the server, and every other computer moves its copy to the trash at its next sync. A file someone changed on another computer meanwhile simply comes back.

**Manage Files…**, in the Sync Settings, lists the files in the folder on the server. Select some and press **Delete…** to clean up: after you confirm, they are deleted from every synced device, this computer included (into the trash), so they stop travelling between your computers.

With a Git repository each sync that changes files makes one commit, named after them ("Update Databases/Games.pdb; add Projects/Study.pch"): a sync that only receives, or finds nothing new, leaves the history alone.

**Options ▸ Connect Mobile App…** shows a code to scan with Pragma Chess on your phone, which then keeps a copy of your databases.

When you delete a database on the phone, the computer asks you at its next sync with the phone whether to delete it here too or keep it: **Keep It** leaves it on the computer, **Ask Me Later** asks again the next time you start Pragma Chess, and **Delete Everywhere…** warns you first that the database will be deleted from every synced device — it goes to the trash on this computer, it is removed from the sync folder on the server, and the other computers that sync with it delete their copy. Either way the phone does not receive it again.

# Language {#language}

**Options ▸ Switch Language** chooses the language of the interface. It is applied the next time Pragma Chess starts. This guide and the opening names follow it.

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
