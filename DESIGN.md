# Open Source Chess Database Desktop

## Visione

Voglio sviluppare un'applicazione desktop open source per la gestione, consultazione e analisi di grandi database di partite di scacchi.

L'obiettivo non è realizzare semplicemente una scacchiera grafica con un archivio PGN, ma costruire un vero **chess database engine** con sopra un'interfaccia desktop nativa.

Il progetto deve poter diventare qualcosa concettualmente vicino a ChessBase o SCID, ma con un'architettura moderna, aperta, modulare e pensata fin dall'inizio per separare completamente:

**chess database engine → application core → desktop GUI**

La caratteristica fondamentale dell'interfaccia è che deve sembrare una vera applicazione del sistema operativo.

Non voglio Electron e non voglio un'interfaccia HTML/CSS travestita da applicazione desktop.

Su Windows deve sembrare un'applicazione Windows, su macOS deve integrarsi correttamente con macOS e su Linux deve inserirsi bene in un ambiente desktop moderno, con particolare attenzione a GNOME.

---

# Stack proposto

La scelta principale per la GUI è:

**Qt 6 + Qt Widgets**

Qt Widgets è preferibile a Qt Quick/QML perché l'applicazione sarà fortemente desktop-oriented.

Avremo elementi come:

* menu
* toolbar
* splitter
* tree view
* table view
* dock
* dialog
* tab
* shortcut da tastiera
* drag & drop
* menu contestuali
* finestre multiple

Sono esattamente gli elementi nei quali Qt Widgets è particolarmente maturo.

Una possibile interfaccia iniziale potrebbe essere:

```text
┌──────────────────────────────────────────────────────────┐
│ File  Edit  Game  Position  Database  Engine  Tools Help │
├──────────────┬────────────────────────┬──────────────────┤
│ DATABASES    │                        │ MOVES            │
│              │                        │                  │
│ My Games     │      CHESSBOARD        │ 1. e4 e5         │
│ Tournaments  │                        │ 2. Nf3 Nc6       │
│ Players      │                        │ 3. Bb5 a6        │
│ Openings     │                        │                  │
│              │                        ├──────────────────┤
│              │                        │ ENGINE           │
│              │                        │ Stockfish        │
├──────────────┴────────────────────────┴──────────────────┤
│ Search [___________________]        4,283,192 games      │
└──────────────────────────────────────────────────────────┘
```

La scacchiera è però solo una delle viste dell'applicazione.

Non deve diventare il centro dell'architettura.

---

# Principio architetturale fondamentale

**Non partire dalla scacchiera.**

Disegnare una scacchiera, muovere pezzi e leggere un PGN sono problemi relativamente semplici.

La parte realmente interessante del progetto è:

> progettare un database scacchistico capace di indicizzare milioni di partite e rispondere velocemente a query scacchistiche.

Quindi il progetto dovrebbe nascere prima come **chess database engine** e solo successivamente diventare una GUI.

Concettualmente:

```text
                ┌───────────────────┐
                │    Desktop GUI    │
                │    Qt Widgets     │
                └─────────┬─────────┘
                          │
                ┌─────────▼─────────┐
                │ Application Core  │
                └─────────┬─────────┘
                          │
                ┌─────────▼─────────┐
                │ Chess DB Engine   │
                └───────────────────┘
```

Il database engine non dovrebbe sapere che esiste Qt.

---

# Linguaggi

Una soluzione tradizionale sarebbe:

```text
C++20/23
Qt 6 Widgets
SQLite
CMake
Stockfish/UCI
```

Ma una soluzione architetturalmente più interessante potrebbe essere:

```text
              Qt 6 / C++
                   │
                   │ FFI/API
                   ▼
              Rust Core
                   │
        ┌──────────┼──────────┐
        ▼          ▼          ▼
       PGN       Search     Database
        │                     │
        ▼                     ▼
   Chess Model              SQLite
        │
        ▼
       UCI
        │
        ▼
    Stockfish
```

Rust potrebbe quindi implementare tutto ciò che costituisce il vero motore dell'applicazione.

Qt/C++ rimarrebbe principalmente responsabile della GUI.

---

# Struttura iniziale del repository

Una possibile organizzazione:

```text
chessdb/
│
├── core/
│   ├── chess/
│   ├── pgn/
│   ├── database/
│   ├── index/
│   ├── search/
│   ├── opening/
│   ├── engine/
│   └── uci/
│
├── gui/
│   └── qt/
│
├── cli/
│
├── tests/
│
├── benchmarks/
│
└── docs/
```

Il core deve poter funzionare senza GUI.

Idealmente:

```bash
chessdb import games.pgn

chessdb search --player "Carlsen, Magnus"

chessdb search --position "..."

chessdb stats

chessdb analyze game.pgn
```

La CLI diventerebbe contemporaneamente uno strumento utile e un modo eccellente per testare il core senza coinvolgere Qt.

---

# Modello concettuale

Bisogna distinguere almeno quattro livelli.

## 1. Chess model

Rappresentazione efficiente dello stato scacchistico:

```text
Position
Move
Piece
Square
Color
CastlingRights
EnPassant
HalfMoveClock
```

Internamente potremmo utilizzare bitboard.

Per esempio:

```text
white_pawns
white_knights
white_bishops
white_rooks
white_queens
white_king

black_pawns
...
```

Questo livello deve essere estremamente efficiente perché verrà utilizzato anche durante indicizzazione e ricerca.

---

# 2. PGN model

Il PGN non deve essere confuso con il database.

Il PGN è un formato di import/export.

Bisogna supportare almeno:

```text
headers
moves
variations
comments
NAG
annotations
results
```

Una partita potrebbe quindi essere modellata come:

```text
Game
 ├── Metadata
 │    ├── Event
 │    ├── Site
 │    ├── Date
 │    ├── Round
 │    ├── White
 │    ├── Black
 │    ├── Result
 │    └── ECO
 │
 └── MoveTree
      ├── Move
      ├── Comment
      ├── NAG
      └── Variations
```

Il database interno non deve necessariamente conservare tutto nella stessa rappresentazione testuale del PGN.

---

# 3. Database metadata

SQLite è un ottimo candidato iniziale per:

```text
players
games
events
sites
tournaments
sources
collections
tags
annotations
```

Esempio concettuale:

```sql
games
-----
id
white_id
black_id
event_id
date
result
white_elo
black_elo
eco
ply_count
moves_offset
```

I dati pesanti delle mosse potrebbero eventualmente essere conservati separatamente in una rappresentazione binaria compatta.

Questa decisione andrà verificata tramite benchmark e non presa prematuramente.

---

# 4. Chess Search Engine

Questa è probabilmente la parte più interessante dell'intero progetto.

Una normale query SQL può facilmente rispondere a:

```text
Carlsen vs Nakamura
```

oppure:

```text
games where year >= 2020
and white_elo > 2700
and eco = "B90"
```

Ma un database scacchistico deve poter rispondere anche a domande come:

> mostrami tutte le partite nelle quali è comparsa questa posizione.

Oppure:

> mostrami partite con questa struttura pedonale.

Oppure:

> trova partite che hanno raggiunto una posizione simile.

Oppure:

> trova tutte le partite nelle quali questa sequenza di mosse è stata giocata.

Queste non sono più semplici query relazionali.

---

# Position indexing

Durante l'importazione di una partita:

```text
PGN
 │
 ▼
Parse moves
 │
 ▼
Initial Position
 │
 ├── move 1
 │      ▼
 │   position
 │
 ├── move 2
 │      ▼
 │   position
 │
 ├── move 3
 │      ▼
 │   position
 │
 ...
```

Per ogni posizione potremmo calcolare una fingerprint.

Per esempio tramite Zobrist hashing:

```text
position
    │
    ▼
Zobrist hash
    │
    ▼
64 bit key
```

L'indice potrebbe concettualmente contenere:

```text
position_hash
game_id
ply
```

Consentendo una ricerca del tipo:

```text
position
   │
   ▼
hash(position)
   │
   ▼
position index
   │
   ▼
game 18392 / ply 34
game 73921 / ply 28
game 88321 / ply 31
```

Bisognerà naturalmente considerare collisioni e verifica finale della posizione.

---

# Ricerca per struttura

Qui il progetto potrebbe diventare particolarmente interessante.

Una posizione non è soltanto una configurazione esatta.

Potremmo costruire diversi fingerprint.

Per esempio:

```text
EXACT_POSITION
PAWN_STRUCTURE
MATERIAL
KING_STRUCTURE
PIECE_CONFIGURATION
OPENING_POSITION
```

Una posizione potrebbe quindi produrre:

```text
position_hash
pawn_hash
material_hash
```

Questo permetterebbe query come:

> tutte le partite con questa struttura pedonale.

anche quando gli altri pezzi si trovano in posizioni differenti.

---

# Query language

In prospettiva sarebbe interessante avere un piccolo linguaggio di query scacchistico.

Per esempio:

```text
player:white = "Carlsen"
year >= 2020
result = win
```

oppure:

```text
position = current
```

oppure:

```text
pawn_structure = current
```

oppure combinazioni:

```text
player = "Carlsen"
AND
pawn_structure = current
AND
year >= 2018
```

La GUI potrebbe semplicemente essere un frontend visuale di questo linguaggio.

Questo avrebbe un vantaggio architetturale enorme:

```text
GUI
CLI
API
```

potrebbero utilizzare lo stesso sistema di query.

---

# Opening tree

Una funzionalità fondamentale sarebbe costruire dinamicamente un albero delle mosse partendo dal database.

Per una posizione:

```text
              current position
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
       e5            c5           e6
     42.1%         31.7%        14.2%
```

Per ogni continuazione:

```text
games
white wins
draws
black wins
average elo
performance
year distribution
```

La stessa infrastruttura dell'indice posizionale potrebbe diventare la base dell'opening explorer.

---

# Stockfish

Stockfish dovrebbe essere trattato come componente esterno tramite protocollo UCI.

Il core dovrebbe implementare qualcosa del tipo:

```text
EngineManager
      │
      ▼
UCI Engine
      │
      ├── Stockfish
      ├── Berserk
      ├── Dragon
      └── qualsiasi engine UCI
```

Non bisogna quindi legare l'architettura direttamente a Stockfish.

L'applicazione supporta **UCI engines**.

Stockfish è semplicemente quello predefinito o consigliato.

---

# GUI

La GUI Qt potrebbe essere composta da dock indipendenti:

```text
Chessboard
Moves
Database
Search
Opening Tree
Engine
Games
Players
Annotations
Tournament
Statistics
```

Qt `QDockWidget` sarebbe particolarmente adatto.

L'utente potrebbe quindi costruire il proprio workspace.

Per esempio:

```text
PLAY
ANALYSIS
DATABASE
OPENING PREPARATION
TOURNAMENT
```

e salvare diversi layout.

---

# Native desktop philosophy

Un principio importante del progetto dovrebbe essere:

> Native desktop first.

Non vogliamo reinventare:

* title bar
* menu
* file picker
* scrollbar
* keyboard navigation
* clipboard
* drag & drop
* context menu
* accessibility
* window management

Quando possibile devono essere utilizzati i comportamenti forniti da Qt e dal sistema operativo.

L'applicazione non deve avere l'aspetto di un sito web dentro una finestra.

---

# Una possibile evoluzione

L'architettura separata permetterebbe in futuro:

```text
                Chess DB Core
                      │
       ┌──────────────┼──────────────┐
       │              │              │
       ▼              ▼              ▼
    Qt GUI           CLI            API
       │
       ▼
Desktop App
```

E eventualmente:

```text
Python bindings
Rust library
C API
```

senza dover riscrivere il database engine.

---

# Prima milestone

La prima milestone non dovrebbe essere:

> mostrare una bella scacchiera.

Dovrebbe essere qualcosa di molto meno spettacolare ma architetturalmente molto più importante:

```text
PGN
 ↓
Parser
 ↓
Chess model
 ↓
Database
 ↓
Position index
 ↓
Query engine
 ↓
CLI
```

Test:

```bash
chessdb import million-games.pgn
```

e successivamente:

```bash
chessdb search --player "Carlsen, Magnus"
```

poi:

```bash
chessdb search --position "FEN..."
```

L'obiettivo iniziale potrebbe essere:

**importare 1 milione di partite e riuscire a recuperare rapidamente tutte le partite nelle quali compare una determinata posizione.**

Questo sarebbe già il nucleo di un vero database scacchistico.

Solo quando questo funziona bene:

```text
Qt GUI
   ↓
Chessboard
   ↓
Game browser
   ↓
Search UI
   ↓
Opening explorer
   ↓
Engine analysis
```

---

# Domanda architetturale centrale

La domanda più importante del progetto non è:

> come disegno la scacchiera?

ma:

> **come rappresento, comprimo e indicizzo centinaia di milioni o miliardi di posizioni provenienti da milioni di partite affinché una ricerca scacchistica sia quasi immediata?**

È questo il problema che può dare identità tecnica al progetto.

La GUI può essere eccellente, ma dovrebbe essere considerata il client di un motore molto più interessante.

Il progetto potrebbe quindi nascere non come **“un'altra GUI per giocare a scacchi”**, ma come:

> **an open-source chess database engine with a native cross-platform desktop client.**

Questa distinzione dovrebbe guidare tutte le decisioni successive.
