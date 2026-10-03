# Per iniziare {#start}

Pragma Chess conserva le tue partite in **database** e te le fa studiare sulla scacchiera con un motore, un libro delle aperture e le tue annotazioni.

La finestra ha la **scacchiera** al centro e quattro pannelli attorno:

- **Partite**, in basso: l'albero del database aperto e la lista delle sue partite.
- **Mosse**: le mosse della partita sulla scacchiera.
- **Albero delle aperture**: le mosse del libro per la posizione sulla scacchiera.
- **Motore**: la valutazione, la linea migliore e l'apertura in cui si trova la partita.

La **barra degli strumenti** ha Sincronizza, Salva progetto, Nuova partita, Nuovo allenamento e tre icone per scegliere il libro delle aperture, il motore e il database in uso.

Tutto quello che vedi — database, partita, mossa, pannelli — è un **progetto**: la prossima volta che avvii Pragma Chess lo ritrovi come l'hai lasciato.

I tuoi file stanno nella cartella Pragma dentro la tua cartella degli scacchi (per esempio `Scacchi/Pragma` nella tua home), con `Databases`, `Projects` e `Books`. Al primo avvio ci trovi un database di partite classiche.

# Database {#databases}

Un database è un file `.pdb` che contiene partite. Se ne apre uno alla volta; il suo nome è nel suggerimento dell'icona del database nella barra degli strumenti.

- **Database ▸ Nuovo database…** ne crea uno vuoto nella cartella dei database.
- **Database ▸ Apri database…** apre un file da qualunque posizione.
- **Database ▸ I miei database** elenca i database della cartella: scegline uno per aprirlo. L'icona del database nella barra degli strumenti apre lo stesso elenco.
- **Database ▸ Impostazioni database…** modifica il nome e la descrizione, dice se il database è una raccolta di partite o un libro d'aperture, e contiene **Ottimizza database**.
- **Database ▸ Salva database come…** ne scrive una copia.
- **Database ▸ I miei database ▸ Mostra cartella dei database** apre la cartella nel gestore dei file.

Le modifiche a un database vengono scritte mentre le fai: non c'è niente da salvare a mano.

# La lista delle partite {#games-list}

La lista mostra le partite del database aperto, una per riga. Fai doppio clic su una partita per metterla sulla scacchiera.

- Clicca il titolo di una colonna per **ordinare**; clicca ancora per invertire.
- Trascina il titolo di una colonna per **spostarla**.
- Clic destro sul titolo di una colonna per **nasconderla**, o per **mostrare** una colonna nascosta. Ogni database ricorda le sue colonne.
- L'ultima colonna, **Linea**, mostra come comincia la partita; è tagliata con «…» dove finisce la colonna.

Clic destro su un giocatore per dire **chi è**: tu, un amico o un avversario. L'albero elenca poi quei giocatori sotto Io, Amici e Avversari, e una partita in cui giochi tu si apre con la scacchiera girata dalla tua parte.

Clic destro su una partita per metterla nel **cestino**.

Per cambiare giocatori, evento, data o risultato della partita sulla scacchiera, clicca l'intestazione sopra la scacchiera.

# L'albero del database {#database-tree}

A sinistra della lista delle partite, l'albero mostra che cosa contiene il database aperto. Seleziona un nodo e la lista mostra solo quelle partite; seleziona il database per vederle tutte.

- **Scacchiera ▸ Posizione**: le partite in cui compare la posizione sulla scacchiera, qualunque sia l'ordine delle mosse.
- **Scacchiera ▸ Variante**: le partite che cominciano esattamente con le mosse giocate sulla scacchiera.
- **Io**, **Amici**, **Avversari**: i giocatori che hai indicato con «Chi è?».
- **ECO**: le partite per codice d'apertura.
- **Tornei** e **Anni**.
- **Fonti**: le partite arrivate da lichess.org, chess.com o torneionline.com.
- **Cestino**: le partite che hai buttato.

Posizione e Variante seguono la scacchiera: scorri una partita e i loro conteggi cambiano.

# La scacchiera {#board}

Muovi un pezzo trascinandolo, oppure cliccandolo e poi cliccando la casa d'arrivo. Un pedone che arriva in fondo chiede a che cosa promuovere.

- **Sinistra** e **Destra** vanno indietro e avanti di una mossa, **Home** e **Fine** all'inizio e alla fine. Cliccando una mossa nel pannello Mosse si va lì.
- **Visualizza ▸ Gira scacchiera** (Ctrl+R) gira la scacchiera; **Visualizza ▸ Mostra coordinate** mostra o nasconde lettere e numeri.
- **Opzioni ▸ Impostazioni scacchiera…** sceglie dove mostrare i pezzi catturati e se indicare a chi tocca.

**Partita ▸ Nuova partita** (Ctrl+Shift+N) comincia una partita da inserire mossa per mossa. **Partita ▸ Salva partita nel database** la mette nel database aperto.

Gioca una mossa che non è la successiva della partita e diventa una **variante**: la partita tiene la sua linea, e quella nuova compare nel pannello Mosse sotto la mossa che sostituisce. Una partita salvata nel database viene salvata subito, varianti comprese.

**Modifica ▸ Copia** mette negli appunti le mosse, la partita come PGN, la posizione come FEN (Ctrl+Shift+C), la linea del motore o la spiegazione. **Modifica ▸ Incolla FEN** (Ctrl+Shift+V) imposta la posizione che c'è negli appunti.

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

# Spiega {#explain}

**Spiega** mostra sulla scacchiera perché l'ultima mossa è buona o cattiva. Premi il bottone tra le frecce sotto la scacchiera, oppure il tasto **E**.

Il motore guarda la posizione prima e dopo la mossa e disegna delle frecce:

- frecce **rosse**: sta per cadere del materiale, e i pezzi che si perdono sono cerchiati;
- frecce **blu**: la risposta che fa la differenza, quando ancora non si perde niente.

Un matto forzato viene giocato sulla scacchiera, dentro una cornice rossa. Il pannello Motore lo dice a parole, per esempio «Errore grave (+0.3 → −2.9). Il Nero vince un cavallo».

Mentre il motore cerca, il bordo della scacchiera pulsa; diventa blu quando la spiegazione è pronta. Spiega riguarda una mossa sola: andando a un'altra mossa si spegne, e la chiedi di nuovo.

**Modifica ▸ Copia ▸ Spiegazione** copia il testo.

# Motori {#engine}

**Motore ▸ Analizza** (Ctrl+E) avvia e ferma l'analisi della posizione sulla scacchiera. Il pannello Motore mostra il punteggio — sempre dal lato del Bianco: positivo è buono per il Bianco —, la profondità e la linea migliore. La barra accanto alla scacchiera mostra lo stesso punteggio.

Pragma Chess arriva con Stockfish, e funziona con qualunque motore UCI.

- **Motore ▸ Usa motore** sceglie tra i motori di questo computer. L'icona del motore nella barra degli strumenti apre lo stesso elenco.
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

# Libri delle aperture {#books}

Un libro delle aperture è un file Polyglot `.bin`: le mosse conosciute in ogni posizione, ciascuna con un peso. Il menu **Libro** elenca i libri della tua cartella dei libri; l'icona del libro nella barra degli strumenti apre lo stesso elenco.

- **Libro ▸ Nuovo libro…** e **Libro ▸ Apri libro…** creano un libro o ne aprono uno da qualunque posizione.
- **Libro ▸ Nessun libro** lavora senza.

Il pannello **Albero delle aperture** mostra, per la posizione sulla scacchiera, ogni mossa del libro con la sua parte del peso, il nome dell'apertura a cui porta e come sono andate dopo quella mossa le partite del database aperto (partite, poi vittorie del Bianco / patte / vittorie del Nero). Clicca una mossa per giocarla; la prima riga ritira l'ultima mossa.

**Il tuo repertorio**: clic destro su una mossa dell'Albero delle aperture e scegli **Metti nel repertorio**. Le mosse del repertorio sono elencate per prime, in grassetto. Il segno è scritto nel file del libro e gli altri programmi lo ignorano.

**I pesi**: lo stesso menu ha **Regola peso**, con +5%, +10%, +25%, +50%, +100%, gli stessi in negativo e **Azzera peso**. La percentuale è della quota della mossa stessa, e la somma della posizione resta sempre 100%: quel che una mossa guadagna lo cedono le altre in proporzione a quanto hanno — le più pesanti di più — e quel che perde torna a loro allo stesso modo. Una mossa allo 0%, o con meno dell'1%, non può crescere per percentuale di sé stessa, quindi un aumento prima le ruba l'1% dalle altre e cresce da lì; una diminuzione di una mossa allo 0% non fa nulla. Azzera peso dà tutta la quota della mossa alle altre che ne hanno. Le modifiche sono scritte nel libro in uso. L'elenco viene riordinato, e la mossa che hai cambiato brilla per un momento ed è segnata, a sinistra del suo peso, con ↑ se è salita, ↓ se è scesa, = se è rimasta dov'era; il segno resta finché non lasci la posizione.

# Nomi delle aperture {#opening-names}

Il pannello Motore dice in quale apertura si trova la partita, e l'Albero delle aperture dice dove porta ogni mossa.

I nomi vengono da un database di linee con un nome. **Opzioni ▸ Nomi delle aperture** sceglie quale: inglese, italiano o nessuno. Finché non scegli, i nomi seguono la lingua dell'interfaccia.

Un database dei nomi è un normale database di tipo Libro d'aperture: puoi aprirlo e aggiungere le tue linee, mettendo il nome nel campo Evento e il codice in ECO.

# Fonti di partite {#sources}

Una fonte porta nel database aperto le tue partite da un sito e le tiene aggiornate.

**Database ▸ Collega fonte…** ne aggiunge una:

- **lichess.org**: accedi con il tuo account;
- **chess.com**: il tuo nome utente;
- **torneionline.com**: il tuo numero FIDE o FSI, per le partite dei tornei che hai giocato.
- **File ChessBase**: un database ChessBase (`.cbh` e i suoi file) su questo computer. Scegli il file `.cbh`: le sue partite vengono copiate, i file restano dove sono e le partite aggiunte in seguito arrivano alla sincronizzazione successiva. Su un altro computer il file non c'è: la sincronizzazione lo dice e propone di ignorare la fonte su quel computer; *Database ▸ Gestisci fonti… ▸ Modifica…* sceglie di nuovo il file.

Le fonti vengono lette quando il database viene aperto e ogni venti minuti. Una partita non viene mai importata due volte. **Database ▸ Gestisci fonti…** sincronizza subito una fonte, la modifica, rifà l'accesso o la rimuove; le partite già importate restano.

Il nodo **Fonti** dell'albero elenca le partite di ogni fonte.

# Cestino {#trash}

Clic destro su una partita della lista e scegli **Cestina partita**. La partita esce da tutte le liste e non viene più cercata.

Il nodo **Cestino**, ultimo dell'albero, mostra le partite cestinate: **Recenti**, buttate negli ultimi sette giorni, e **Vecchie**. Lì puoi scegliere **Ripristina partita** o **Elimina partita…**.

Eliminare non rimpicciolisce ancora il file. **Database ▸ Impostazioni database… ▸ Ottimizza database** toglie per sempre le partite eliminate e compatta il file. Fino ad allora non si perde niente.

# Progetti {#projects}

Un progetto è quello che stai guardando: il database, la partita e la mossa, il lato da cui vedi la scacchiera, il motore, i pannelli e se ti stai allenando. La barra del titolo ne mostra il nome, con un asterisco quando ha modifiche non salvate.

- **File ▸ Nuovo progetto** parte dalla disposizione predefinita.
- **File ▸ Apri progetto…** e **File ▸ Apri recenti** aprono un file `.pch`.
- **File ▸ Salva progetto** e **File ▸ Salva progetto come…** lo salvano.

Non sei obbligato a salvare: Pragma Chess si riapre come l'hai chiuso.

I pannelli si possono ridimensionare e chiudere; il menu **Visualizza** (Mosse, Albero delle aperture, Motore, Lista partite) li mostra di nuovo, e **Visualizza ▸ Ripristina disposizione dei pannelli** li rimette dove stanno all'inizio.

# Sincronizzazione {#sync}

La sincronizzazione tiene uguale la tua cartella Pragma — database e progetti — su più computer, attraverso una cartella su un server.

**File ▸ Sincronizza…** la imposta: un server **FTP** (anche con TLS), un server **WebDAV** o un **repository Git**. **Prova connessione** la verifica.

**Sincronizza ora** (Ctrl+Y, primo bottone della barra degli strumenti) fa tutto in ordine: legge le fonti, salva il progetto e scambia i file con il server. **Sincronizza prima di chiudere** lo fa ogni volta che esci.

La sincronizzazione non elimina mai: un database che manca da una parte viene copiato lì, e un database modificato su due computer viene unito partita per partita. Per liberarti di una partita usa il cestino, che gli altri computer seguono.

**Opzioni ▸ Collega app mobile…** mostra un codice da inquadrare con Pragma Chess sul telefono, che da quel momento tiene una copia dei tuoi database.

# Lingua {#language}

**Opzioni ▸ Lingua** sceglie la lingua dell'interfaccia. Viene applicata al prossimo avvio di Pragma Chess. Questa guida e i nomi delle aperture la seguono.

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
