# Per iniziare {#start}

Pragma Chess conserva le tue partite in **database** e te le fa studiare sulla scacchiera con un motore, un libro delle aperture e le tue annotazioni.

La finestra ha la **scacchiera** al centro e quattro pannelli attorno:

- **Partite**, in basso: l'albero del database aperto e la lista delle sue partite.
- **Mosse**: le mosse della partita sulla scacchiera.
- **Albero delle aperture**: le mosse del libro per la posizione sulla scacchiera.
- **Motore**: la valutazione, la linea migliore e l'apertura in cui si trova la partita.

La **barra degli strumenti** ha Sincronizza, Salva progetto, Nuova partita, Nuovo allenamento, Nuova partita online e tre icone per scegliere il libro delle aperture, il motore e il database in uso.

Tutto quello che vedi — database, partita, mossa, pannelli — è un **progetto**: la prossima volta che avvii Pragma Chess lo ritrovi come l'hai lasciato.

I tuoi file stanno nella cartella Pragma dentro la tua cartella degli scacchi (per esempio `Scacchi/Pragma` nella tua home), con `Databases`, `Projects` e `Books`. Al primo avvio ci trovi un database di partite classiche.

**Opzioni ▸ Impostazioni personali…** dice chi sei: il tuo nome, il tuo anno di nascita e il tuo ID FIDE. Il tuo nome va dalla tua parte in una nuova partita, in una partita da Inserisci posizione (dal lato in basso della scacchiera) e in un allenamento (col colore che hai scelto), a meno che il database aperto ti conosca già — vince il giocatore segnato come Io con **Chi è?**. **Stile della scacchiera** sceglie insieme i colori delle case e i pezzi: Pragma Classic (case marroni e i pezzi dei libri di scacchi) Lichess Alpha (la scacchiera verde e i pezzi Alpha di lichess.org) o Classic Book (il diagramma di un libro di scacchi stampato: carta antica, le case scure tratteggiate a inchiostro, un margine di carta attorno a ogni pezzo che vi sta sopra, e nessuna coordinata); tutte le scacchiere cambiano appena premi OK. Queste impostazioni stanno in `.pragma-chess.conf`, un file YAML nella tua cartella Pragma, che la sincronizzazione porta sugli altri tuoi computer anche se è nascosto; cambiato su due computer insieme, vince il più recente.

**Opzioni ▸ Impostazioni cartelle…** le sposta altrove su questo computer: la cartella Pragma stessa, oppure solo i database, i progetti, i libri o i nomi delle aperture. Lascia vuota una cartella per tenerla al suo posto solito. I file che ci sono già non vengono spostati, e le nuove cartelle si usano dal prossimo avvio di Pragma Chess. La sincronizzazione tiene uguale sugli altri computer solo ciò che sta dentro la cartella Pragma.

# Database {#databases}

Un database è un file `.pdb` che contiene partite. Se ne apre uno alla volta; il suo nome è nel suggerimento dell'icona del database nella barra degli strumenti.

- **Database ▸ Nuovo database…** ne crea uno vuoto nella cartella dei database.
- **Database ▸ Apri database…** apre un file da qualunque posizione.
- **Database ▸ Cambia database** elenca i database della cartella: scegline uno per aprirlo. L'icona del database nella barra degli strumenti apre lo stesso elenco.
- **Database ▸ Impostazioni database…** modifica il nome e la descrizione, dice se il database è una raccolta di partite o un libro d'aperture, e contiene **Ottimizza database**.
- **Database ▸ Salva database come…** ne scrive una copia.
- **Database ▸ Cambia database ▸ Mostra cartella dei database** apre la cartella nel gestore dei file.

Le modifiche a un database vengono scritte mentre le fai: non c'è niente da salvare a mano.

# La lista delle partite {#games-list}

La lista mostra le partite del database aperto, una per riga. Fai doppio clic su una partita per metterla sulla scacchiera.

- Clicca il titolo di una colonna per **ordinare**; clicca ancora per invertire.
- Trascina il titolo di una colonna per **spostarla**.
- Clic destro sul titolo di una colonna per **nasconderla**, o per **mostrare** una colonna nascosta. Ogni database ricorda le sue colonne.
- L'ultima colonna, **Linea**, mostra come comincia la partita; è tagliata con «…» dove finisce la colonna.

Clic destro su un giocatore per dire **chi è**: tu, un amico o un avversario. L'albero elenca poi quei giocatori sotto Io, Amici e Avversari, il tuo nome è in grassetto nella lista, e una partita in cui giochi tu si apre con la scacchiera girata dalla tua parte.

Clic destro su una partita per metterla nel **cestino**.

Per cambiare giocatori, evento, data o risultato della partita sulla scacchiera, clicca l'intestazione sopra la scacchiera.

# L'albero del database {#database-tree}

A sinistra della lista delle partite, l'albero mostra che cosa contiene il database aperto. Seleziona un nodo e la lista mostra solo quelle partite; seleziona il database per vederle tutte.

- **Scacchiera ▸ Posizione**: le partite in cui compare la posizione sulla scacchiera, qualunque sia l'ordine delle mosse.
- **Scacchiera ▸ Variante**: le partite che cominciano esattamente con le mosse giocate sulla scacchiera.
- **Io**, **Amici**, **Avversari**: i giocatori che hai indicato con «Chi è?».
- **ECO**: le partite per codice d'apertura.
- **Tornei** e **Anni**.
- **Sorgenti**: le partite arrivate da lichess.org, chess.com o torneionline.com.
- **Cestino**: le partite che hai buttato.

Posizione e Variante seguono la scacchiera: scorri una partita e i loro conteggi cambiano.

# La scacchiera {#board}

Muovi un pezzo trascinandolo, oppure cliccandolo e poi cliccando la casa d'arrivo. Un pedone che arriva in fondo chiede a che cosa promuovere.

- **Sinistra** e **Destra** vanno indietro e avanti di una mossa, **Home** e **Fine** all'inizio e alla fine. Cliccando una mossa nel pannello Mosse si va lì.
- **Visualizza ▸ Gira scacchiera** (Ctrl+R) gira la scacchiera; **Visualizza ▸ Mostra coordinate** mostra o nasconde lettere e numeri.
- **Opzioni ▸ Impostazioni grafiche…** sceglie l'**Aspetto** — Segui il sistema (il predefinito), oppure sempre Chiaro o sempre Scuro, qualunque cosa dica il sistema —, dove mostrare i pezzi catturati, se indicare a chi tocca e se le mosse si sentono: **Suono quando si muove un pezzo** fa sentire il colpo di un pezzo appoggiato sulla scacchiera per le tue mosse, per quelle del motore in allenamento e per quelle del tuo avversario online (queste ultime quando arrivano sulla casa). Queste impostazioni restano su questo computer: ognuno può accordarsi al proprio desktop.
- Quando la posizione sulla scacchiera è **scacco matto**, il bordo della scacchiera diventa rosso.

**Partita ▸ Nuova partita** (Ctrl+Shift+N) comincia una partita da inserire mossa per mossa. **Partita ▸ Salva partita nel database** la mette nel database aperto; **Partita ▸ Salva partita in un altro database…** la mette in un database a tua scelta, che non viene aperto.

Gioca una mossa che non è la successiva della partita e diventa una **variante**: la partita tiene la sua linea, e quella nuova compare nel pannello Mosse sotto la mossa che sostituisce. Una partita salvata nel database viene salvata subito, varianti comprese.

**Partita ▸ Inserisci posizione…** apre una scacchiera su cui disegnare una posizione: scegli un pezzo a destra e fai clic sulle case per posarlo (un clic sullo stesso pezzo lo toglie), trascina un pezzo per spostarlo dove vuoi sulla scacchiera (lasciato fuori, torna al suo posto), fai clic col tasto destro su una casa per svuotarla. Imposta il tratto, gli arrocchi, l'en passant e il numero di mossa, o scrivi un FEN; aiutano **Posizione iniziale**, **Svuota scacchiera** e **Gira scacchiera**. OK si attiva solo per una posizione da cui può cominciare una partita, e il testo sotto la scacchiera dice cosa non va. La posizione comincia una nuova partita in fondo al capitolo, così la partita su cui eri resta com'è. Una partita online in corso viene prima continuata o abbandonata.

**Modifica ▸ Copia** mette negli appunti le mosse, la partita come PGN, la posizione come FEN (Ctrl+Shift+C), la linea del motore o la spiegazione. **Modifica ▸ Incolla** prende quello che c'è negli appunti: **Incolla FEN** (Ctrl+Shift+V) imposta la posizione, **Incolla linea** comincia una nuova partita con le mosse (PGN, mosse con o senza numeri, o UCI), e **Incolla linea dalla posizione attuale** (Ctrl+Alt+V) gioca le mosse dalla posizione sulla scacchiera, come se le giocassi tu: alla fine della partita vengono aggiunte, altrove diventano una variante.

# Mosse e annotazioni {#moves}

Il pannello Mosse elenca la partita sulla scacchiera, con i pezzi disegnati come figurine. Clicca una mossa per andarci.

La linea principale è una tabella, una mossa per cella: clicca la cella per andare alla mossa. Le **varianti** hanno una riga tutta loro sotto la mossa che sostituiscono, più piccole, come testo; le varianti di una variante la seguono tra parentesi. Clicca una mossa di una variante per seguire quella linea: le frecce poi si muovono lungo di essa, e cliccando una mossa della linea principale si torna indietro. All'inizio di una variante, giocare la mossa della linea principale riprende la linea principale, e un'altra mossa apre una variante sorella. Le varianti sono salvate con la partita e viaggiano nel PGN che copi.

Clic destro su una mossa per il suo menu:

- **Copia ▸ Copia mossa** e **Copia ▸ Copia linea fino a qui** mettono negli appunti la mossa, o la partita fino a lì.
- **Annotazioni** elenca i simboli con il significato di ciascuno.

Una mossa può avere un giudizio sulla mossa e una valutazione della posizione:

- `!!` mossa brillante, `!` buona mossa, `!?` mossa interessante, `?!` mossa dubbia, `?` errore, `??` errore grave, `□` mossa unica.
- `+−` il Bianco è in vantaggio decisivo, `±` il Bianco sta meglio, `⩲` il Bianco sta leggermente meglio, `=` posizione pari, `∞` posizione poco chiara, `⩱` `∓` `−+` lo stesso per il Nero.

Scegli il simbolo che la mossa ha già per toglierlo, oppure **Nessuna annotazione** per toglierli tutti. Le annotazioni sono salvate subito con la partita e vengono scritte quando copi la partita come PGN.

I **commenti** — il testo tra le mosse di una partita PGN o di uno studio lichess — sono in corsivo: sotto la loro mossa nella linea principale, tra le mosse in una variante. I comandi che i programmi mettono nei commenti (una valutazione `[%eval 0.18]`, un orologio, delle frecce) restano con la partita ma non si vedono. I commenti si salvano nel database con la partita e si riscrivono quando la partita viene scritta in PGN.

# Spiega {#explain}

**Spiega** mostra sulla scacchiera perché l'ultima mossa è buona o cattiva. Premi il bottone tra le frecce sotto la scacchiera, oppure il tasto **E**.

Il motore guarda la posizione prima e dopo la mossa e disegna delle frecce:

- frecce **rosse**: sta per cadere del materiale, e i pezzi che si perdono sono cerchiati;
- frecce **blu**: la risposta che fa la differenza, quando ancora non si perde niente.

Un matto forzato viene giocato sulla scacchiera, dentro una cornice rossa. Il pannello Motore lo dice a parole, per esempio «Errore grave (+0.3 → −2.9). Il Nero vince un cavallo».

Mentre il motore cerca, il bordo della scacchiera pulsa; diventa blu quando la spiegazione è pronta. Spiega riguarda una mossa sola: andando a un'altra mossa si spegne, e la chiedi di nuovo.

**Modifica ▸ Copia ▸ Spiegazione** copia il testo.

# Motori {#engine}

**Motore ▸ Analisi** (Ctrl+E) accende e spegne l'analisi della posizione sulla scacchiera. Il pannello Motore mostra il punteggio — sempre dal lato del Bianco: positivo è buono per il Bianco —, la profondità e la linea migliore. La barra accanto alla scacchiera mostra lo stesso punteggio.

Pragma Chess arriva con Stockfish, e funziona con qualunque motore UCI.

- **Motore ▸ Cambia motore** sceglie tra i motori di questo computer. L'icona del motore nella barra degli strumenti apre lo stesso elenco.
- **Motore ▸ Gestisci motori…** aggiunge, modifica e rimuove motori. **Rileva motori** trova quelli installati sul computer; **Usa questo motore** passa a quello selezionato.

Il motore in uso fa parte del progetto.

# Allenamento {#training}

**Partita ▸ Nuovo allenamento…** (Ctrl+Shift+T) comincia una partita contro il motore. Scegli Bianco, Nero o Casuale e premi Inizia.

Finché tocca a te il motore nasconde la sua linea migliore e mostra solo il punteggio. Quando hai mosso risponde da solo, lentamente, così vedi la sua mossa.

Finché la posizione è nel **libro di aperture**, il motore risponde con una mossa del libro, scegliendo ciascuna tanto spesso quanto dice il suo peso: regola i pesi nell'Albero delle aperture per allenarti contro le linee che vuoi, quanto vuoi. Fuori dal libro gioca la sua mossa migliore.

Il **tutor** guarda le tue mosse. Quando una è un'**imprecisione**, un **errore**, un **errore grave** o un'**occasione mancata**, il motore non risponde; il bordo della scacchiera diventa rosso, il pannello Motore dice che cosa è successo e offre:

- **Ritira la mossa**: torni alla posizione e provi un'altra mossa;
- **Spiega**: mostra sulla scacchiera perché è un errore;
- **Ignora**: tieni la mossa, e il motore risponde.

Scacco matto o stallo chiudono la partita, che viene salvata nel database aperto.

Spunta **Ricorda per questa sessione** nella finestra Nuovo allenamento e il bottone della barra degli strumenti comincerà i prossimi allenamenti con la stessa scelta, senza chiedere. Il menu chiede sempre.

**Motore ▸ Modalità allenamento** accende o spegne l'allenamento per la partita sulla scacchiera. Aprire una partita dalla lista lo spegne. Se chiudi Pragma Chess mentre ti alleni, riparte in allenamento.

# Giocare online {#online}

**Partita ▸ Nuova partita online…** gioca una partita contro una persona su lichess.org (altre piattaforme seguiranno).

La finestra elenca le **piattaforme a cui sei connesso**, ciascuna con l'account con cui giochi. **Connetti piattaforma…** chiede di che tipo è, apre nel browser la sua pagina di accesso e porta qui la connessione; **Disconnetti** ne toglie una. Le connessioni sono tue su questo computer, conservate con le tue impostazioni, mai in un progetto. Scegli la connessione, l'**orologio** (minuti e incremento), il colore e se la partita è **classificata**, poi **Cerca un avversario**.

Mentre Pragma Chess cerca un avversario e mentre giochi, è in **modalità gioco online**, spuntata in **Motore ▸ Modalità gioco online** (sceglierla avvia o interrompe il gioco online): il motore, Spiega e la Modalità allenamento sono spenti e non si possono accendere, e l'Albero delle aperture resta dove l'hai messo ma non elenca mosse, e la sua prima riga dice perché — sei tu contro il tuo avversario. Il pannello Motore mostra i nomi, i punteggi, gli orologi e a chi tocca. Le tue mosse vanno alla piattaforma appena le fai; quelle dell'avversario scivolano sulla scacchiera. Si gioca solo la posizione in corso: puoi riguardare le mosse precedenti, e tornare alla fine per muovere. Se Pragma Chess si chiude durante una partita, al successivo avvio si ricollega e la partita continua da dove è (l'orologio, sulla piattaforma, intanto è andato avanti); una partita finita nel frattempo viene salvata con il suo risultato. Puoi cambiare database mentre giochi: la partita resta sulla scacchiera e, quando finisce, viene salvata nel database aperto in quel momento.

Spunta **Ricorda per questa sessione** e il bottone Gioca online della barra degli strumenti cerca un avversario con le stesse scelte senza chiedere; il menu chiede sempre.

**Partita ▸ Nuova partita online…** durante una partita ne inizia una nuova online, **Partita ▸ Nuova partita** chiede se vuoi una nuova partita online (come Nuova partita online nella barra degli strumenti) o una nuova partita da analizzare — con **Ricorda per questa sessione** non lo chiede più finché Pragma Chess resta aperto —, e **Partita ▸ Nuovo allenamento…** una partita contro il motore; ciascuna chiede prima se **continuare a giocare** la partita in corso o **abbandonarla** (se si sta ancora cercando un avversario, la ricerca si interrompe e basta). Scegliere **Motore ▸ Modalità gioco online** quando è attiva interrompe la ricerca, o abbandona la partita dopo averlo chiesto. Quando la partita finisce — scacco matto, abbandono, tempo, patta — il risultato viene scritto e la partita è salvata nel database aperto, con i giocatori, i loro punteggi e un collegamento alla partita.

# Libri delle aperture {#books}

Un libro delle aperture è un file Polyglot `.bin`: le mosse conosciute in ogni posizione, ciascuna con un peso. L'icona del libro nella barra degli strumenti apre l'elenco dei libri della tua cartella dei libri.

- **Libro ▸ Nuovo libro…** e **Libro ▸ Apri libro…** creano un libro o ne aprono uno da qualunque posizione.
- **Libro ▸ Cambia libro** elenca i libri della cartella: scegline uno per usarlo, oppure **Nessun libro** per lavorare senza. **Mostra cartella dei libri**, in fondo, apre la cartella nel gestore dei file.

Il pannello **Albero delle aperture** mostra, per la posizione sulla scacchiera, ogni mossa del libro con la sua parte del peso, il nome dell'apertura a cui porta e come sono andate dopo quella mossa le partite del database aperto (partite, poi vittorie del Bianco / patte / vittorie del Nero). Clicca una mossa per giocarla; la prima riga ritira l'ultima mossa.

**Il tuo repertorio**: clic destro su una mossa dell'Albero delle aperture e scegli **Metti nel repertorio**. Le mosse del repertorio sono elencate per prime, in grassetto. Il segno è scritto nel file del libro e gli altri programmi lo ignorano.

**I pesi**: lo stesso menu ha **Regola peso**, con +5%, +10%, +25%, +50%, +100%, gli stessi in negativo e **Azzera peso**. La percentuale è della quota della mossa stessa, e la somma della posizione resta sempre 100%: quel che una mossa guadagna lo cedono le altre in proporzione a quanto hanno — le più pesanti di più — e quel che perde torna a loro allo stesso modo. Una mossa allo 0%, o con meno dell'1%, non può crescere per percentuale di sé stessa, quindi un aumento prima le ruba l'1% dalle altre e cresce da lì; una diminuzione di una mossa allo 0% non fa nulla. Azzera peso dà tutta la quota della mossa alle altre che ne hanno. Le modifiche sono scritte nel libro in uso. L'elenco viene riordinato, e la mossa che hai cambiato brilla per un momento ed è segnata, a sinistra del suo peso, con ↑ se è salita, ↓ se è scesa, = se è rimasta dov'era; il segno resta finché non lasci la posizione.

# Nomi delle aperture {#opening-names}

Il pannello Motore dice in quale apertura si trova la partita, e l'Albero delle aperture dice dove porta ogni mossa.

I nomi vengono da un database di linee con un nome. **Opzioni ▸ Cambia nomi delle aperture** sceglie quale: inglese, italiano o nessuno. Finché non scegli, i nomi seguono la lingua dell'interfaccia.

Un database dei nomi è un normale database di tipo Libro d'aperture: puoi aprirlo e aggiungere le tue linee, mettendo il nome nel campo Evento e il codice in ECO.

# Sorgenti di partite {#sources}

Una sorgente porta nel database aperto le tue partite da un sito e le tiene aggiornate.

**Database ▸ Collega sorgente…** ne aggiunge una:

- **lichess.org**: le partite pubbliche dell'account; l'accesso è facoltativo e le scarica più in fretta;
- **chess.com**: il tuo nome utente;
- **torneionline.com**: il tuo numero FIDE o FSI, per le partite dei tornei che hai giocato.
- **File ChessBase**: un database ChessBase (`.cbh` e i suoi file) su questo computer. Scegli il file `.cbh`: le sue partite vengono copiate, i file restano dove sono e le partite aggiunte in seguito arrivano alla sincronizzazione successiva. Su un altro computer il file non c'è: la sincronizzazione lo dice e propone di ignorare la sorgente su quel computer; *Database ▸ Gestisci sorgenti… ▸ Modifica…* sceglie di nuovo il file.
- **Lichess Study**: uno studio su lichess.org. Incolla l'indirizzo della sua pagina (o di uno dei suoi capitoli) in **Indirizzo dello studio**: ogni capitolo diventa una partita del database, con i suoi commenti e le sue varianti, e i suoi tag `StudyName`, `ChapterName` e `ChapterURL` dicono di quale studio e capitolo si tratta (i capitoli dello studio non sono i capitoli di un progetto). Un capitolo cambiato su lichess.org sostituisce la sua partita alla sincronizzazione successiva, a meno che la partita sia cambiata anche qui: allora si tengono tutte e due. Uno studio pubblico non chiede un account; per uno privato, **Accedi con lichess.org…** con un account che può vederlo. Per ora lo studio viene solo letto: la scrittura è in arrivo.
- **File PGN**: un file `.pgn` su questo computer, tenuto al passo con il database. Sceglilo con **Sfoglia…**, o creane uno vuoto con **Nuovo file…**, poi la **Direzione**: **Leggi e scrivi** (le partite del file entrano nel database e quelle del database vanno nel file, così una partita aggiunta o modificata da una parte arriva all'altra), **Solo lettura** (il file non viene mai scritto) o **Solo scrittura** (tutte le partite del database vanno nel file; le altre partite del file restano fuori dal database). Una partita che salvi, modifichi o annoti viene scritta nel file pochi secondi dopo. Niente di ciò che togli da una parte viene tolto dall'altra, e una partita modificata da entrambe le parti tra due sincronizzazioni viene tenuta due volte. Pragma Chess segna ogni partita che lega al database con un tag `PragmaUid`, e tiene un indice accanto al file in un file nascosto, così un file che non è cambiato non viene riletto.

Le sorgenti vengono lette quando il database viene aperto e ogni venti minuti. Una partita non viene mai importata due volte. **Database ▸ Gestisci sorgenti…** sincronizza subito una sorgente, la modifica, rifà l'accesso o la rimuove; le partite già importate restano.

Il nodo **Sorgenti** dell'albero elenca le partite di ogni sorgente.

# Cestino {#trash}

Clic destro su una partita della lista e scegli **Cestina partita**. La partita esce da tutte le liste e non viene più cercata.

Il nodo **Cestino**, ultimo dell'albero, mostra le partite cestinate: **Recenti**, buttate negli ultimi sette giorni, e **Vecchie**. Lì puoi scegliere **Ripristina partita** o **Elimina partita…**.

Eliminare non rimpicciolisce ancora il file. **Database ▸ Impostazioni database… ▸ Ottimizza database** toglie per sempre le partite eliminate e compatta il file. Fino ad allora non si perde niente.

# Progetti {#projects}

Un progetto è quello che stai guardando: il database, la partita e la mossa, il lato da cui vedi la scacchiera, il motore, i pannelli e se ti stai allenando. La barra del titolo ne mostra il nome — quello del file, o quello dato in **File ▸ Impostazioni progetto…** — con un asterisco quando ha modifiche non salvate, e il capitolo aperto quando sono più d'uno: *Aperture* - L'Italiana - Pragma Chess*.

- **File ▸ Nuovo progetto** tiene quello che vedi — database, motore, pannelli — e comincia una partita nuova, vuota, col Bianco in basso.
- **File ▸ Apri progetto…** e **File ▸ Apri progetto recente** aprono un file `.pch`.
- **File ▸ Salva progetto** e **File ▸ Salva progetto come…** lo salvano.

Non sei obbligato a salvare: Pragma Chess si riapre come l'hai chiuso.

I pannelli si possono ridimensionare e chiudere; il menu **Visualizza** (Mosse, Albero delle aperture, Motore, Lista partite) li mostra di nuovo, e **Visualizza ▸ Ripristina disposizione dei pannelli** li rimette dove stanno all'inizio.

# Capitoli e paragrafi {#chapters}

Un progetto è una raccolta di **capitoli**, come uno studio o un libro di scacchi, e un capitolo contiene partite una dopo l'altra, con del testo tra le loro mosse. La lista delle mosse mostra tutto il capitolo: una riga leggera segna dove comincia ogni partita, e la numerazione riparte; fai clic su una mossa di un'altra partita e la scacchiera va lì.

Fai clic col tasto destro sulla lista delle mosse — su una mossa, su un paragrafo, o dove vuoi, anche senza mosse — per:

- **Inserisci ▸ Paragrafo**: un paragrafo dopo la mossa (o dove si trova la scacchiera), scritto direttamente nella lista delle mosse. Scrivi come in un libro: il testo è giustificato, e ogni a capo comincia un nuovo capoverso, rientrato. **Esc**, **Ctrl+Invio** o un clic altrove per finire; un paragrafo lasciato vuoto sparisce. Fai clic su un paragrafo per riscriverci; **Modifica paragrafo**, **Sposta** ed **Elimina paragrafo** sono nel suo menu: Sposta lo porta **In testa** o **In fondo** alla partita, oppure **Su** e **Giù** di una semimossa alla volta — dopo la mossa del Bianco, Giù lo porta dopo quella del Nero, che torna sulla riga del Bianco. I paragrafi vanno sulla linea principale.
- **Inserisci ▸ Titolo** e **Inserisci ▸ Sottotitolo**: un'intestazione dove andrebbe un paragrafo, per dividere un capitolo come fa un libro: il titolo in grassetto e centrato, il sottotitolo in grassetto, un po' più piccolo e a sinistra. Si scrivono, si spostano e si eliminano come i paragrafi.
- **Sposta**: sposta quello su cui hai fatto clic. Su un titolo, un sottotitolo o un paragrafo sposta quello, come sopra; su una mossa, o altrove, sposta l'intera partita **In testa** o **In fondo** al capitolo, oppure **Su** e **Giù** oltre la partita sopra o sotto — le interruzioni tra le partite si adeguano; la scacchiera resta sulla partita che mostra.
- **Inserisci ▸ Interruzione partita**: una nuova partita dalla posizione iniziale, con la numerazione che riparte da 1. Dopo ogni partita tranne l'ultima c'è già un'interruzione, quindi la nuova partita va in fondo al capitolo; un'interruzione rimasta senza mosse sparisce quando passi a un'altra partita.

Nuova partita, Nuovo allenamento, Inserisci posizione, gli Incolla e le partite online aggiungono la loro partita in fondo al capitolo, e così una partita aperta dalla lista delle partite (una che il capitolo ha già viene semplicemente mostrata). Le partite salvate in un database vengono salvate lì man mano che cambiano; le altre, e tutti i paragrafi, vengono salvati con il progetto.

Il menu **File** ha i capitoli: **Nuovo capitolo…**, **Cambia capitolo** per scegliere quello aperto, e **Gestione capitoli…** per riordinarli (trascinando, o con Sposta su e Sposta giù), rinominarli, aggiungerli ed eliminarli. **Impostazioni progetto…** dà al progetto un nome suo.

# Sincronizzazione {#sync}

La sincronizzazione tiene uguale la tua cartella Pragma — database e progetti — su più computer, attraverso una cartella su un server.

**Opzioni ▸ Impostazioni di sincronizzazione…** la imposta: un server **FTP** (anche con TLS), un server **WebDAV** o un **repository Git**. **Prova connessione** la verifica.

**File ▸ Sincronizza ora** (Ctrl+Y, anche primo bottone della barra degli strumenti) fa tutto in ordine: legge le sorgenti, salva il progetto e scambia i file con il server. **Sincronizza prima di chiudere** lo fa ogni volta che esci.

La sincronizzazione non elimina niente di sua iniziativa: un database che manca da una parte viene copiato lì, e un database modificato su due computer viene unito partita per partita. Per liberarti di una partita usa il cestino, che gli altri computer seguono.

Ogni computer ricorda cosa ha sincronizzato l'ultima volta in un file nascosto della sua cartella Pragma, `.pragma-chess.local`, che non va mai sul server. Così distingue un file che hai eliminato a mano — dal file manager, per esempio — da uno che deve ancora ricevere: alla sincronizzazione successiva ti chiede cosa farne: **Elimina ovunque**, **Ripristina** o **Chiedimelo più tardi** (te lo richiede al prossimo avvio di Pragma Chess). Eliminato ovunque, il file viene tolto dal server e ogni altro computer sposta la sua copia nel cestino alla sincronizzazione successiva. Un file che nel frattempo qualcuno ha modificato su un altro computer torna semplicemente indietro.

**Gestisci file…**, nelle Impostazioni di sincronizzazione, elenca i file della cartella sul server. Selezionane alcuni e premi **Elimina…** per fare pulizia: dopo la conferma vengono eliminati da tutti i dispositivi sincronizzati, compreso questo computer (nel cestino), e smettono di girare tra i tuoi computer.

Con un repository Git ogni sincronizzazione che cambia dei file fa un commit, che porta il loro nome ("Update Databases/Games.pdb; add Projects/Study.pch"): una sincronizzazione che riceve soltanto, o non trova niente di nuovo, lascia la storia com'è.

**Opzioni ▸ Collega app mobile…** mostra un codice da inquadrare con Pragma Chess sul telefono, che da quel momento tiene una copia dei tuoi database.

Quando elimini un database sul telefono, il computer ti chiede alla sincronizzazione successiva con il telefono se eliminarlo anche qui o tenerlo: **Tienilo** lo lascia sul computer, **Chiedimelo più tardi** te lo chiede di nuovo al prossimo avvio di Pragma Chess, ed **Elimina ovunque…** ti avvisa prima che il database sarà eliminato da tutti i dispositivi sincronizzati — finisce nel cestino di questo computer, viene tolto dalla cartella di sincronizzazione sul server e gli altri computer che si sincronizzano con essa eliminano la loro copia. In ogni caso il telefono non lo riceve più.

# Lingua {#language}

**Opzioni ▸ Cambia lingua** sceglie la lingua dell'interfaccia. Viene applicata al prossimo avvio di Pragma Chess. Questa guida e i nomi delle aperture la seguono.

# Scorciatoie da tastiera {#shortcuts}

**Scorrere una partita**

- Sinistra, Destra — mossa precedente e successiva
- Home, Fine — prima e ultima mossa
- E — Spiega
- Ctrl+E — avvia o ferma l'analisi
- Ctrl+R — gira la scacchiera

**Partite**

- Ctrl+Shift+N — nuova partita
- Ctrl+Shift+T — nuovo allenamento

**Appunti**

- Ctrl+Alt+C — copia le mosse fino alla posizione corrente
- Ctrl+Shift+C — copia la posizione come FEN
- Ctrl+Shift+V — incolla una FEN

**Progetti e sincronizzazione**

- Ctrl+N, Ctrl+O, Ctrl+S — nuovo progetto, apri e salva
- Ctrl+Y — sincronizza ora
- F1 — questa guida
