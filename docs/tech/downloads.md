# Scaricare motori e database: come fa En Croissant

> **Fatto (2026-10-11):** Aiuto ▸ Gestione estensioni, con il registro di En
> Croissant come primo provider: i motori si installano (scaricati dai loro
> autori, aperti con miniz, configurati in Gestisci motori), i database sono
> elencati ma non installabili. Vedi AGENTS.md, "Extensions".

Studio del 2026-10-11 su come [En Croissant](https://github.com/franciscoBSalgueiro/en-croissant)
offre "con un clic" motori e database (la sua funzione più citata dagli
utenti, POSITIONING.md), per decidere più avanti come farlo noi. Niente qui è
ancora deciso né scritto nel codice.

## In breve

- **Nessun gestore di pacchetti.** En Croissant non chiama mai apt, dnf,
  pacman, winget, brew: scarica con una sua richiesta HTTP un file, lo apre se
  è un archivio, lo mette nella sua cartella dei dati e lo rende eseguibile.
- **I cataloghi stanno sul suo sito**, non nel programma: tre file JSON che
  il programma legge quando apri la finestra (motori, database, problemi).
  Cambiano senza una nuova release.
- **I motori vengono dai loro autori** (release su GitHub di Stockfish e
  RubiChess, il sito di Komodo): il binario ufficiale per il sistema e la CPU.
- **I database vengono da lui, già convertiti** nel suo formato (SQLite,
  `.db3`), ospitati su un suo dominio (Cloudflare R2): il programma li scarica
  e li apre, non converte niente.

## I motori

Il catalogo è `https://encroissant.org/engines?os=<sistema>&bmi2=<sì/no>`
(`src/utils/engines.ts`, `useDefaultEngines`). Il programma chiede prima al
suo lato Rust se la CPU ha le istruzioni BMI2 (`isBmi2Compatible`), poi tiene
le voci di quel sistema e di quella CPU. Ogni voce:

```json
{
  "name": "Stockfish", "version": "19", "os": "linux", "bmi2": true,
  "image": "https://upload.wikimedia.org/…/NewLogoSF.png",
  "downloadLink": "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/stockfish-linux-x86-64-universal.tar.gz",
  "path": "stockfish/stockfish-linux-x86-64-universal",
  "elo": 3635, "downloadSize": 81431614
}
```

Il catalogo di oggi (23 voci):

| Motore | Versione | Sistemi | Da dove |
|---|---|---|---|
| Stockfish | 19 | Windows, macOS, Linux (con e senza BMI2) | release ufficiali su GitHub (`official-stockfish`) |
| RubiChess | 20240817 | Windows, Linux | release dell'autore su GitHub |
| Dragon by Komodo | 1 | Windows, macOS, Linux | `komodochess.com/pub/dragon.zip` |
| Komodo | 14 | Windows, macOS, Linux | `komodochess.com/pub/komodo-14.zip` |
| Leela Chess Zero | 0.30.0 | solo Windows | release su GitHub |

L'installazione (`src-tauri/src/fs.rs`, `download_file`):

1. una richiesta GET (reqwest), tutto il file in memoria, con l'avanzamento
   mostrato in percentuale;
2. dall'estensione dell'indirizzo: `.zip` aperto (crate `zip`), `.tar` e
   `.tar.gz` aperti (`tar`, `flate2`), altro scritto così com'è;
3. nella cartella dei motori dell'app, per utente; poi `set_file_as_executable`
   (permessi 0755 su Linux e macOS);
4. il motore entra nella lista con il percorso del catalogo (`path`) dentro
   la cartella aperta.

Oltre ai motori locali ce ne sono due **remoti**: il motore cloud di lichess
e chessdb.cn, interrogati per posizione.

## I database

Il catalogo è `https://encroissant.org/databases` (`src/utils/db.ts`,
`useDefaultDatabases`):

| Database | Partite | Dimensione | File |
|---|---|---|---|
| Lumbra's GigaBase (2025-06) | 9 570 564 | 2,8 GB | `db.encroissant.org/LumbrasGigaBase2025-06.db3` |
| Caissabase 2024 | 5 404 926 | 1,3 GB | `db.encroissant.org/caissabase_2024.db3` |
| Ajedrez Data – OTB | 4 279 012 | 1,0 GB | `db.encroissant.org/AJ-OTB.db3` |
| MillionBase | 3 451 068 | 0,8 GB | `db.encroissant.org/mb-3.db3` |
| Ajedrez Data – Correspondence | 1 524 027 | 0,3 GB | `db.encroissant.org/AJ-COR.db3` |

E `https://encroissant.org/puzzle_databases`: *Lichess Puzzles 2026*
(`db.encroissant.org/Lichess Puzzles 2026.db3`, 1,2 GB).

Sono **file SQLite già pronti** nel formato di En Croissant (l'intestazione è
`SQLite format 3`): la conversione dal PGN o dal formato di Scid l'ha fatta lui
una volta, sul suo computer. Il programma li scarica in una richiesta sola e li
apre. Il dominio `db.encroissant.org` è dietro Cloudflare e gli ETag sono di un
caricamento a più parti di tipo S3 (`…-149`): è **Cloudflare R2**, che non fa
pagare il traffico in uscita — il motivo per cui può distribuire gigabyte a
migliaia di utenti. Le date dicono che i file vengono aggiornati raramente
(MillionBase è del febbraio 2024).

Il PGN dell'utente (anche `.pgn.zst` e `.pgn.bz2`) si converte invece in
locale, dalla scheda "Local".

## Le licenze

**Motori: niente da ridistribuire.** I binari non stanno sul server di En
Croissant: il suo catalogo ha solo nome, versione e il link alla fonte
ufficiale (le release di Stockfish e RubiChess su GitHub, `komodochess.com`),
e il programma scarica da lì, come farebbe l'utente col browser. Il GPL di
Stockfish e le condizioni di Komodo restano tra l'utente e l'autore.

**Database: qui sì.** I `.db3` li ospita lui, convertiti: è una
ridistribuzione di un'opera derivata, e conta la licenza di ciascuna raccolta.

| Raccolta | Condizioni trovate (2026-10-11) | Cosa vorrebbero |
|---|---|---|
| Lumbra's GigaBase | CC BY-NC-SA 4.0 dal piè di pagina del sito (il testo annuncia anche CC BY-NC 4.0); i dati di lichess fino al 2022 CC BY-SA 4.0; le partite di TWIC tolte dalla raccolta su richiesta | l'attribuzione all'autore (Michael Jansen), nessun uso commerciale, la versione convertita con la stessa licenza. Il catalogo di En Croissant non ha descrizione, autore né licenza per nessuna voce, e offre la versione di giugno 2025 |
| Caissabase | nessuna condizione trovata (il sito rifiuta l'accesso automatico) | da chiedere all'autore |
| MillionBase, Ajedrez Data | nessuna condizione trovata | da chiedere |

"Gratis da scaricare" non vuol dire "ridistribuibile". Le mosse di una partita
sono fatti, ma in Europa una raccolta può essere protetta dal diritto *sui
generis* sulle banche dati, e i commenti dal diritto d'autore.

**Per noi:** i motori come fa lui (il catalogo con i link alle fonti
ufficiali); i database li ospitiamo solo con una licenza chiara che permetta
la ridistribuzione (i dati di lichess sono CC0), con fonte, autore e licenza
scritti nel catalogo, nella finestra e nelle proprietà del database
(`DatabaseProperties`). Dove la licenza non c'è o non lo permette, il catalogo
indica la fonte originale e il programma converte sul computer dell'utente
(Strumenti ▸ Converti).

## Pilotare apt-get sarebbe una porcata?

Sì. Per Pragma Chess sarebbe la strada sbagliata, per molte ragioni insieme:

1. **Servono i permessi di amministratore.** `apt-get install` vuole root: il
   programma dovrebbe chiedere la password con pkexec o sudo per installare un
   motore. Un'app che chiede la password di sistema per una funzione di
   comodo spaventa, e giustamente.
2. **Vale per una sola famiglia di Linux.** apt su Debian e Ubuntu, dnf su
   Fedora, pacman su Arch, zypper su openSUSE: quattro strade da mantenere, e
   Windows e macOS (il 70% e passa degli utenti di En Croissant) restano
   fuori comunque.
3. **Non funziona nei pacchetti chiusi.** Dentro Flatpak (Flathub è un nostro
   obiettivo) l'app non vede il sistema e non può installarci niente.
4. **Versioni vecchie.** I repository delle distribuzioni hanno spesso uno
   Stockfish di uno o due anni prima (Debian stabile); gli autori pubblicano
   i binari nuovi su GitHub il giorno stesso.
5. **Tocca lo stato del sistema.** Un pacchetto installato da noi resta lì,
   con le sue dipendenze, finché qualcuno non lo toglie a mano; il lock di
   dpkg occupato da un aggiornamento in corso fa fallire tutto; gli errori di
   apt arrivano in un formato che non controlliamo.
6. **Niente database.** I gestori di pacchetti non hanno i database di partite:
   per quelli una via nostra serve comunque.

Quello che di apt vale la pena tenere c'è già: **Gestisci motori ▸ Rileva
motori** (`EngineDetector`) trova i motori installati nel sistema (PATH e
cartelle solite), quindi chi ha fatto da sé `sudo apt install stockfish` lo
ritrova senza fare niente. Il pacchetto AUR usa lo Stockfish di Arch nello
stesso spirito: è la distribuzione a installarlo, non il programma.

## Cosa ne viene per noi (da discutere)

- **Motori**: la via di En Croissant è quella giusta e per noi è più corta.
  Stockfish lo includiamo già; per gli altri basta un catalogo JSON (sul
  nostro sito, generato da `site/`), lo scaricamento con `QNetworkAccessManager`
  su file (non in memoria), l'apertura di `.zip` e `.tar.gz` (manca in Qt: zlib
  c'è in QtCore ma l'API pubblica non apre gli archivi — una libreria piccola o
  il codice nostro), la scelta del binario per CPU (BMI2, AVX2: QSysInfo non lo
  dice, serve `cpuid`), i permessi, l'ingresso nella lista dei motori
  (`EngineCatalog`). Licenze: Stockfish GPL (lo distribuiamo già col sorgente),
  RubiChess GPL, Komodo e Dragon gratuiti ma non liberi — da valutare se
  offrirli.
- **Database**: la sua scelta (convertire una volta, ospitare i file pronti)
  si adatta bene a noi: abbiamo il convertitore e ora i database grandi
  reggono (docs/tech/large-databases.md). Restano da decidere le **licenze**
  di ciascuna raccolta (Lumbra's GigaBase, Caissabase, MillionBase hanno
  condizioni diverse: verificare che la ridistribuzione sia permessa), e
  l'**ospitalità**: le release di GitHub accettano file fino a 2 GB (Lumbra's
  supera), Cloudflare R2 no limiti e nessun costo di traffico. Con le mosse
  salvate una volta sola (piano 4 di large-databases.md) i nostri `.pdb`
  sarebbero più piccoli dei loro `.db3`.
- **Problemi di lichess**: li abbiamo già (Allenati sui finali, Allenati sulla
  tattica, una selezione); la raccolta completa (5 milioni di problemi, CC0) si
  potrebbe offrire come database scaricabile allo stesso modo.
- **Motori remoti** (il cloud di lichess): un'idea a parte, utile per chi ha
  un computer lento.

## Fonti

- Codice: `src/utils/engines.ts`, `src/utils/db.ts`,
  `src/components/engines/AddEngine.tsx`, `src/components/databases/AddDatabase.tsx`,
  `src-tauri/src/fs.rs` nel repository di En Croissant (ramo master, 2026-10-11)
- Lumbra's GigaBase: <https://lumbrasgigabase.com/en/> (licenza nel testo e nel piè di pagina)
- Cataloghi: <https://encroissant.org/engines?os=linux&bmi2=true>,
  <https://encroissant.org/databases>, <https://encroissant.org/puzzle_databases>
- I file: le intestazioni HTTP di `db.encroissant.org` (server Cloudflare,
  ETag di caricamento a più parti) e i primi byte (`SQLite format 3`)
