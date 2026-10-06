# TODO — handoff

Stato del lavoro e conoscenza acquisita, per chi riprende (persona o agente).
AGENTS.md descrive com'è fatto il codice; qui c'è quello che AGENTS.md non
dice: cosa resta da fare, le decisioni aperte, i formati decifrati e i
trucchi di verifica. Quello che è fatto non sta qui: è nel codice, in
AGENTS.md, nel CHANGELOG e nella storia di git.

## Da fare, in ordine di priorità

### Priorità: la sorgente ChessBase legga anche i file .2cbh

Richiesta del 6 ottobre 2026, prioritaria. La sorgente ChessBase
(`chessbase`, `app/chessbase/ChessBaseDatabase`, `ChessBaseFetch`) legge
oggi solo la famiglia `.cbh` (vedi "Formato ChessBase" più sotto). Le basi
più recenti di ChessBase sono salvate come **`.2cbh`**: il selettore di file
di Connetti sorgente deve accettarle e il lettore deve decifrarle.

- Prima cosa: procurarsi un `.2cbh` vero (ChessBase 17/18, o una base
  scaricata in quel formato) e confrontarlo con il `.cbh` della stessa base:
  intestazione, dimensione dei record, quali file compagni ha accanto
  (`.2cbg`? `.cbp`/`.cbt` come prima?). Annotare quanto decifrato nella
  sezione "Formato ChessBase", come per `.cbh`.
- Se i record sono gli stessi con un'intestazione diversa, basta riconoscere
  l'estensione e la famiglia di file; altrimenti un lettore suo accanto a
  `ChessBaseDatabase`, puro e con unit-test su record veri.
- Il filtro del selettore (`*.cbh`) diventa `*.cbh *.2cbh`; la guida in ogni
  lingua cita entrambe.

### INSIGHT.smart: tarare i piani della linea

INSIGHT (AGENTS.md, "INSIGHT") disegna i piani della linea sull'occhio.
Regole e punteggi sono costanti in testa a `smart/INSIGHT.smart`; si tara con
`pragma-explain --insight -t`, e ogni caso deciso va in
`smart/tests/plans.insight` (`--record`). Resta:

- **Tarare su linee vere con l'utente.** Prima prova (Spagnola chiusa dopo
  11…Dc7, Stockfish a profondità 30): Dd1-e2-b2-e5, c3-c5 e a6-a4. La donna
  viene dalla coda della linea (mosse 22–26), dove la PV è rumore: da
  decidere se leggere solo i primi ~20 ply (e allora forse anche il peek
  dovrebbe fermarsi lì), o se va bene così perché è la posizione che
  l'occhio mostra.
- Un pezzo il cui viaggio finisce con una cattura (Dxe5) è ancora una
  manovra: forse la cattura finale dovrebbe tagliare la strada o abbassare
  il punteggio.
- I tre colori dei percorsi (viola, arancio, verde acqua) sono in due punti,
  `BoardWidget.cpp` (`planColor`) e `BoardArrows.kt` (`ArrowColors.plans`):
  da cambiare insieme se l'utente ne vuole altri.
- Android: l'occhio non c'è ancora; quando arriverà, le frecce ci sono già
  (`lineInsight`, `drawExplanation` disegna `via`).

### Prossimo: limitare la CPU del motore — "Potenza di calcolo" a 5 livelli

Richiesta del 6 ottobre 2026, da fare (proposta, l'utente deve confermare i
livelli). Oggi l'analisi è `go infinite` su metà dei core
(`UciEngine::handleLine`, `uciok`: `idealThreadCount() / 2`) e non si ferma
mai: anche a posizione ferma, con la finestra in secondo piano, metà della
CPU resta al 100%, ventole e batteria comprese. Il numero di thread da solo
non basta a moderarla: **quello che consuma è che l'analisi non finisce
mai**. Le leve vere, insieme:

1. **Thread** (`setoption Threads`): quanta CPU nello stesso istante.
2. **Fine dell'analisi**: fermarsi a una profondità o dopo un tempo per
   posizione (`go depth N` / `go movetime`), invece di `infinite`. A
   posizione analizzata la CPU torna a zero. Spiega ha bisogno di
   profondità ≥ `kExplainBeforeDepth` (16) e di 3 profondità su 4 dopo
   `MIN_DEPTH` (8): ogni livello deve arrivare almeno a ~20.
3. **Priorità del processo**: il motore a priorità bassa cede la CPU a tutto
   il resto (il sistema resta reattivo), ma non scende sotto il 100% se la
   CPU è libera. Unix: `QProcess::setChildProcessModifier` con
   `setpriority`/`nice(10)`; Windows: `SetPriorityClass(...,
   BELOW_NORMAL_PRIORITY_CLASS)` sul `processId()`. Non è una leva UCI: vale
   per ogni motore.
4. **Hash** in proporzione (meno memoria ai livelli bassi).

Proposta dei livelli (Motore ▸ Gestisci motori…, al posto di "Thread:
Automatico"; un'impostazione per computer, in `EngineProfile`):

| Livello      | Thread       | L'analisi si ferma a        | Priorità |
|--------------|--------------|-----------------------------|----------|
| 1 Minima     | 1            | profondità 20 o 10 s        | bassa    |
| 2 Leggera    | 25% dei core | profondità 24 o 20 s        | bassa    |
| 3 Equilibrata (default) | 50% | profondità 30 o 60 s      | bassa    |
| 4 Alta       | 75%          | mai (infinita)              | normale  |
| 5 Piena      | tutti        | mai, nessun limite          | normale  |

- Thread e Hash scritti a mano da chi li conosce restano: un valore
  esplicito vince sul livello (il livello decide solo dove oggi c'è
  "Automatico" / "predefinito").
- **Pausa in secondo piano** (proposta, per ogni livello sotto Piena): con
  la finestra ridotta a icona o non attiva per più di qualche minuto
  l'analisi si ferma (`stop`) e riparte al ritorno
  (`QGuiApplication::applicationStateChanged`). È la cosa che risparmia di
  più, e non costa niente a chi guarda la scacchiera.
- L'allenamento (`kTrainingDepth`) e la ricerca "prima" di Spiega sono già
  a profondità fissa: non cambiano, cambia solo quanti thread usano.
- Il pannello del motore dice quando l'analisi si è fermata per il limite
  ("Analisi completata a profondità 24"), con un modo di continuare
  (il bottone di analisi riparte senza limite per quella posizione).
- Scartato: sospendere il processo a intermittenza (SIGSTOP/SIGCONT,
  `NtSuspendProcess`) per un "x% di CPU": funziona, ma è fragile, diverso
  per ogni sistema e falsa i tempi che il motore misura.
- Da fare: `EngineProfile::power` (1–5) in `EngineCatalog` (unit-test di
  load/save), la traduzione livello → thread/limite/priorità pura e
  testata (`EnginePower`), `UciEngine` che accetta priorità e limite,
  `MainWindow::analyzeCurrentPosition` che usa il limite, il campo nel
  dialogo, la guida in ogni lingua, `GET /api/state` col livello.

### 0. Dopo la 0.3.0: quel che resta dei pacchetti

- **Al prossimo rilascio, pulizia:** la release 0.3.0 ha quattro file
  estranei, `AUTHORS`, `Copying.txt`, `README.txt` e uno `stockfish.exe`
  sciolto, arrivati dall'artifact `engine-windows`. Il workflow è già
  corretto (`pattern:` nel download degli artifact, commit `9a27ed0`): alla
  prossima release controllare che non ci siano più, poi toglierli dalla
  0.3.0 (`gh release delete-asset v0.3.0 <nome>`) e rigenerare il suo
  `SHA256SUMS.txt` senza di loro.
- **Peso:** l'utente vuole i pacchetti **sotto i 20 MB**. La 0.3.0: setup
  Windows 18 MB, zip 23 MB, dmg 33 MB, deb/rpm 7 MB. Guardare cosa c'è
  dentro zip e dmg (plugin Qt superflui di `windeployqt`/`macdeployqt`:
  imageformats, tls, styles, …; il runtime) e sfoltire.
- **Flathub:** il modulo `stockfish` del manifest ora compila dal sorgente;
  mai provato con `flatpak-builder`. Aggiornare il `tag` e provarlo.
- Avvisare l'utente che aveva la 0.2.0 rotta su Windows (DLL di OpenSSL
  mancanti) che la 0.3.0 la corregge.
- Stockfish 19 non ha più la rete piccola (`packaging/stockfish/small-net.patch`):
  per aggiornare il motore incluso serve una release che ce l'abbia.

### 1. Varianti: quel che resta

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

Il giudizio ora sta in `smart/TUTOR.smart` (`Classify`, costanti
`BLUNDER_DROP`, `MISTAKE_DROP`, `INACCURACY_DROP`): dice di usare la scala
di lichess ma usa soglie doppie: imprecisione ≥10, errore ≥20, errore grave ≥30 punti di
probabilità di vittoria su scala 0–100. Lichess (0.1/0.2/0.3 su scala −1..1)
corrisponde a 5/10/15. Esempio: 1.e4 e5 2.f4 exf4 3.a4 passa da −0.5 a −2.1,
13,7 punti: per noi "imprecisione", per lichess "errore". L'utente si
aspettava "errore". Non è stato cambiato perché tocca tutti i verdetti dello
Spiega, tarato in `docs/tech/explain-tuning.md` (test con verdetti attesi in
`tst_chessrules.cpp` e nei casi di `smart/tests/`). Il tutor intanto si ferma
anche sulle imprecisioni. Se l'utente dice di allineare a lichess: cambiare le
tre soglie, rifare la taratura sui casi del documento, aggiornare la guida.

### 2b. SMART e Spiega reattivo: quel che resta

- **La frase posizionale persa.** Spiega reattivo non ha più la sonda lungo
  la variante (ricerche brevi su ogni posizione della linea, `concretePly`):
  "nessun materiale lo spiega: la valutazione è posizionale, chiara dopo …"
  non compare più, al suo posto "Linea principale: …" (es. 3.a4 nel Gambetto
  di re). Va ricavata dai tick, in EXPLAIN.smart: la memoria tra un tick e
  l'altro ha le valutazioni per profondità (`settledDepth` del desktop è
  l'idea: da che profondità la valutazione regge). Decidere cosa dire con
  quel dato, poi togliere `concretePly` da `Explain` e la sonda da
  `ExplanationSearch` (oggi solo `pragma-explain` la usa, con probe 0).
- **9.Bxf6 (feedback del 5 ottobre, caso in `smart/tests/user-feedback.ticks`).**
  1.d4 d5 2.Nf3 Nf6 3.Nc3 e6 4.Bg5 Bb4 5.a3 Bxc3+ 6.bxc3 c5 7.dxc5 Qc7
  8.Qd4 Nc6 9.Bxf6: l'utente non capiva la freccia 2. È f6–d4 blu, la
  ripresa del Bianco 10.Bxd4 dopo 9…Nxd4: la sequenza finisce lì perché il
  materiale si conta a scambi finiti. Resta: la frase "vince la donna per
  2 cavalli" conta anche il cavallo che la mossa stessa ha preso (9.Bxf6):
  giusto in bilancio, strano da leggere. Meglio "la donna era attaccata:
  9.Qe3 la salvava".
- **Minacce a più mosse (feedback del 6 ottobre).** Spiega vede le minacce
  di una presa sola (`ThreatText`, 4…Bxf3). Restano, come dice l'utente,
  le minacce tattiche forzate a più mosse (un'infilata, un doppio che si
  prepara con uno scacco, un sacrificio che apre), più difficili da
  raccontare; servirà guardare più a fondo, forse
  scambi interi (SEE) invece di una ripresa, e disegnarle (una freccia
  della minaccia?). Poi i temi posizionali (coppia degli alfieri, sviluppo).
- **I temi del re (8…Bc5 a profondità 40, 6 ottobre).** Dopo 9.b4 Bxb4
  10.Nxb4 Nxb4 11.Qb3! Spiega dice l'attacco doppio, ma +3.7 con un pedone
  in più non si spiega ancora del tutto. Dopo 12.Bxf7+ Kf8 il
  materiale torna pari e il resto è il re nero che non arrocca più; la
  frase "non si perde materiale: è posizionale" è vera ma non dice *quale*
  posizione. Servono i temi del re (arrocco perso, re esposto).
- **Android: la posizione prima della mossa.** Il desktop, quando Spiega
  non ha la valutazione della posizione prima (si arriva diretti alla mossa,
  o dopo un riavvio), la analizza prima per un momento
  (`unjudgedBefore`, `kExplainBeforeDepth`); il telefono no: lì la mossa
  resta senza giudizio finché l'utente non è passato dalla posizione prima.
  Portare lo stesso giro in `AppViewModel.positionChanged`.
- **La minaccia ignorata (5…Qh4+, 6 ottobre).** Spiega dice "5…Qh4+ leaves
  the rook on a8 attacked: 7.Qxa8" (`IgnoredThreatText`). Resta: nella linea del motore il Nero
  riprende la torre (7…Qxa1) e il vantaggio viene da 8.Qxb8+; la frase non
  lo dice. E solo nel ramo dell'errore senza materiale: se il materiale
  cade, il ramo del materiale non cerca minacce.
- **Le combinazioni profonde (8…Bc5, 6 ottobre).** Spiega disegna ora il
  cammino del pezzo che cattura (12.Bxf7+ 13.Ne5+ 14.Ng6+ 15.Nxh8), ma lo
  trova solo quando il motore è arrivato a vederlo (profondità ~26 qui): a
  profondità più basse dice "guadagna tempo", vero ma parziale. Sul desktop
  l'analisi live ci arriva in pochi secondi; vale la pena vedere se la
  spiegazione cambia davanti all'utente e se va detto ("il motore sta ancora
  cercando"). Da osservare dal vero.
- **Stabilità di Spiega (8…Bc5, 6 ottobre).** Da osservare sul desktop vero: se capita ancora di vedere spiegazioni che
  cambiano, registrare i tick (`PRAGMA_EXPLAIN_RECORD`) e rigiocarli. Resta
  12.Bxe5, che a profondità 18 oscilla tra "vince un alfiere per un pedone"
  e la spiegazione senza materiale (il Bianco riprende un pedone dopo
  13.Bb5+): forse il "tiene 4 semimosse" va rivisto per i guadagni netti
  che restano positivi.
- **Il tutor reattivo.** `TrainingTutor::judge` chiama ancora `Judge` una
  volta, a fine ricerca della risposta del motore. Con `Tick` anche lì
  l'avviso arriverebbe appena la ricerca mostra il crollo.
- **Ricarica a caldo.** Con `PRAGMA_SMART_DIR` i programmi si rileggono solo
  al riavvio: rileggerli quando il file cambia (QFileSystemWatcher) renderebbe
  il tuning sul desktop immediato.
- **Salvare i tick dall'interfaccia.** Oggi si registrano solo con la
  variabile `PRAGMA_EXPLAIN_RECORD`; un comando (es. nel menu del pannello
  Motore: "Copia i tick di Spiega") porterebbe un caso sbagliato a
  `pragma-explain --replay` senza riavviare.
- **Android: frasi nuove.** `ExplainStrings` mappa a mano le frasi di
  EXPLAIN.smart sulle risorse `explain_*`; una frase nuova resta in inglese
  sul telefono finché non si aggiunge. Serve un test come
  `tst_chessrules` (ogni `TEXT("…")` di EXPLAIN.smart è nella mappa).
- **Android: posizioni senza re.** Il desktop ora accetta come posizione di
  partenza un diagramma senza re (i capitoli di testo degli studi lichess,
  `ChessPosition::Kings::Optional`); la `Position` Kotlin no, quindi quei
  capitoli sul telefono non si aprono. Portare la stessa regola.
- **Non provato a mano:** Spiega reattivo sul desktop vero (visto solo nei
  test e nella CLI: frecce che cambiano solo quando reggono, matto che non
  riparte) e sul telefono vero (solo test JVM e APK compilate).

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
255: per importare le varianti serve prima l'albero (punto 1, "Varianti").

## Pagine e strumenti utili

- `ChessBaseDatabase` legge `.cbg` per partita con `QFile::seek`; `.cbh`,
  `.cbp`, `.cbt` interi in memoria. La base di esempio si legge in ~35 s con
  la verifica di legalità (nel test di sync è in blocchi da 200).
- Validare un PGN prodotto: `./build/gui/qt/pragma-explain --trace "…"` o il
  parser `Pgn::parseLine`.
- `scripts/`, `packaging/`: non toccati in queste sessioni.
