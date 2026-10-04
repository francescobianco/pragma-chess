# TODO — handoff

Stato del lavoro e conoscenza acquisita nelle sessioni del 2 e 3 ottobre 2026,
per chi riprende (persona o agente). AGENTS.md descrive com'è fatto il codice;
qui c'è quello che AGENTS.md non dice: cosa resta da fare, le decisioni aperte,
i formati decifrati e i trucchi di verifica. Tutto è su `main`, pushato;
l'ultimo commit è `48eb8cc` (sorgente ChessBase).

## Da fare, in ordine di priorità

### 0. Release Windows: sbloccare la verifica dei pacchetti (blocca la 0.3.0)

Contesto (4 ottobre 2026, commit `6644142` e `22ef96c`). Un utente ha
segnalato che la 0.2.0 su Windows non parte: "libssl-3-x64.dll non è stato
trovato" (e `libcrypto-3-x64.dll`). Causa: Phone Link → libdatachannel →
OpenSSL linkato dinamicamente, DLL mai copiate nel pacchetto; sul runner
c'erano, quindi nessun test se ne accorgeva. In più i pacchetti pesavano
85–113 MB per colpa di Stockfish (103 MB, quasi tutto la rete NNUE grande);
l'utente vuole restare **sotto i 20 MB**.

Fatto (vedi `packaging/README.md`, "Bundled engine" e "Packages that start"):

- Stockfish ora è nostro: Stockfish 18 dal sorgente con
  `packaging/stockfish/small-net.patch` (solo la rete piccola,
  `nn-37f18f62d772.nnue`), costruito da `scripts/build-stockfish.sh`
  (ex `fetch-stockfish.sh`): 4,1 MB su Linux, 5,1 MB su Windows (MinGW,
  job `engine-windows` su Ubuntu), `x86-64-sse41-popcnt`. Stockfish 19 non
  ha più la rete piccola: per aggiornare serve una release che ce l'abbia.
  Il sorgente GPL allegato alla release è `stockfish-sf_18-pragma-source.tar.gz`.
  Explain confrontato con Stockfish 16 ufficiale: valutazioni vicine
  (`docs/tech/explain-tuning.md`, voce del 2026-10-04).
- `packaging/windows/build.ps1`: `dumpbin /dependents` su ogni exe/dll del
  pacchetto, copia OpenSSL e il runtime C++ (mai dati per presenti in
  System32), fallisce se una DLL non si trova; poi avvia `stockfish.exe`
  (deve rispondere `uci`/`bestmove`) e `pragma-chess.exe` (deve essere vivo
  dopo 15 s) con PATH = solo Windows e `SetErrorMode` che trasforma il
  dialogo "DLL mancante" in un exit code. Via anche `dxcompiler.dll`,
  `dxil.dll` (`--no-system-dxc-compiler`) e le traduzioni Qt separate
  (`--no-translations`: le nostre sono incorporate).
- macOS (`build.sh`): `otool -L` su tutto il bundle, niente fuori da
  `@rpath`/`/System`/`/usr/lib`; motore e app devono partire. **Passato in CI.**
- Linux: il motore deve rispondere. .deb e .rpm **passati in CI.**
- I test hanno impostazioni proprie (`initTestCase`: organizzazione
  "Pragma Chess Tests", INI): `syncsChessBaseFiles` falliva su Windows
  perché `QSettings()` senza organizzazione non scriveva nel registro.

Da fare:

1. Il job Windows si ferma ancora ai test, prima del packaging, quindi **la
   verifica delle DLL e l'avvio da pacchetto non sono mai stati eseguiti su
   Windows**. Fallisce `deletesDatabasesAcrossGitDevices` ("Access is
   denied", run 37215734260): è lo stesso problema della sync Git su
   Windows per cui `build.ps1` salta già `reconcilesGitFoldersWithoutDeleting`
   e `mergesDuplicatesAcrossGitDevices` (`$knownFailures`). Correggere il
   bug (meglio: probabilmente file del clone ancora aperti o in sola
   lettura quando git li sostituisce) o aggiungerlo a `$knownFailures`.
2. Rilanciare il workflow a mano (Actions ▸ Release ▸ Run workflow, o
   `gh workflow run release.yml --ref main`) finché il job `windows` passa.
   Il codice PowerShell nuovo di `build.ps1` non è mai girato (non c'è
   `pwsh` sulla macchina di sviluppo): aspettarsi qualche errore da
   correggere lì (parsing di `dumpbin`, `OPENSSL_INCLUDE_DIR` dal
   `CMakeCache.txt`, avvio del processo).
3. Controllare il peso degli artifact: stime ~12 MB l'installer Windows,
   ~20 MB (incerto) il dmg macOS. Se il dmg supera i 20 MB, sfoltire i
   plugin che `macdeployqt` copia e non servono (imageformats, ecc.).
   Poi correggere se serve "about 80 MB smaller" nel CHANGELOG.
4. Solo allora il tag della release. Dopo: aggiornare il `tag` nel manifest
   Flatpak (il modulo `stockfish` ora compila dal sorgente: verificarlo con
   `flatpak-builder`, mai provato) e avvisare l'utente che aveva la 0.2.0
   rotta su Windows.

### 1. Varianti: quel che resta

Il grosso è fatto (vedi CHANGELOG: modello, archiviazione in `games.variations`
con migrazione 7, PGN, sessione per linee, vista ad albero `MoveTreeView`,
salvataggio immediato nelle partite del database). Restano:

- **Import delle varianti da ChessBase**: `CbgDecoder` si ferma al primo
  codice di fine linea; con la regola del formato (sotto) può costruire
  l'albero intero. Verificare sulla base di esempio (253 partite annotate).
- **Comandi sulle varianti**: promuovere una variante a linea principale,
  eliminarla, tagliare la coda di una linea. Oggi non c'è modo di toglierle
  se non con un editor del PGN.
- **Allenamento con una partita del database aperta**: le risposte del
  motore ora finiscono nelle varianti della partita salvata (prima la
  partita si staccava). Decidere se va bene.
- **Il telefono** mostra la linea principale e, se riscrive una partita,
  perde le varianti (`PdbDatabase.kt`, `put("moves_san", …)` senza
  `variations`): da sistemare quando si tocca l'app Android.
- Riferimento visivo indicato dall'utente: la vista mosse degli studi lichess
  (blocchi `interrupt` sotto la mossa, varianti `inline` tra parentesi,
  `move.empty` per riprendere la numerazione del nero). La nostra vista fa la
  stessa cosa in forma più sobria.

### 2. Taratura dello Spiega (decisione dell'utente aperta)

`classifyMove` in `MoveExplanation.cpp` dice di usare la scala di lichess ma
usa soglie doppie: imprecisione ≥10, errore ≥20, errore grave ≥30 punti di
probabilità di vittoria su scala 0–100. Lichess (0.1/0.2/0.3 su scala −1..1)
corrisponde a 5/10/15. Esempio: 1.e4 e5 2.f4 exf4 3.a4 passa da −0.5 a −2.1,
13,7 punti: per noi "imprecisione", per lichess "errore". L'utente si
aspettava "errore". Non è stato cambiato perché tocca tutti i verdetti dello
Spiega, tarato in `docs/explain-tuning.md` (test con verdetti attesi alle
righe ~193, 235, 255, 546 di `tst_chessrules.cpp`). Il tutor intanto si ferma
anche sulle imprecisioni. Se l'utente dice di allineare a lichess: cambiare le
tre soglie, rifare la taratura sui casi del documento, aggiornare la guida.

### 3. Verifiche mancate sull'app vera

Fatte solo fuori schermo o nella sessione GNOME di prova (vedi sotto):

- Ridimensionamento dai bordi del telaio delle finestre, bottone Riduci,
  schermo intero (`WindowChrome`).
- Flusso completo del tutor in una partita giocata (avviso → Ritira/Spiega/
  Ignora) e il bordo rosso.
- Procedura guidata "Collega fonte… ▸ File ChessBase" con la finestra di
  scelta file (`SourceSettingsWidget`, ramo `localFile`), la finestra "Fonte
  non trovata" (`MainWindow::reportUnavailableSource`) e "Ignorata su questo
  computer" in Gestisci fonti.
- Import della base vera dell'utente nell'app (è stata provata solo con lo
  strumento di prova e i test: 14 820 partite, 0 errori, linee principali
  lunghe quanto dice l'indice).

### 4. Cose piccole note

- Toolbar: la freccetta della tendina dei tre bottoni libro/motore/database
  sta nell'angolo in basso a destra (stile Fusion); l'utente non ha chiesto di
  cambiarla.
- Finestre affiancate a mezzo schermo su GNOME: il margine dell'ombra di
  `WindowChrome` entra nella geometria che Mutter affianca (Qt 6.4 non espone
  `set_window_geometry` con margini): può restare una striscia trasparente.
- I pannelli staccabili **non esistono e non devono esistere** (decisione
  dell'utente: layout fisso, niente capricci di riorganizzazione). I dock
  hanno barra del titolo vuota e nessuna `DockWidgetFloatable`.
- `ManageSourcesDialog`: lo stato "Ignorata su questo computer" si toglie
  solo modificando la fonte (Modifica… ▸ OK). Manca un comando esplicito.
- Nomi dei giocatori ChessBase in Latin-1 (`QString::fromLatin1`); ChessBase
  usa in realtà Windows-1252: differiscono solo per i caratteri 0x80–0x9F
  (es. "€", virgolette tipografiche). Qt 6 non ha il codec 1252 di serie.
- `ChessBaseDatabase` legge annotatori (`.cbc`) e fonti (`.cbs`) ancora no;
  commenti testuali (`.cba`) no.

## Ambiente dell'utente (conta per riprodurre i bug)

- Ubuntu 24.04, GNOME 46 su **Wayland**, Qt 6.4.2 di sistema, Ubuntu Dock
  (dash-to-dock) con intellihide `ALL_WINDOWS`, dock in basso,
  `show-dock-urgent-notify` attivo. L'interfaccia dell'app è in italiano.
- `make start` dell'utente ricompila `build/` a ogni modifica: **non compilare
  in `build/`** (si corrompe `libpragma-chess-core.a`); usare una cartella
  propria (`cmake -S . -B <scratch>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`).
- Dati veri in `~/Scacchi/Pragma/` (Databases, Projects, Books). Per le prove
  copiarli altrove e usare `PRAGMA_CHESS_DIR`, `XDG_CONFIG_HOME`,
  `XDG_DATA_HOME`, `XDG_CACHE_HOME` separati: così non si toccano sessione,
  impostazioni di sincronizzazione (server!) e database dell'utente.
- Stockfish: `/usr/games/stockfish` e il bundled in `build/gui/qt/engines`.
- Wine: `~/.wine/drive_c/users/francesco/`; ChessBase Light e la sua base di
  esempio in `…/Documents/ChessBase/Bases/CB Light Database.cbh` (14 831
  record, 11 testi, 724 giocatori, 222 tornei, del 2009).

## Banco di prova GNOME headless (il modo per *vedere* davvero)

Documentato anche in AGENTS.md ("Menus and windows on Wayland"). Script di
lavoro erano nello scratchpad della sessione (persi): la ricetta è questa.

```bash
dbus-run-session -- bash -c '
  export XDG_SESSION_TYPE=wayland; unset XDG_ACTIVATION_TOKEN DESKTOP_STARTUP_ID
  gnome-shell --headless --wayland-display wl-probe --virtual-monitor 1280x800 &
  sleep 4   # la shell parte nella panoramica: mandare Esc prima di tutto
  WAYLAND_DISPLAY=wl-probe QT_QPA_PLATFORM=wayland WAYLAND_DEBUG=1 <app> > client.log 2>&1 &
  sleep 4; python3 drive.py …; kill %1 %2'
```

`drive.py`: una sola connessione D-Bus (Gio) a `org.gnome.Mutter.RemoteDesktop`
(`CreateSession` → `Session.Start`, `NotifyPointerMotionRelative(dx,dy)`,
`NotifyPointerButton(272|273, bool)`, `NotifyKeyboardKeycode(evdev, bool)`;
Esc=1, Down=108, Enter=28, F1=59, Ctrl=29, Shift=42, T=20). La sessione
muore se il client D-Bus che l'ha creata esce: non usare `gdbus` a chiamate
separate. Il puntatore parte vicino a (16,16); la finestra massimizzata ha il
pannello di GNOME nei primi 32 px.

Screenshot: `org.gnome.Mutter.ScreenCast` `CreateSession({})` →
`Session.RecordMonitor("Meta-0", {"cursor-mode": uint32 1})` → sottoscrivere
`Stream.PipeWireStreamAdded` → `Session.Start` → con il node id:
`gst-launch-1.0 -q pipewiresrc path=<node> num-buffers=1 ! videoconvert !
pngenc ! filesink location=shot.png`. Funziona (pipewire e gstreamer sono
installati). La sessione headless avvia anche Nautilus/DING/evolution nel suo
bus: muoiono con lei, ma ogni avvio costa ~20 s. Per trovare chi fa una
chiamata Wayland: `gdb -batch -x cmd` con `break
_ZN15QtWaylandClient18QWaylandXdgSurface15requestActivateEv` e `bt`.

## Bug GNOME risolti (perché, non solo cosa)

- **Dock che compare aprendo un menu**: il plugin Wayland di Qt 6.4
  (`QWaylandIntegration` ctor) collega `QGuiApplication::focusObjectChanged`
  a una lambda che chiama `requestActivate()` sulla shell surface della
  finestra col fuoco → `xdg_activation_v1.activate` con token non valido →
  Mutter marca la finestra "richiede attenzione" → la dock si mostra per le
  finestre urgenti. Fix in `main.cpp`: `QObject::disconnect(&app,
  SIGNAL(focusObjectChanged(QObject*)), nullptr, nullptr)` su Wayland (è
  l'unico ascoltatore fra tutte le librerie/plugin Qt installati: verificato
  con `objdump -R`).
- **Menu e finestre piatti**: Mutter non decora né ombreggia; l'unica
  decorazione Qt installata è `bradient` (senza ombra; `qgnomeplatform`/
  `qadwaitadecorations` per Qt 6 non esistono in Ubuntu 24.04). Soluzione:
  ombre disegnate dall'app (`GtkDesktopStyle` per i `QMenu`, `WindowChrome`
  per `QDialog` e `QMainWindow`). Insidie trovate: la finestra nativa resta
  opaca se `WA_TranslucentBackground` arriva dopo la sua creazione → si
  distrugge il `QWindow` nativo al polish e lo show lo ricrea;
  `QPainterPath::arcTo` con rettangolo vuoto non fa nulla → gli angoli
  squadrati vanno fatti con `lineTo`, altrimenti il pannello non si riempie;
  il titolo con "[*]" si legge da `windowHandle()->title()`.

## Formato ChessBase (.cbh e famiglia) — quanto decifrato e verificato

Letto e verificato su tutta la base di esempio (14 820 partite, 0 errori,
tutte le mosse legali, linea principale lunga quanto dice l'indice). Fonti:
`harshitpawar64/cbh2pgn` (Python, MIT: tabella e idea, ma legge male nomi e
tornei e tronca alla prima variante), `asavis/oschess-cb-bridge` (Rust, **AGPL:
letto solo per capire il formato, nessun codice copiato**). Il nostro codice
è scritto da zero in `gui/qt/src/app/chessbase/`.

**.cbh** — record di 46 byte, il primo è l'intestazione del file. Per record:
byte 0 flag (bit0 presente, bit1 = testo guida non partita, bit7 =
cancellato); 1–4 offset nel `.cbg` (BE); 9–11 id bianco, 12–14 id nero, 15–17
id torneo, 18–20 annotatore, 21–23 fonte (BE 3 byte, indici nelle entità);
24–26 data (BE: `anno<<9 | mese<<5 | giorno`, zeri = ignoto); 27 risultato
(0 = 0-1, 1 = ½, 2 = 1-0, altro = *); 29 turno; 31–32 Elo bianco, 33–34 Elo
nero (BE); 35–36 ECO: `(valore >> 7)` da 1 a 500 = A00…E99; 45 numero di
mosse della linea principale.

**.cbp** (giocatori) e **.cbt** (tornei): intestazione 28 byte (i primi 4 =
conteggio LE); record 67 byte (giocatori: cognome [9,39), nome [39,59)) e 99
byte (tornei: titolo [9,49), luogo [49,79), data LE 4 byte a 79 con lo stesso
schema di bit, tipo a 83). Testi Latin-1 troncati al NUL.

**.cbg** — per partita: 4 byte di testa: byte 0 flag (bit 6 = posizione
iniziale esplicita, bassi 6 bit = modo di codifica; supportiamo solo il modo
0), byte 1–3 dimensione del record (BE, testa inclusa). Se bit 6: 28 byte di
posizione: byte 1 = `0x10` nero al tratto | colonna en passant (1–8) nei 4
bit bassi; byte 2 arrocchi (1 Bianco lungo, 2 Bianco corto, 4 Nero lungo, 8
Nero corto); byte 3 numero di mossa; byte 4–27 = 192 bit di scacchiera,
casella per casella nell'ordine a1, a2, …, a8, b1, …: bit 0 = vuota, altrimenti
1, bit di colore (1 = nero), 3 bit di pezzo (1 re, 2 donna, 3 cavallo, 4
alfiere, 5 torre, 6 pedone). Poi il flusso delle mosse: ogni byte `b` si
traduce con `T[(b − n) mod 256]`, dove `T` è la tabella di 256 byte in
`CbgDecoder.cpp` e `n` è il numero di mosse decodificate finora (nulle e a
due byte comprese; i marcatori non contano).

Codici tradotti: 0 mossa nulla; 1–8 re (direzioni (0,1),(1,1),(1,0),(1,−1),
(0,−1),(−1,−1),(−1,0),(−1,1) come (Δcolonna, Δtraversa)); 9 O-O, 10 O-O-O;
11–38 donna n.1, 143–170 donna n.2, 171–198 donna n.3; 39–52 torre 1, 53–66
torre 2, 199–212 torre 3; 67–80 alfiere 1, 81–94 alfiere 2, 213–226 alfiere
3; 95–102 cavallo 1, 103–110 cavallo 2, 227–234 cavallo 3; 111–142 pedoni:
`pedone = (c−111)/4`, `modo = (c−111)%4` → (0,f), (0,2f), (f,f), (−f,f) con f =
+1 bianco, −1 nero. Donna/torre/alfiere: `k = c − inizio`, direzione
`dir[k/7]`, passi `k%7+1`, con direzioni donna (0,1),(1,0),(1,1),(1,−1), torre
(0,1),(1,0), alfiere (1,1),(1,−1); cavallo (2,1),(1,2),(−1,2),(−2,1),(−2,−1),
(−1,−2),(1,−2),(2,−1). Le coordinate si prendono **modulo 8**. 235 = mossa a
due byte: i due byte seguenti (tradotti) formano `w`; `da = w & 63`, `a = (w
>> 6) & 63` (caselle nell'ordine a1 = 0, a2 = 1, …), promozione `(w >> 12) & 3`
= D, T, A, C. 236 = da ignorare. 237–253 inutilizzati. **254 e 255 sono i
marcatori delle varianti.**

Liste dei pezzi (per lato): donne, torri, alfieri, cavalli numerati
nell'ordine di scansione a1, a2, …; pedoni in 8 slot nello stesso ordine
(nella posizione iniziale = colonna). Cattura di D/T/A/C: tolto dalla lista, i
successivi scalano; cattura di pedone: slot vuoto; en passant tolto il pedone
dietro; promozione: slot del pedone vuoto, nuovo pezzo **in coda** alla sua
lista; arrocco: re di due caselle, torre riposizionata.

**Varianti — la regola vera, verificata** (quella del Python è sbagliata, quella
del Rust è un'interpretazione diversa): ChessBase scrive **prima la linea
principale e poi le varianti**. 254 ("ramo") dice: "le mosse da qui alla 255
corrispondente sono la continuazione principale; dopo quella 255 seguono le
alternative alla prima mossa del blocco, fino alla fine del blocco che le
contiene". Quindi **la linea principale è tutto ciò che precede la prima 255**
(verificato: coincide col byte 45 dell'intestazione su tutte le partite), e
leggendo il flusso con uno stack (254 = push della posizione corrente dopo
che le mosse del blocco sono state giocate… più semplicemente: la posizione al
254 si ripristina alla 255 e le mosse dopo la 255 ripartono da lì come
variante) tutte le mosse di tutte le varianti risultano legali. Esempi reali:
`25.Nd7 ( 25…Rfd8 26.Qe5 ) 25…Rfe8 26.Re5 Qb4` = principale "…25.Nd7 Rfd8
26.Qe5", variante "25…Rfe8 26.Re5 Qb4"; `12…f6 ( 13.Bf3 Qxc4 ( 14.Be2 … ) 14.Qa3
Nc7 15.Qxa7 Qa6 ) 13.Qa3 Kb7 …`. Il decodificatore attuale si ferma alla prima
255: per importare le varianti serve prima l'albero (punto 1).

## Pagine e strumenti utili

- `ChessBaseDatabase` legge `.cbg` per partita con `QFile::seek`; `.cbh`,
  `.cbp`, `.cbt` interi in memoria. La base di esempio si legge in ~35 s con
  la verifica di legalità (nel test di sync è in blocchi da 200).
- Validare un PGN prodotto: `./build/gui/qt/pragma-explain --trace "…"` o il
  parser `Pgn::parseLine`.
- `scripts/`, `packaging/`: non toccati in queste sessioni.
