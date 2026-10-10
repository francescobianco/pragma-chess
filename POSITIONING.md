# Posizionamento: Pragma Chess ed En Croissant

Studio della presenza in rete di [En Croissant](https://github.com/franciscoBSalgueiro/en-croissant),
il programma libero più vicino a Pragma Chess, per capire come è cresciuto e
costruire il nostro posizionamento ripercorrendo le sue orme dove hanno senso.
Dati raccolti il 2026-10-10; le azioni che ne vengono vanno in
[DISTRIBUTING.md](DISTRIBUTING.md).

## 1. Chi è

| | En Croissant | Pragma Chess |
|---|---|---|
| Slogan | *The Ultimate Chess Toolkit* | *Your chess, in one place* — il database che spiega il motore |
| Promessa | "open-source, cross-platform chess GUI that aims to be powerful, customizable and easy to use"; "all the nice features of Lichess, Chess.com and ChessBase combined into a single app" | leggero, semplice, con ciò che serve davvero; **Spiega**: il perché della valutazione sulla scacchiera |
| Tecnologia | Tauri (Rust + React/TypeScript): un'app web dentro una finestra | Qt 6 Widgets, C++: un'app nativa del desktop |
| Licenza | GPL-3.0 | MIT |
| Nato | settembre 2022, prima release marzo 2023 (0.1.0) | settembre 2026 (0.1.0) |
| Versione | 0.15.1 (settembre 2026) | 0.3.0 (ottobre 2026) |
| Piattaforme | Windows, macOS (Intel e Apple Silicon), Linux (deb, rpm, AppImage) | Windows, macOS (Apple Silicon), Linux (deb, rpm), **Android** |
| Lingue | inglese (più traduzioni nell'app) | inglese e italiano, sito compreso |

## 2. I numeri

| | En Croissant | Pragma Chess |
|---|---|---|
| Stelle su GitHub | 1 933 | 2 |
| Fork | 328 | 0 |
| Contributori | 75 | 1 |
| Issue aperte | 198 | 0 |
| Membri Discord | 1 427 (148 online) | — |
| Scaricamenti, tutte le release | circa 210 000 installer (1,77 milioni di file contando l'aggiornamento automatico) | 76 |
| Installazioni vive (stima) | il file dell'aggiornamento automatico (`latest.json`) è stato letto 109 000 volte per la sola 0.15.1 | — |
| Ultima release, per sistema | Windows 68%, macOS 13%, Linux 19% (deb 1 544, AppImage 1 201, rpm 426) | Windows 25, Android 16, macOS 2, Linux 2 |
| AlternativeTo | 9 "mi piace", 4,8/5, 10 alternative — **Pragma Chess è già elencato tra queste** | online, alternativa a 7 programmi |

## 3. Come è cresciuto: la cronologia

Scaricamenti degli installer per release (la curva dell'adozione):

| Quando | Release | Scaricamenti | Cosa è successo |
|---|---|---|---|
| mar 2023 | 0.1.x | 50–120 | annuncio su **TalkChess**, "En Croissant - a new chess GUI", scritto dallo sviluppatore: 22 risposte, lodi all'aspetto, un bug, richieste |
| apr–set 2023 | 0.2–0.5 | 40–230 | un anno di release frequenti, quasi nessuno lo sa |
| ott 2023 – gen 2024 | 0.6–0.8 | ~1 000 | prime citazioni nei forum (chess.com, open-chess.org) |
| **10 feb 2024** | 0.9.x | 1 800 → **7 000** | **post sul blog di lichess** dello sviluppatore, "En Croissant: The Ultimate Chess Toolkit": **27 660 visite, 1 140 "mi piace"**. Una riga di presentazione, nove punti di funzioni, un link. È la svolta |
| mar 2024 | 0.10.0 | **19 500** | l'onda del post; secondo post su lichess, l'allenatore delle aperture |
| set 2024 | 0.11.1 | **82 000** | resta l'ultima per 14 mesi: le persone continuano ad arrivare (passaparola, AlternativeTo, Softpedia, recensione LinuxLinks, pacchetti AUR fatti da altri) |
| 2025–2026 | 0.12–0.15 | 6 000–45 000 | crescita costante; l'aggiornamento automatico tiene gli utenti |
| set 2026 | 0.15.1 | 17 700 in un mese | citato su **Hacker News** nei commenti ("TIL: En Croissant"), mai con un "Show HN" proprio |

**La lezione**: per un anno le release non sono bastate. Ha fatto la differenza
**un solo post, dello sviluppatore, nel posto dove stanno i giocatori** (il blog
di lichess), con un'immagine e un elenco chiaro di cose utili. Da lì in poi
sono cresciuti da soli il passaparola, gli elenchi e i pacchetti fatti da altri.

## 4. Dove si trova

| Canale | En Croissant | Pragma Chess |
|---|---|---|
| Sito | encroissant.org: una pagina di vetrina, Download, Docs, Support | yafb.net/pragma-chess: più completo (EN/IT, screenshot, video, blog, sostenitori) |
| Documentazione | sito di documentazione online (Getting started, guide per funzione) | la guida è dentro l'app (F1), in EN e IT; **niente documentazione online** |
| Comunità | **Discord** (1 427 membri), linkato dal README e dal sito | GitHub Discussions, gruppo WhatsApp dei circoli |
| Blog di lichess | 2 post dello sviluppatore (lancio, allenatore delle aperture) | 1 post in italiano; quello in inglese è pronto |
| TalkChess | thread di lancio dello sviluppatore | — |
| Forum chess.com | thread aperti dagli utenti | — |
| Reddit | nessun post ufficiale trovato | — |
| Hacker News | solo citazioni nei commenti | — |
| YouTube | nessun video ufficiale | un clip di 40 s sul sito |
| AlternativeTo | sì (4,8/5) | sì |
| Softpedia, LinuxLinks, KDE Store | sì (recensione LinuxLinks: "attractive interface, rough around the edges") | no |
| winget | sì (`EnCroissant`) | PR in attesa del moderatore |
| AUR | 3 pacchetti, **fatti da altri** (`en-croissant`, `-bin`, `-git`) | no |
| Flathub | **no** | manifest pronto |
| Homebrew | no | no (servono ~75 stelle) |
| Donazioni | pagina Support: Buy Me a Coffee ("Buy me a croissant"), PayPal | nessuna |
| Aggiornamento automatico | sì, nell'app | no |

## 5. Cosa offre e cosa gli manca

**I suoi punti forti** (quelli che i post e le recensioni citano):
- un'interfaccia che somiglia ai siti (lichess, chess.com): "user-friendly come le app web";
- **installazione con un clic** di motori e database (Stockfish, Lc0, database pronti come Caissabase);
- **rapporto della partita**: grafico della valutazione, mappa di calore della scacchiera, errori ed eccellenze;
- analisi con più motori insieme, anche il motore "cloud" di lichess;
- repertorio d'aperture allenato con la ripetizione dilazionata;
- 3 milioni di problemi di lichess, offline;
- statistiche delle aperture, storico del rating;
- import da lichess e chess.com.

**I suoi punti deboli** (detti dagli utenti):
- "la parte database è in generale molto limitata", manca l'unione delle partite (chi usa ChessBase, sul forum di chess.com);
- su Linux Tauri dà problemi ("neither ever works", su Hacker News), niente Flathub;
- non parte su Windows 7; "rough around the edges";
- nessuna spiegazione del *perché*: mostra valutazioni e mosse migliori, non le ragioni;
- solo inglese come prima lingua; nessuna app per il telefono.

## 6. Il nostro posizionamento

Non "un altro En Croissant": dove lui è *il coltellino svizzero che somiglia a
un sito*, Pragma Chess è **il database che spiega**, nativo, leggero e serio
coi dati.

1. **Spiega, non solo valuta.** È la differenza che nessuno ha: le frecce del
   perché, il tutor che ferma l'errore, il grafico della partita. Va in testa a
   ogni messaggio, con l'immagine dell'Opera di Morphy.
2. **Un database vero.** Milioni di partite misurate (docs/tech/large-databases.md),
   file ChessBase letti, ricerca per posizione e variante, cestino, unione tra
   copie, sincronizzazione. È proprio il punto debole che gli utenti di En
   Croissant segnalano: parliamo a chi viene da ChessBase e Scid.
3. **Nativo e leggero.** Qt, non un browser in una finestra: parte subito, poca
   memoria, aspetto del sistema, funziona bene su Linux (Wayland, GNOME).
4. **Per i circoli e per chi studia, anche in italiano.** Lingua, circoli
   sostenitori, scuole: una comunità che En Croissant non cura.
5. **Ovunque.** Desktop e telefono con gli stessi database; gioco su lichess e
   freechess.org; licenza MIT, più libera della GPL per chi vuole riusare.

In una riga: *Pragma Chess — il database di scacchi libero che ti spiega il
perché delle mosse.*

## 7. Le sue orme, in ordine

Quello che ha funzionato per lui, adattato a noi:

1. **Il post sul blog di lichess, in inglese** — la sua svolta. Il nostro è
   pronto (`packaging/announcements/lichess-blog-en.md`): immagine di Spiega,
   elenco di cose utili, un solo link. Pubblicarlo al più presto; poi un post
   per ogni funzione forte (il tutor, il grafico, i database grandi), come lui
   ha fatto con l'allenatore delle aperture.
2. **Il thread su TalkChess**, dello sviluppatore, tono amichevole, "work in
   progress, ditemi cosa non va": lì leggono autori di motori e di interfacce.
3. **Una comunità dove parlare**: lui ha Discord. Per noi: aprire un server
   Discord (o rafforzare le Discussions) e linkarlo da README, sito e app.
4. **Installazione con un clic di motori e database**: è la funzione che gli
   utenti citano per prima. Noi abbiamo già Stockfish incluso e il
   convertitore PGN: aggiungere un gestore che scarica e converte database
   pronti (lichess, Caissabase, i problemi) ci porta lì, con i nostri database
   grandi come vantaggio.
5. **Aggiornamento automatico**: tiene gli utenti e misura quanti sono (il
   suo `latest.json` letto 109 000 volte). Un controllo leggero delle release
   di GitHub all'avvio, con una notifica.
6. **Elenchi e pacchetti**: AlternativeTo (fatto), Softpedia e LinuxLinks
   (inviare), winget (in attesa), AUR (scrivere noi il PKGBUILD, lui l'ha
   avuto da altri), e **Flathub, dove lui non c'è**: per Linux è il nostro
   vantaggio da prendere.
7. **Documentazione online**: la guida esiste già dentro l'app; pubblicarla
   anche sul sito (stesse pagine, EN e IT) la rende trovabile dai motori di
   ricerca.
8. **Una pagina per sostenere il progetto** (GitHub Sponsors, Buy Me a
   Coffee): lui la linka dal README.
9. **Hacker News** con un "Show HN" vero, che lui non ha mai fatto: ci arriva
   gente che il blog di lichess non raggiunge.

## Fonti

- Repository e release: <https://github.com/franciscoBSalgueiro/en-croissant> (API di GitHub, 2026-10-10)
- Sito: <https://encroissant.org>, <https://www.encroissant.org/support>, <https://www.encroissant.org/docs>
- Blog di lichess: <https://lichess.org/@/FrankWillow/blog/en-croissant-the-ultimate-chess-toolkit/GwYz71GJ>, <https://lichess.org/@/FrankWillow/blog/training-openings-with-en-croissant/r8PrLTGW>
- TalkChess: <https://talkchess.com/viewtopic.php?p=945316>
- Forum chess.com: <https://www.chess.com/forum/view/chess-equipment/en-croissant>
- AlternativeTo: <https://alternativeto.net/software/en-croissant/about/>
- LinuxLinks: <https://www.linuxlinks.com/en-croissant-gui-chess-toolkit/>
- Softpedia: <https://www.softpedia.com/get/Gaming-Related/En-Croissant.shtml>
- KDE Store: <https://store.kde.org/p/2127170>
- Hacker News (ricerca Algolia), AUR (API), winget-pkgs, Discord (invito), Flathub (API)
