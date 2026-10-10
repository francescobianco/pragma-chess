# Per iniziare {#start}

Pragma Chess conserva le tue partite in **database** e te le fa studiare sulla scacchiera con un motore, un libro delle aperture e le tue annotazioni.

A ogni avvio si apre, sopra la finestra principale, la **finestra di benvenuto**: cosa puoi fare con Pragma Chess, poi i tuoi **progetti** e i tuoi **database**, letti dalle loro cartelle (un file che aggiungi lì compare). Fai clic su un progetto per aprirlo, su un database per aprirlo in un nuovo progetto, oppure su **Nuovo progetto**. Spunta **Non mostrare questa finestra all'avvio** per partire subito; **Aiuto ▸ Benvenuto…** la riapre quando vuoi.

La finestra ha la **scacchiera** al centro e quattro pannelli attorno:

- **Partite**, in basso: l'albero del database aperto e la lista delle sue partite.
- **Mosse**: le mosse della partita sulla scacchiera.
- **Albero delle aperture**: le mosse del libro per la posizione sulla scacchiera.
- **Motore**: la valutazione, la linea migliore e l'apertura in cui si trova la partita.

La **barra degli strumenti** ha Sincronizza, Salva progetto, Nuova partita, Nuovo allenamento, Nuova partita online, Entra nella lobby (il bicchiere da martini) e tre icone per scegliere il libro delle aperture, il motore e il database in uso.

Tutto quello che vedi — database, partita, mossa, pannelli — è un **progetto**: la prossima volta che avvii Pragma Chess lo ritrovi come l'hai lasciato.

I tuoi file stanno nella cartella Pragma dentro la tua cartella degli scacchi (per esempio `Scacchi/Pragma` nella tua home), con `Databases`, `Projects` e `Books`. Al primo avvio ci trovi un database di partite classiche.

**Opzioni ▸ Impostazioni personali…** dice chi sei: il tuo nome, il tuo anno di nascita e il tuo ID FIDE. Il tuo nome va dalla tua parte in una nuova partita, in una partita da Inserisci posizione (dal lato in basso della scacchiera) e in un allenamento (col colore che hai scelto), a meno che il database aperto ti conosca già — vince il giocatore segnato come Io con **Chi è?**. **Stile della scacchiera** sceglie insieme i colori delle case e i pezzi: Pragma Classic (case marroni e i pezzi dei libri di scacchi) Lichess Alpha (la scacchiera verde e i pezzi Alpha di lichess.org) o Classic Book (il diagramma di un libro di scacchi stampato: carta antica, le case scure tratteggiate a inchiostro, un margine di carta attorno a ogni pezzo che vi sta sopra, e nessuna coordinata); tutte le scacchiere cambiano appena premi OK. Queste impostazioni stanno in `.pragma-chess.conf`, un file YAML nella tua cartella Pragma, che la sincronizzazione porta sugli altri tuoi computer anche se è nascosto; cambiato su due computer insieme, vince il più recente.

**Opzioni ▸ Impostazioni cartelle…** le sposta altrove su questo computer: la cartella Pragma stessa, oppure solo i database, i progetti, i libri o i nomi delle aperture. Lascia vuota una cartella per tenerla al suo posto solito. I file che ci sono già non vengono spostati, e le nuove cartelle si usano dal prossimo avvio di Pragma Chess. La sincronizzazione tiene uguale sugli altri computer solo ciò che sta dentro la cartella Pragma.

# Database {#databases}

Un database è un file `.pdb` che contiene partite. Se ne apre uno alla volta; il suo nome è nel suggerimento dell'icona del database nella barra degli strumenti.

- **Database ▸ Nuovo database…** ne crea uno vuoto nella cartella dei database.
- **Database ▸ Apri database…** apre un file da qualunque posizione.
- **Database ▸ Cambia database** elenca i database della cartella: scegline uno per aprirlo. L'icona del database nella barra degli strumenti apre lo stesso elenco.
- **Database ▸ Impostazioni database…** (anche dal menu del clic destro sul database in cima all'albero) modifica il nome e la descrizione, dice se il database è una raccolta di partite, un libro d'aperture o **Puzzle e allenamento**, e contiene **Ottimizza database**. In un database Puzzle e allenamento (lo sono Allenati sui finali e Allenati sulla tattica) la lista partite non mostra le mosse, così la soluzione resta nascosta, e una partita aperta da lì parte in **Modalità allenamento**: la scacchiera si gira dalla parte di chi ha il tratto, che è il tuo, e il motore gioca l'altro. Un database con un nome si presenta con quello, seguito dal file tra parentesi — *Mie Partite (partite.pdb)* — in **Cambia database** e in cima all'albero, dove il nome è in grassetto.
- **Database ▸ Salva database come…** ne scrive una copia.
- **Database ▸ Cambia database ▸ Mostra cartella dei database** apre la cartella nel gestore dei file.

Pragma Chess arriva con due database per allenarsi, aggiunti alla cartella Database: **Allenati sui finali** — i finali teorici classici (i matti elementari, l'opposizione, Lucena, Philidor…) con il loro obiettivo, e più di duemila problemi di finale — e **Allenati sulla tattica**, problemi di venti temi, dai facili ai difficili. I problemi vengono dal database dei problemi di lichess.org (libero, CC0); ognuno parte dopo la mossa dell'avversario, e tocca a te trovare la risposta: giocalo in **Modalità allenamento**, o scorri la soluzione. Li trovi nell'albero sotto **Finali** e **Tattica**. Un database di allenamento che elimini non viene aggiunto di nuovo.

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

A sinistra della lista delle partite, l'albero mostra che cosa contiene il database aperto. Seleziona un nodo e la lista mostra solo quelle partite; seleziona il database per vederle tutte. L'albero resta come lo lasci: la prossima volta che il database si apre, anche dopo aver chiuso Pragma Chess, sono aperti gli stessi nodi ed è selezionato lo stesso.

- **Scacchiera ▸ Posizione**: le partite in cui compare la posizione sulla scacchiera, qualunque sia l'ordine delle mosse.
- **Scacchiera ▸ Variante**: le partite che cominciano esattamente con le mosse giocate sulla scacchiera.
- **Io**, **Amico**, **Avversario**: i giocatori che hai indicato con «Chi è?».
- **ECO**: le partite per codice d'apertura.
- **Torneo/Evento** e **Anno**.
- **Cadenza**: le cadenze a cui sono state giocate le partite, dalla più veloce — *Blitz 3+2*, *Rapid 10+5*, *Classica 90+30*, *Corrispondenza 1 giorno*. Le partite di lichess.org e chess.com portano la loro; per ogni altra partita fai clic sui nomi dei giocatori sopra la scacchiera e scrivila in **Cadenza**, come la dicono i giocatori (*3+2*: tre minuti, due secondi aggiunti a ogni mossa).
- **Studio**: gli studi da cui vengono le partite — uno studio di lichess collegato come fonte, o partite il cui PGN porta i tag `StudyName` e `ChapterName` — ognuno con i suoi capitoli nell'ordine dello studio. Seleziona uno studio per tutte le sue partite, o un capitolo per le sue.
- **Sorgente**: le partite arrivate da lichess.org, chess.com o torneionline.com.
- **Finali**: le partite che partono da un finale, per famiglia — *Matti elementari*, *Finali di pedoni*, *Finali di torre*… — e, sotto ciascuna, per materiale: *R+T+P contro R+T*, prima la parte più forte.
- **Tattica**: i problemi per tema — *Inchiodatura*, *Infilata*, *Forchetta*, *Attacco di scoperta*, *Deviazione*…
- **Cestino**: le partite che hai buttato.

Posizione e Variante seguono la scacchiera: scorri una partita e i loro conteggi cambiano. Fai doppio clic su un nodo con una sola partita, e quella partita si apre.

# La scacchiera {#board}

Muovi un pezzo trascinandolo, oppure cliccandolo e poi cliccando la casa d'arrivo. Un pedone che arriva in fondo chiede a che cosa promuovere.

- **Sinistra** e **Destra** vanno indietro e avanti di una mossa, **Home** e **Fine** all'inizio e alla fine. Cliccando una mossa nel pannello Mosse si va lì.
- **Visualizza ▸ Gira scacchiera** (Ctrl+R) gira la scacchiera; **Visualizza ▸ Mostra coordinate** mostra o nasconde lettere e numeri.
- **Opzioni ▸ Impostazioni grafiche…** sceglie l'**Aspetto** — Segui il sistema (il predefinito), oppure sempre Chiaro o sempre Scuro, qualunque cosa dica il sistema —, dove mostrare i pezzi catturati, se indicare a chi tocca e se le mosse si sentono: **Suono quando si muove un pezzo** fa sentire il colpo di un pezzo appoggiato sulla scacchiera per le tue mosse, per quelle del motore in allenamento e per quelle del tuo avversario online (queste ultime quando arrivano sulla casa). Queste impostazioni restano su questo computer: ognuno può accordarsi al proprio desktop.
- Quando la posizione sulla scacchiera è **scacco matto**, il bordo della scacchiera diventa rosso.

**Partita ▸ Nuova partita** (Ctrl+Shift+N) comincia una partita da inserire mossa per mossa. **Partita ▸ Salva partita nel database** la mette nel database aperto; **Partita ▸ Salva partita in un altro database…** la mette in un database a tua scelta, che non viene aperto.

Gioca una mossa che non è la successiva della partita e ti viene chiesto cosa farne: **Inserisci come variante** tiene la linea della partita, e quella nuova compare nel pannello Mosse sotto la mossa che sostituisce; **Sostituisci linea principale** (o **Sostituisci linea**, in una variante) elimina le mosse successive e prosegue con la tua. Durante l'allenamento o una partita online non viene chiesto niente: una mossa diversa è una variante. Una partita salvata nel database viene salvata subito, varianti comprese.

**Partita ▸ Inserisci posizione…** apre una scacchiera su cui disegnare una posizione: scegli un pezzo a destra e fai clic sulle case per posarlo (un clic sullo stesso pezzo lo toglie), trascina un pezzo per spostarlo dove vuoi sulla scacchiera (lasciato fuori, torna al suo posto), fai clic col tasto destro su una casa per svuotarla. Imposta il tratto, gli arrocchi, l'en passant e il numero di mossa, o scrivi un FEN; aiutano **Posizione iniziale**, **Svuota scacchiera** e **Gira scacchiera**. OK si attiva solo per una posizione da cui può cominciare una partita, e il testo sotto la scacchiera dice cosa non va. La posizione comincia una nuova partita in fondo al capitolo, così la partita su cui eri resta com'è. Una partita online in corso viene prima continuata o abbandonata.

**Modifica ▸ Copia** mette negli appunti le mosse, la partita come PGN, la posizione come FEN (Ctrl+Shift+C), la linea del motore o la spiegazione. **Modifica ▸ Incolla** prende quello che c'è negli appunti: **Incolla FEN** (Ctrl+Shift+V) imposta la posizione, **Incolla linea** comincia una nuova partita con le mosse (PGN, mosse con o senza numeri, o UCI), e **Incolla linea dalla posizione attuale** (Ctrl+Alt+V) gioca le mosse dalla posizione sulla scacchiera, come se le giocassi tu: alla fine della partita vengono aggiunte, altrove diventano una variante.

**Modifica ▸ Appunti…** tiene le cose sotto un nome: ogni cassetto degli appunti ha un nome e un contenuto — mosse, una variante, una posizione FEN, una nota. **Nuovo cassetto** ne aggiunge uno, **Elimina** toglie quello selezionato; OK conserva le modifiche, purché ogni cassetto abbia un nome e due non abbiano lo stesso. Gli appunti sono tuoi, conservati con le tue impostazioni personali, quindi sono gli stessi su ogni computer sincronizzato. Metterci e riprendere le cose al volo arriverà presto.

# Mosse e annotazioni {#moves}

Il pannello Mosse elenca la partita sulla scacchiera, con i pezzi disegnati come figurine. Clicca una mossa per andarci.

La linea principale è una tabella, una mossa per cella: clicca la cella per andare alla mossa. Le **varianti** hanno una riga tutta loro sotto la mossa che sostituiscono, più piccole, come testo; le varianti di una variante la seguono tra parentesi. Clicca una mossa di una variante per seguire quella linea: le frecce poi si muovono lungo di essa, e cliccando una mossa della linea principale si torna indietro. All'inizio di una variante, giocare la mossa della linea principale riprende la linea principale, e un'altra mossa apre una variante sorella. Le varianti sono salvate con la partita e viaggiano nel PGN che copi.

Una partita scritta a mano può avere una **mossa nulla**, scritta `--`: chi ha il tratto passa, dove una mossa non era nota o per mostrare un piano (ChessBase e il PGN la scrivono così). È mostrata e copiata come `--`, le frecce la attraversano come ogni altra mossa e il motore analizza la posizione dopo di essa. Spiega non ha niente da dire su una mossa nulla; non si gioca mai in allenamento, online o nella lobby.

Clic destro su una mossa per il suo menu:

- **Copia ▸ Copia mossa** e **Copia ▸ Copia linea fino a qui** mettono negli appunti la mossa, o la partita fino a lì.
- **Annotazioni** elenca i simboli con il significato di ciascuno.
- **Varianti**: su una mossa di una variante, **Promuovi variante** la fa diventare la linea da cui parte (le mosse che sostituiva diventano la sua variante, con le loro) e **Elimina variante** la toglie; su qualunque mossa, **Elimina da qui** toglie lei e le mosse dopo di lei sulla sua linea, con le loro varianti. Eliminare chiede prima conferma: non si può annullare.

Una mossa può avere un giudizio sulla mossa e una valutazione della posizione:

- `!!` mossa brillante, `!` buona mossa, `!?` mossa interessante, `?!` mossa dubbia, `?` errore, `??` errore grave, `□` mossa unica.
- `+−` il Bianco è in vantaggio decisivo, `±` il Bianco sta meglio, `⩲` il Bianco sta leggermente meglio, `=` posizione pari, `∞` posizione poco chiara, `⩱` `∓` `−+` lo stesso per il Nero.

Scegli il simbolo che la mossa ha già per toglierlo, oppure **Nessuna annotazione** per toglierli tutti. Le annotazioni sono salvate subito con la partita e vengono scritte quando copi la partita come PGN.

I **commenti** — il testo tra le mosse di una partita PGN o di uno studio lichess — sono in corsivo: sotto la loro mossa nella linea principale, tra le mosse in una variante. I comandi che i programmi mettono nei commenti (una valutazione `[%eval 0.18]`, un orologio, delle frecce) restano con la partita ma non si vedono. I commenti si salvano nel database con la partita e si riscrivono quando la partita viene scritta in PGN.

Fai doppio clic su un commento per scriverci, lì dov'è, come si scrive un paragrafo (Esc, Ctrl+Invio o un clic altrove finiscono; se lo svuoti, sparisce). Il clic destro su una mossa offre **Aggiungi commento** o **Modifica commento**, su un commento **Modifica commento** ed **Elimina commento**. I comandi che contiene restano com'erano. Le mosse scritte in un commento — "4.d4 cxd4 era meglio", "poi Qg5" — sono link: fai clic su una e la sua linea viene giocata sulla scacchiera come variante della partita (o viene presa la variante che è già), poi la segui con le frecce come le altre. Una mossa con il numero parte da quella mossa della linea; una senza numero continua dal commento, o prende il posto della mossa commentata. I cerchi e le frecce che uno studio lichess disegna sulle caselle (`[%csl …]`, `[%cal …]` nel commento) compaiono sulla scacchiera, nei loro colori, mentre è mostrata la posizione del loro commento.

# Spiega {#explain}

**Spiega** mostra sulla scacchiera perché l'ultima mossa è buona o cattiva. Premi il bottone tra le frecce sotto la scacchiera, oppure il tasto **E**.

Il motore guarda la posizione prima e dopo la mossa e disegna delle frecce:

- frecce **rosse**: sta per cadere del materiale, e i pezzi che si perdono sono cerchiati;
- frecce **verdi**: le idee della tua parte, le mosse che fanno la valutazione;
- frecce **blu**: le mosse dell'altra parte — la risposta che fa la differenza quando ancora non si perde niente, o il suo piano; *la tua parte* è quella che giochi in allenamento, altrimenti quella in basso sulla scacchiera;
- frecce **rosse tratteggiate**: una minaccia, un pezzo lasciato sotto attacco — «5.Dxf3 attacca la torre in a8», o quella a cui la tua mossa non ha risposto, «5…Dh4+ lascia la torre in a8 sotto attacco: 7.Dxa8»;
- una freccia **verde tratteggiata**: la mossa migliore che avevi al posto della tua. Quando muove lo stesso pezzo che hai mosso tu — e quindi la casa da cui parte ora è vuota, perché quel pezzo è andato altrove — il pezzo è disegnato piccolo e trasparente dove doveva arrivare.

Un matto forzato viene giocato sulla scacchiera, dentro una cornice rossa. Il pannello Motore lo dice a parole, per esempio «Errore grave (+0.3 → −2.9). Il Nero vince un cavallo».

Spiega legge l'analisi in corso nel pannello Motore: la spiegazione compare appena il motore ha cercato abbastanza a fondo, e si affina mentre va più a fondo — le frecce cambiano solo quando una nuova linea regge per un po'. La mossa viene giudicata rispetto alla posizione prima: se quella non è mai stata analizzata (sei arrivato direttamente alla mossa, o hai riaperto Pragma Chess lì), il motore la guarda prima, per un momento, e poi continua con la posizione sulla scacchiera. Fino alla prima risposta il bordo della scacchiera pulsa; diventa blu quando la spiegazione è pronta. Spiega riguarda una mossa sola: andando a un'altra mossa si spegne, e la chiedi di nuovo. Se chiudi Pragma Chess con Spiega acceso, si riapre acceso, sulla stessa mossa.

**Modifica ▸ Copia ▸ Spiegazione** copia il testo.

# Motori {#engine}

**Motore ▸ Analisi** (Ctrl+E) accende e spegne l'analisi della posizione sulla scacchiera. Il pannello Motore mostra il punteggio — sempre dal lato del Bianco: positivo è buono per il Bianco —, la profondità e la linea migliore. La barra accanto alla scacchiera mostra lo stesso punteggio.

**Per vedere dove porta la linea**, tieni premuto l'occhio accanto a Ferma analisi: finché lo tieni, la scacchiera mostra la posizione alla fine della linea del motore, come se tutte le sue mosse fossero giocate, e segue il motore: quando il motore trova un'altra linea, la scacchiera va dove finisce quella nuova. Rilascia e torna la tua posizione. Su quella posizione, frecce viola mostrano i piani che la linea contiene: la strada fatta da un pezzo per arrivarci (il giro di un cavallo, una torre che si alza, la donna che cambia ala), la marcia del re in un finale, un pedone che corre o rompe. Una freccia che gira passa per le case dove il pezzo si è fermato. Gli scambi e le mosse forzate da uno scacco non sono piani: una linea fatta solo di quelli non mostra frecce.

Pragma Chess arriva con Stockfish, e funziona con qualunque motore UCI.

- **Motore ▸ Cambia motore** sceglie tra i motori di questo computer. L'icona del motore nella barra degli strumenti apre lo stesso elenco.
- **Motore ▸ Gestisci motori…** aggiunge, modifica e rimuove motori. **Rileva motori** trova quelli installati sul computer; **Usa questo motore** passa a quello selezionato.
- **Potenza di calcolo**, nella stessa finestra, è quanta parte del computer il motore può usare mentre analizza, da **Minima** (3% del processore) a **Piena** (tutto, senza limiti); **Media**, 10%, è quella da cui parte ogni motore. A ogni livello il motore è forte uguale, solo più lento: riceve meno thread, una priorità più bassa e, su Linux e Windows, un tetto alla sua parte del processore, così le ventole restano tranquille e il resto del computer libero. I **Thread**, se li imposti, valgono più del livello.

Il motore in uso fa parte del progetto.

# Allenamento {#training}

**Partita ▸ Nuovo allenamento…** (Ctrl+Shift+T) comincia una partita contro il motore. Scegli Bianco, Nero o Casuale e premi Inizia.

Finché tocca a te il motore nasconde la sua linea migliore e mostra solo il punteggio. Quando hai mosso risponde da solo, lentamente, così vedi la sua mossa.

Finché la posizione è nel **libro di aperture**, il motore risponde con una mossa del libro, scegliendo ciascuna tanto spesso quanto dice il suo peso: regola i pesi nell'Albero delle aperture per allenarti contro le linee che vuoi, quanto vuoi. Fuori dal libro gioca la sua mossa migliore.

Il **tutor** guarda le tue mosse. Quando una è un'**imprecisione**, un **errore**, un **errore grave** o un'**occasione mancata**, il motore non risponde; il bordo della scacchiera diventa rosso, il pannello Motore dice che cosa è successo e offre:

- **Ritira**: torni alla posizione e provi un'altra mossa;
- **Spiega**: lo stesso Spiega del pulsante sotto la scacchiera, che si accende e si spegne insieme a lui: mostra sulla scacchiera perché la mossa è un errore;
- **Ignora**: tieni la mossa, e il motore risponde.

Puoi tornare indietro in una partita di allenamento e rigiocare una mossa, anche dove la partita prosegue: una mossa che giochi sulla scacchiera è il tuo turno, quindi il motore risponde e il tutor la giudica (una risposta diversa da quella già presente apre una variante). Scorrere le mosse con le frecce è solo guardare.

Scacco matto o stallo chiudono la partita, che viene salvata nel database aperto.

Spunta **Ricorda per questa sessione** nella finestra Nuovo allenamento e il bottone della barra degli strumenti comincerà i prossimi allenamenti con la stessa scelta, senza chiedere. Il menu chiede sempre.

**Motore ▸ Modalità allenamento** accende o spegne l'allenamento per la partita sulla scacchiera. Aprire una partita dalla lista lo spegne. Se chiudi Pragma Chess mentre ti alleni, riparte in allenamento.

# Giocare online {#online}

**Partita ▸ Nuova partita online…** gioca una partita contro una persona su lichess.org (altre piattaforme seguiranno).

La finestra elenca le **piattaforme a cui sei connesso**, ciascuna con l'account con cui giochi. **Connetti piattaforma…** chiede di che tipo è, apre nel browser la sua pagina di accesso e porta qui la connessione; **Disconnetti** ne toglie una. Le connessioni sono tue su questo computer, conservate con le tue impostazioni, mai in un progetto. Scegli la connessione, l'**orologio** (minuti e incremento), il colore e se la partita è **classificata**, poi **Cerca un avversario**.

Mentre Pragma Chess cerca un avversario e mentre giochi, è in **modalità gioco online**, spuntata in **Motore ▸ Modalità gioco online** (sceglierla avvia o interrompe il gioco online): il motore, Spiega e la Modalità allenamento sono spenti e non si possono accendere, e l'Albero delle aperture resta dove l'hai messo ma non elenca mosse, e la sua prima riga dice perché — sei tu contro il tuo avversario. Il pannello Motore mostra i nomi, i punteggi, gli orologi e a chi tocca. Le tue mosse vanno alla piattaforma appena le fai; quelle dell'avversario scivolano sulla scacchiera. Si gioca solo la posizione in corso: puoi riguardare le mosse precedenti, e tornare alla fine per muovere. Se Pragma Chess si chiude durante una partita, al successivo avvio si ricollega e la partita continua da dove è (l'orologio, sulla piattaforma, intanto è andato avanti); una partita finita nel frattempo viene salvata con il suo risultato. Puoi cambiare database mentre giochi: la partita resta sulla scacchiera e, quando finisce, viene salvata nel database aperto in quel momento.

Spunta **Ricorda per questa sessione** e il bottone Gioca online della barra degli strumenti cerca un avversario con le stesse scelte senza chiedere; il menu chiede sempre.

**Partita ▸ Nuova partita online…** durante una partita ne inizia una nuova online, **Partita ▸ Nuova partita** chiede se vuoi una nuova partita online (come Nuova partita online nella barra degli strumenti) o una nuova partita da analizzare — con **Ricorda per questa sessione** non lo chiede più finché Pragma Chess resta aperto —, e **Partita ▸ Nuovo allenamento…** una partita contro il motore; ciascuna chiede prima se **continuare a giocare** la partita in corso o **abbandonarla** (se si sta ancora cercando un avversario, la ricerca si interrompe e basta). Scegliere **Motore ▸ Modalità gioco online** quando è attiva interrompe la ricerca, o abbandona la partita dopo averlo chiesto. Quando la partita finisce — scacco matto, abbandono, tempo, patta — il risultato viene scritto e la partita è salvata nel database aperto, con i giocatori, i loro punteggi e un collegamento alla partita.

**Partita ▸ Entra nella lobby…** apre la lobby dei tornei senza orologio, giocati con gli altri utenti di Pragma Chess su una rete peer-to-peer: non c'è un nostro server, ogni computer tiene una copia della lobby e controlla ogni mossa, e dei relay pubblici conservano le mosse per chi è assente. Ogni stanza è un torneo di quattro giocatori, in cui tutti giocano contro tutti due volte, una per colore; comincia appena si siedono due giocatori, e la lobby tiene sempre almeno due stanze con un posto libero, offrendo una **Nuova stanza** quando sono meno. Ogni stanza ha il nome di un tema degli scacchi e di un campione, come La fortezza di Capablanca; quelle piene sono elencate dopo le altre, e se ne possono seguire le partite. **Entra nella stanza** mostra la classifica — posizione, vittorie, patte, sconfitte e punti, 1 per una vittoria e ½ per una patta, poi i posti liberi —, le partite e, su una piccola scacchiera, la posizione della partita che selezioni. **Siediti al tavolo** ti fa sedere a un posto libero: le tue partite con chi c'è compaiono in grassetto, e le stanze dove sei seduto vengono prima nella lobby. Giochi al massimo due tornei alla volta: un terzo posto aspetta che uno dei tuoi sia finito — tutti i posti presi e tutte le partite concluse —, e la stanza lo dice. Chi arriva primo in un torneo vince una medaglia, un pallino giallo dopo il suo nome ovunque la lobby lo mostri, per sempre: chi è primo a pari merito la riceve anche lui, e il suggerimento dice quanti tornei un giocatore ha vinto. Una stanza con partite che aspettano la tua mossa ha **Gioca ora** nella colonna Tocca a te: con una sola partita ti porta subito lì, con più partite ti chiede quale, per avversario e colore. Nella stanza, le partite in cui tocca a te sono in cima alla lista, poi le altre tue, poi il resto. **Gioca la tua mossa** porta la partita sulla scacchiera, vista dalla tua parte e alla mossa da giocare, con la stanza come evento; se la partita sulla scacchiera non è salvata da nessuna parte ti viene prima chiesto se salvarla, lasciarla andare o restare dove sei, e la lobby si fa da parte (Partita ▸ Entra nella lobby… la riapre com'era). La partita è allora in **modalità lobby**, spuntata in **Motore ▸ Modalità lobby**: non si spegne niente — il gioco per corrispondenza ammette il motore e la preparazione — e il pannello Motore dice a che punto è la partita, con due bottoni. **Invia mossa** invia la mossa successiva al punto in cui è arrivata la partita. **Invia piano** la invia insieme alle risposte che hai preparato: continua a giocare muovendo anche i pezzi del tuo avversario, e metti le sue altre possibili risposte come varianti, ognuna seguita dalla tua; quando l'avversario ne gioca una, la tua risposta viene giocata subito. Anche l'avversario può aver preparato le sue: dopo l'invio vedi le mosse giocate dai due piani, finché tocca a qualcuno pensare. Un piano ha una risposta per ogni posizione, quella della linea; un'altra tua mossa in una variante viene lasciata fuori, e il pannello lo dice. Chiudendo Pragma Chess non si perde niente: si riapre sulla partita della lobby, in modalità lobby, alla mossa che stavi guardando, con il piano che stavi preparando. Il tuo piano non viene mai inviato all'avversario: lo tiene il tuo computer e lo gioca quando arriva la sua mossa, quindi risponde solo mentre Pragma Chess è aperto su uno dei tuoi computer. Pragma Chess non ti disturba mentre è aperto: nessun messaggio ti dice che qualcuno ha mosso. L'unico posto che lo fa è la finestra della lobby, finché è aperta: segue la lobby in diretta, e una partita in cui è appena diventato il tuo turno si illumina per un momento, insieme alla sua stanza nella lista. In fondo alla lobby, e nella barra di stato finché la lobby è attiva, c'è come sei collegato — quanti relay e quanti computer di altri giocatori —, oppure che non lo sei: quello che invii allora parte appena lo sei. Nella lobby sei una chiave, che firma le tue mosse: la trovi in **Opzioni ▸ Impostazioni personali…**, e per sicurezza non viene sincronizzata, quindi per giocare come te da un altro computer copiala laggiù a mano.

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
- **File ChessBase**: un database ChessBase su questo computer: il formato classico (`.cbh` e i suoi file) o quello di ChessBase 17 e successivi (`.2cbh` e i suoi file). Scegli il file `.cbh` o `.2cbh`: le sue partite vengono copiate (da un `.2cbh` con le varianti, i commenti nella tua lingua quando ce ne sono in più lingue, i simboli, le case e le frecce colorate e le valutazioni del motore), i file restano dove sono e le partite aggiunte in seguito arrivano alla sincronizzazione successiva. Su un altro computer il file non c'è: la sincronizzazione lo dice e propone di ignorare la sorgente su quel computer; *Database ▸ Gestisci sorgenti… ▸ Modifica…* sceglie di nuovo il file, e **Usa su questo computer** smette di ignorarla.
- **Lichess Study**: uno studio su lichess.org. Incolla l'indirizzo della sua pagina (o di uno dei suoi capitoli) in **Indirizzo dello studio**: ogni capitolo diventa una partita del database, con i suoi commenti e le sue varianti, e i suoi tag `StudyName`, `ChapterName` e `ChapterURL` dicono di quale studio e capitolo si tratta (i capitoli dello studio non sono i capitoli di un progetto). Un capitolo cambiato su lichess.org sostituisce la sua partita alla sincronizzazione successiva, a meno che la partita sia cambiata anche qui: allora si tengono tutte e due. Uno studio pubblico non chiede un account; per uno privato, **Accedi con lichess.org…** con un account che può vederlo. Uno studio può essere visibile a tutti ma non scaricabile: il suo autore sceglie chi può esportarlo (**Condividi ed esporta** nelle impostazioni dello studio su lichess.org), e solo loro possono collegarlo. Per ora lo studio viene solo letto: la scrittura è in arrivo.
- **File PGN**: un file `.pgn` su questo computer, tenuto al passo con il database. Sceglilo con **Sfoglia…**, o creane uno vuoto con **Nuovo file…**, poi la **Direzione**: **Leggi e scrivi** (le partite del file entrano nel database e quelle del database vanno nel file, così una partita aggiunta o modificata da una parte arriva all'altra), **Solo lettura** (il file non viene mai scritto) o **Solo scrittura** (tutte le partite del database vanno nel file; le altre partite del file restano fuori dal database). Una partita che salvi, modifichi o annoti viene scritta nel file pochi secondi dopo. Niente di ciò che togli da una parte viene tolto dall'altra, e una partita modificata da entrambe le parti tra due sincronizzazioni viene tenuta due volte. Pragma Chess segna ogni partita che lega al database con un tag `PragmaUid`, e tiene un indice accanto al file in un file nascosto, così un file che non è cambiato non viene riletto.

Le sorgenti vengono lette quando il database viene aperto e ogni venti minuti. Una partita non viene mai importata due volte. **Database ▸ Gestisci sorgenti…** sincronizza subito una sorgente, la modifica, rifà l'accesso o la rimuove; le partite già importate restano.

Il nodo **Sorgente** dell'albero elenca le partite di ogni sorgente.

# Cestino {#trash}

Clic destro su una partita della lista e scegli **Cestina partita**. La partita esce da tutte le liste e non viene più cercata. Per agire su più partite insieme, selezionale con **Maiusc** e le frecce (o un clic), oppure **Ctrl** e un clic: il menu del tasto destro allora le cestina, ripristina o elimina tutte; **Chi è?** compare solo per una partita.

Il nodo **Cestino**, ultimo dell'albero, mostra le partite cestinate: **Recenti**, buttate negli ultimi sette giorni, e **Vecchie**. Lì puoi scegliere **Ripristina partita** o **Elimina partita…**.

Eliminare non rimpicciolisce ancora il file. **Database ▸ Impostazioni database… ▸ Ottimizza database** toglie per sempre le partite eliminate e compatta il file. Fino ad allora non si perde niente.

# Progetti {#projects}

Un progetto è quello che stai guardando: il database, la partita e la mossa, il lato da cui vedi la scacchiera, il motore, i pannelli e se ti stai allenando. La barra del titolo ne mostra il nome — quello del file, o quello dato in **File ▸ Impostazioni progetto…** — con un asterisco quando ha modifiche non salvate, e, quando il progetto ha dei capitoli, il capitolo aperto (sparisce quando il progetto torna senza capitoli): *Aperture* - L'Italiana - Pragma Chess*.

- **File ▸ Nuovo progetto** tiene quello che vedi — database, motore, pannelli — e comincia una partita nuova, vuota, col Bianco in basso.
- **File ▸ Apri progetto…** e **File ▸ Apri progetto recente** aprono un file `.pch`. Nel gestore dei file un progetto ha l'icona dei documenti del tuo sistema con un pedone sul foglio, e aprirlo apre Pragma Chess.
- **File ▸ Salva progetto** e **File ▸ Salva progetto come…** lo salvano.

Non sei obbligato a salvare: Pragma Chess si riapre come l'hai chiuso.

I pannelli si possono ridimensionare e chiudere; il menu **Visualizza** (Mosse, Albero delle aperture, Motore, Lista partite) li mostra di nuovo, e **Visualizza ▸ Ripristina disposizione dei pannelli** li rimette dove stanno all'inizio.

**File ▸ Impostazioni progetto… ▸ Sola lettura** protegge un progetto da modifiche fatte senza pensarci: i suoi capitoli, titoli, paragrafi, commenti e varianti non si possono cambiare, e non viene salvato — la scacchiera si può comunque esplorare, e la barra del titolo dice *(sola lettura)*. Togli la spunta per modificare il progetto. I progetti che arrivano con Pragma Chess sono in sola lettura.

# Capitoli e paragrafi {#chapters}

Un progetto è una raccolta di **capitoli**, come uno studio o un libro di scacchi, e un capitolo contiene partite una dopo l'altra, con del testo tra le loro mosse. La lista delle mosse mostra tutto il capitolo: una riga leggera segna dove comincia ogni partita, e la numerazione riparte; fai clic su una mossa di un'altra partita e la scacchiera va lì.

Fai clic col tasto destro sulla lista delle mosse — su una mossa, su un paragrafo, o dove vuoi, anche senza mosse — per:

- **Inserisci ▸ Paragrafo**: un paragrafo dopo la mossa (o dove si trova la scacchiera), scritto direttamente nella lista delle mosse. Scrivi come in un libro: il testo è giustificato, e ogni a capo comincia un nuovo capoverso, rientrato. **Esc**, **Ctrl+Invio** o un clic altrove per finire; un paragrafo lasciato vuoto sparisce. Fai doppio clic su un paragrafo (o un titolo, o un sottotitolo) per riscriverci — un clic singolo non fa nulla; **Modifica paragrafo**, **Sposta** ed **Elimina paragrafo** sono nel suo menu: Sposta lo porta **In testa** o **In fondo** alla partita, oppure **Su** e **Giù** di una semimossa alla volta — dopo la mossa del Bianco, Giù lo porta dopo quella del Nero, che torna sulla riga del Bianco. I paragrafi vanno sulla linea principale.
- **Inserisci ▸ Titolo** e **Inserisci ▸ Sottotitolo**: un'intestazione dove andrebbe un paragrafo, per dividere un capitolo come fa un libro: il titolo in grassetto e centrato, il sottotitolo in grassetto, un po' più piccolo e anch'esso centrato. Si scrivono, si spostano e si eliminano come i paragrafi. Un titolo o un sottotitolo è una riga sola: **Invio** lo conferma.
- **Sposta**: sposta quello su cui hai fatto clic. Su un titolo, un sottotitolo o un paragrafo sposta quello, come sopra; su una mossa, o altrove, sposta l'intera partita **In testa** o **In fondo** al capitolo, oppure **Su** e **Giù** oltre la partita sopra o sotto — le interruzioni tra le partite si adeguano; la scacchiera resta sulla partita che mostra.
- **Inserisci ▸ Interruzione partita**: una nuova partita dalla posizione iniziale, con la numerazione che riparte da 1, subito sotto la partita in cui hai fatto clic destro — sempre, anche più di una di seguito. Una partita ancora senza mosse ha comunque il suo posto nella lista, con la casella della prima mossa sbiadita («1. …»): un clic lì porta a quella partita. Resta finché non la elimini.
- Clic destro in qualunque punto di una partita — una mossa, la sua prima casella, il suo testo — per **Cancella partita**, o **Cancella linea** quando non comincia dalla mossa 1. Clic destro sul numero della prima mossa di una partita per **Cambia numero di mossa…**: una linea da una posizione impostata può cominciare dalla mossa 12 invece che dalla 1 (è il numero di mossa della posizione iniziale, come lo scrive il PGN).
- Clic destro sulla riga di un'interruzione di partita: **Elimina interruzione di partita** la toglie quando dopo non è stato inserito niente; quando la partita che segue ha delle mosse, **Elimina partita seguente** toglie l'interruzione e quella partita (una partita del database resta lì; se andrebbero perse mosse non salvate da nessuna parte te lo chiede prima). Titoli, sottotitoli e paragrafi si eliminano allo stesso modo, dal loro menu.

Nuova partita, Nuovo allenamento, Inserisci posizione, gli Incolla e le partite online aggiungono la loro partita in fondo al capitolo, e così una partita aperta dalla lista delle partite (una che il capitolo ha già viene semplicemente mostrata). Le partite salvate in un database vengono salvate lì man mano che cambiano; le altre, e tutti i paragrafi, vengono salvati con il progetto.

Il menu **File** ha i capitoli: **Nuovo capitolo…**, **Cambia capitolo** per scegliere quello aperto, e in fondo al suo elenco **Gestione capitoli…** per riordinarli (trascinando, o con Sposta su e Sposta giù), rinominarli, aggiungerli ed eliminarli. **Impostazioni progetto…** dà al progetto un nome suo.

Un progetto può essere **multilingua**: spunta **Progetto multilingua** in **File ▸ Impostazioni progetto…** e scegli la **Lingua dei testi**, English o Italiano. Il progetto resta uno — gli stessi capitoli, partite e mosse —, ma il suo nome, i titoli dei capitoli, i titoli, i sottotitoli e i paragrafi si scrivono in ogni lingua a parte: quello che scrivi va nella lingua scelta, e un testo non ancora scritto in quella lingua si vede in inglese, o nell'altra lingua, così puoi tradurlo lì dov'è. Un progetto si apre sempre nella lingua dell'interfaccia; uno che non è multilingua resta in quella. Così puoi usare Pragma Chess in italiano e curare i testi di un progetto in inglese.

Pragma Chess arriva con alcuni progetti suoi, nella cartella Progetti dal primo avvio, per vedere cosa può essere un progetto: aprili con **File ▸ Apri progetto…**. **Finali di torre** percorre i finali di torre che ogni giocatore incontra, **Finali di pedone** le idee a cui ogni finale si riduce — il quadrato, l’opposizione, le case chiave, il pedone di torre, il pedone passato lontano, lo sfondamento —, e **Fischer – Spassky 1972** commenta la sesta partita del match di Reykjavík; tutti in italiano e in inglese.

Un progetto nuovo e vuoto non ha capitoli: **Cambia capitolo** mostra *(Nessun capitolo)*, in grigio, e una partita aperta dalla lista partite, o una nuova, prende il posto di quella sulla scacchiera. Le mosse che giochi sulla scacchiera non sono ancora un capitolo: quando non sono salvate da nessuna parte, prima ti viene chiesto se **Salvare** la partita nel database aperto o lasciarla andare (**Non salvare**). Appena ci metti qualcosa — un'interruzione di partita, un paragrafo, un titolo — diventa il primo capitolo, con tutto ciò che c'è; **Nuovo capitolo…** fa lo stesso. Elimina tutti i capitoli in **Gestione capitoli…** e il progetto torna senza capitoli, con la scacchiera vuota. E se il capitolo era nato da solo, eliminare ciò che l'aveva creato — l'interruzione, il paragrafo, il titolo — riporta anche il progetto senza capitoli; un capitolo creato con **Nuovo capitolo…**, o rinominato, resta.

# Sincronizzazione {#sync}

La sincronizzazione tiene uguale la tua cartella Pragma — database e progetti — su più computer, attraverso una cartella su un server.

**Opzioni ▸ Impostazioni di sincronizzazione…** la imposta: un server **FTP** (anche con TLS), un server **WebDAV** o un **repository Git**. **Prova connessione** la verifica.

**File ▸ Sincronizza ora** (Ctrl+Y, anche primo bottone della barra degli strumenti) fa tutto in ordine: legge le sorgenti, salva il progetto e scambia i file con il server. **Sincronizza prima di chiudere** lo fa ogni volta che esci.

La sincronizzazione non elimina niente di sua iniziativa: un database che manca da una parte viene copiato lì, e un database modificato su due computer viene unito partita per partita. Per liberarti di una partita usa il cestino, che gli altri computer seguono.

Ogni computer ricorda cosa ha sincronizzato l'ultima volta in un file nascosto della sua cartella Pragma, `.pragma-chess.local`, che non va mai sul server. Così distingue un file che hai eliminato a mano — dal file manager, per esempio — da uno che deve ancora ricevere: alla sincronizzazione successiva ti chiede cosa farne: **Elimina ovunque**, **Ripristina** o **Chiedimelo più tardi** (te lo richiede al prossimo avvio di Pragma Chess). Eliminato ovunque, il file viene tolto dal server e ogni altro computer sposta la sua copia nel cestino alla sincronizzazione successiva. Un file che nel frattempo qualcuno ha modificato su un altro computer torna semplicemente indietro.

I file che appartengono solo al server — il README e la LICENSE di un repository Git, per esempio — si elencano in un file **`.pragmaignore`** nella radice della cartella sul server, uno per riga, scritti come in `.gitignore` (`*.tmp`, `Appunti/`, `# un commento`). Restano sul server: non vengono mai copiati sui tuoi computer, la copia che un computer aveva già ricevuto se ne va alla sincronizzazione successiva (se l'avevi modificata lì resta, ma non viene più sincronizzata) e nessun file con quel nome parte dai tuoi computer.

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
