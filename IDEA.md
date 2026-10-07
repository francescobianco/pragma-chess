# Partite asincrone P2P con mosse condizionali e ledger distribuito

## Idea generale

Il sistema introduce una forma di gioco per corrispondenza completamente asincrona, senza orologio e senza server centrale.

Ogni giocatore esegue localmente un nodo della rete. I nodi comunicano tra loro in peer-to-peer e mantengono sincronizzato un ledger temporaneo contenente esclusivamente le partite attualmente aperte o in corso.

Le partite terminate non appartengono più al ledger della rete: vengono materializzate come normali partite nei database locali.

La rete rappresenta quindi il **presente**:

```text
partite aperte
partite in corso
mosse appena giocate
stato corrente delle partite
```

Il database rappresenta invece la **storia**:

```text
partite concluse
analisi
annotazioni
varianti
collezioni
ricerche
```

Non esiste quindi un archivio storico centrale.

---

# 1. Partita asincrona senza tempo

Una partita non possiede necessariamente un controllo del tempo.

Un giocatore può pubblicare una nuova partita:

```text
OPEN_GAME
```

specificando almeno:

```text
game_id
creator
colore desiderato
posizione iniziale
regole
eventuali metadati
```

Normalmente la posizione iniziale sarà quella standard, ma nulla impedisce in futuro di partire da una FEN arbitraria.

La partita compare nel ledger distribuito come partita disponibile.

Gli altri utenti della rete la vedono:

```text
OPEN GAMES

A - waiting
B - waiting
C - waiting
```

Un altro giocatore può chiedere di prenderla.

È preferibile non considerare immediatamente definitivo il primo `ACCEPT` ricevuto, perché in una rete distribuita due giocatori potrebbero accettare contemporaneamente.

Il modello più sicuro è:

```text
OPEN_GAME
     ↓
JOIN_REQUEST
     ↓
MATCH
```

Il creatore della partita è l'autorità che decide quale richiesta accettare.

Il suo nodo può farlo automaticamente secondo una policy:

```text
first valid request
```

oppure chiedere conferma all'utente.

Quando viene emesso:

```text
MATCH {
    game_id
    white
    black
}
```

la partita diventa attiva.

---

# 2. Nessun timeout scacchistico

Non esiste necessariamente:

```text
5 minuti
24 ore per mossa
3 giorni per mossa
```

Una partita può rimanere ferma fino a quando uno dei giocatori decide di continuare.

Lo stato può semplicemente mostrare:

```text
Last move: 4 days ago
Waiting for Black
```

L'inattività non determina automaticamente la sconfitta.

La partita termina esclusivamente attraverso normali eventi scacchistici:

```text
checkmate
stalemate
draw
resignation
agreement
abort
```

oppure altri risultati eventualmente previsti dalle regole.

---

# 3. Mosse condizionali

La caratteristica fondamentale è la possibilità di preparare in anticipo non soltanto una mossa, ma un **albero di decisioni**.

Per esempio dopo:

```text
1.e4
```

il Bianco può preparare:

```text
if ...e5
    2.Nf3

if ...c5
    2.Nf3

if ...c6
    2.d4
```

Queste non sono semplici varianti di analisi.

Sono decisioni future già prese.

Il significato è:

```text
SE l'avversario gioca X
ALLORA io autorizzo Y
```

La mossa dell'avversario è quindi una **condizione**, mentre la propria mossa successiva è un **commitment operativo**.

---

# 4. Alberi più profondi

Il meccanismo può essere arbitrariamente profondo.

Per esempio:

```text
1.e4

├─ if ...e5
│    2.Nf3
│
│    ├─ if ...Nc6
│    │    3.Bb5
│    │
│    └─ if ...Nf6
│         3.Nxe5
│
├─ if ...c5
│    2.Nf3
│
│    ├─ if ...d6
│    │    3.d4
│    │
│    └─ if ...Nc6
│         3.d4
│
└─ if ...c6
     2.d4
```

Il giocatore costruisce questo albero semplicemente muovendo i pezzi sulla scacchiera.

L'interfaccia non deve sembrare un linguaggio di programmazione.

Dal punto di vista interno, però, il significato è equivalente a:

```text
IF opponent_move == c7c5
THEN play g1f3
```

---

# 5. Il giocatore non può programmare le mosse dell'avversario

È importante distinguere una variante di analisi da una sequenza autorizzata.

Il giocatore non sta dicendo:

```text
io gioco e4
poi l'altro giocherà c5
poi io giocherò Nf3
```

Sta dicendo:

```text
io gioco e4

SE lui decide autonomamente di giocare c5
ALLORA autorizzo Nf3
```

L'avversario resta sempre libero di scegliere qualsiasi mossa legale.

---

# 6. Esecuzione automatica

Supponiamo che il Bianco abbia preparato:

```text
if ...c5 → Nf3
```

Il Nero gioca:

```text
...c5
```

Il nodo del Bianco riceve l'evento.

Verifica:

```text
questa è la posizione attesa?
la mossa è legale?
esiste una risposta programmata?
```

Se la risposta è sì, genera automaticamente:

```text
Nf3
```

e firma la nuova mossa.

Il giocatore non deve essere davanti al computer.

È sufficiente che almeno uno dei suoi nodi autorizzati sia attivo.

---

# 7. Cascata di mosse automatiche

Entrambi i giocatori possono aver preparato varianti.

Può quindi verificarsi:

```text
Nero      → ...c5

Bianco    → Nf3       automatico
Nero      → ...d6     automatico
Bianco    → d4        automatico
Nero      → ...cxd4   automatico
Bianco    → Nxd4      automatico
```

La partita può avanzare di diversi semiturni senza intervento umano.

La catena si ferma non appena il giocatore al tratto non possiede una risposta programmata per quella posizione.

Per esempio:

```text
1.e4 c5
2.Nf3 d6
3.d4 cxd4
4.Nxd4 Nf6
```

se il Bianco non ha preparato una risposta a `...Nf6`:

```text
WAITING FOR WHITE
```

Quando torna, può decidere la mossa e contemporaneamente preparare nuove ramificazioni.

---

# 8. Le varianti vengono costruite durante la partita

Il sistema non richiede che tutto l'albero venga preparato all'inizio.

Può crescere progressivamente.

Per esempio:

```text
position P

3.d4

if ...cxd4 → 4.Nxd4
if ...Nc6  → 4.Nxc6
```

Più avanti:

```text
position Q

if ...Nf6 → e5
if ...g6  → Nc3
```

Anche l'avversario fa la stessa cosa.

La partita diventa quindi un processo nel quale entrambi costruiscono progressivamente piccoli programmi scacchistici.

---

# 9. Le varianti preparate devono rimanere segrete

Le mosse future non devono essere mostrate all'avversario.

Se il Bianco ha preparato:

```text
if ...Nc6 → Bb5
```

il Nero non deve sapere in anticipo che `Bb5` è stata programmata.

Altrimenti la conoscenza della variante influenzerebbe la sua scelta.

Le varianti condizionali appartengono quindi inizialmente allo stato privato del giocatore.

Il nodo locale conserva:

```text
game_id
position_hash
trigger_move
response_move
children
```

Per esempio:

```text
position = P123
trigger  = c7c5
response = g1f3
```

---

# 10. Commitment crittografico opzionale

Può essere utile dimostrare che una variante era stata preparata **prima** della mossa dell'avversario.

Si può quindi pubblicare nel ledger solamente un commitment crittografico.

Per esempio:

```text
commitment =
HASH(
    game_id
    position_hash
    trigger
    response
    nonce
)
```

La rete vede soltanto:

```text
BRANCH_COMMIT {
    game_id
    position_hash
    commitment_hash
}
```

Non conosce:

```text
trigger
response
nonce
```

Quando la variante viene attivata, il giocatore pubblica:

```text
BRANCH_REVEAL {
    trigger
    response
    nonce
}
```

Ogni nodo può verificare:

```text
HASH(reveal) == commitment_hash
```

e quindi avere la prova che la decisione era stata presa precedentemente.

Per alberi complessi è possibile pubblicare una sola **Merkle root** dell'intero albero privato e rivelare successivamente soltanto i rami effettivamente percorsi.

Questo meccanismo non è indispensabile per una prima implementazione, ma l'architettura dovrebbe consentirlo.

---

# 11. Limite fondamentale dell'automazione

Se le varianti rimangono segrete, qualche nodo controllato dal giocatore deve essere online per eseguirle.

Non è possibile contemporaneamente ottenere:

```text
variante completamente segreta

+

nessun nodo del giocatore online

+

rete pubblica capace di eseguirla automaticamente
```

senza introdurre sistemi crittografici o infrastrutture molto più complicate.

Il modello semplice è quindi:

```text
varianti private
        +
nodo personale sempre acceso
```

Il computer di casa può svolgere perfettamente questa funzione.

---

# 12. Identità crittografica

Non è necessario un account centrale.

Ogni identità è rappresentata da una coppia:

```text
private key
public key
```

La public key identifica il giocatore.

La private key firma gli eventi.

Per esempio:

```text
identity = public_key
```

Ogni evento contiene:

```text
author_public_key
signature
```

e può essere verificato indipendentemente da qualsiasi nodo.

Una scelta pratica potrebbe essere:

```text
Ed25519       firme
BLAKE3/SHA256 hash
```

ma il protocollo non dovrebbe dipendere concettualmente da uno specifico algoritmo.

---

# 13. Più dispositivi per lo stesso giocatore

Una persona può possedere:

```text
desktop di casa
laptop
telefono
altro desktop
```

Tutti possono appartenere alla stessa identità.

È preferibile però non copiare semplicemente la chiave principale ovunque.

Si può avere:

```text
master identity
      │
      ├── device key A
      ├── device key B
      └── device key C
```

La chiave principale autorizza le chiavi dispositivo.

Un evento può quindi essere firmato dal dispositivo e verificato attraverso la delega dell'identità principale.

---

# 14. Evitare mosse concorrenti dai propri dispositivi

Due dispositivi dello stesso giocatore potrebbero essere temporaneamente scollegati e produrre due mosse diverse dalla stessa posizione.

Per esempio:

```text
device A → Nf3
device B → Bc4
```

Non bisogna affidarsi al timestamp per decidere quale sia corretta.

Una soluzione semplice consiste nell'avere per ogni partita un **controller device**.

Solo quel dispositivo è autorizzato a produrre mosse.

Il controllo può essere trasferito attraverso:

```text
HANDOFF
```

firmato.

In alternativa, se vengono rilevati due eventi validamente firmati dallo stesso giocatore e riferiti allo stesso stato precedente, la partita entra nello stato:

```text
CONFLICTED
```

e richiede una risoluzione esplicita.

Non deve esistere una regola "first seen wins", perché peer diversi potrebbero aver visto per prima una mossa differente.

---

# 15. Struttura di un evento

Ogni modifica dello stato della partita è rappresentata da un evento immutabile.

Una struttura minimale può essere:

```text
GameEvent {
    version
    event_id
    game_id
    previous_event
    author
    type
    payload
    signature
}
```

`event_id` può essere l'hash della rappresentazione canonica dell'evento.

Per esempio:

```text
event_id = HASH(serialized_event_without_signature)
```

oppure includendo la firma secondo il formato scelto.

Gli eventi non vengono modificati.

Se cambia qualcosa viene generato un nuovo evento.

---

# 16. Tipi di evento fondamentali

Il protocollo può partire con pochi eventi:

```text
OPEN_GAME
JOIN_REQUEST
MATCH

MOVE

BRANCH_COMMIT
BRANCH_REVEAL

DRAW_OFFER
DRAW_ACCEPT
DRAW_REJECT

RESIGN
ABORT

GAME_FINISHED

DEVICE_HANDOFF
```

In futuro possono essere aggiunti eventi senza modificare quelli precedenti grazie al campo:

```text
version
```

---

# 17. Rappresentazione delle mosse

Sul protocollo sarebbe meglio utilizzare una rappresentazione non ambigua, per esempio coordinate origine-destinazione:

```text
e2e4
g1f3
e7e8q
```

La SAN:

```text
e4
Nf3
Qxe7+
```

può essere derivata localmente.

Non dovrebbe essere utilizzata come rappresentazione canonica dell'evento.

---

# 18. Validazione deterministica

Ogni nodo conosce:

```text
posizione iniziale
+
sequenza delle mosse
```

quindi può verificare autonomamente ogni evento.

Quando riceve:

```text
MOVE c7c5
```

verifica:

```text
autore corretto?
tocca a quel giocatore?
mossa legale?
previous_event corretto?
firma valida?
game_id corretto?
```

Solo se tutti i controlli passano l'evento entra nello stato valido.

Questo elimina la necessità di un arbitro centrale.

Gli scacchi sono particolarmente adatti a questo modello perché, data una posizione e una mossa, la transizione di stato è completamente deterministica.

---

# 19. Ledger distribuito

Ogni peer mantiene una copia del ledger delle **partite vive**.

Non è una blockchain tradizionale.

Non esistono:

```text
mining
proof-of-work globale
token
blocchi
consenso economico
```

È semplicemente un:

> replicated signed event log

Ogni peer conserva gli eventi relativi alle partite attive.

---

# 20. Il ledger contiene esclusivamente il presente

Il ledger globale non deve diventare l'archivio storico dell'intera rete.

Contiene soltanto:

```text
OPEN
MATCHED
ACTIVE
```

cioè:

```text
sfide ancora disponibili
partite attualmente giocate
eventi necessari a ricostruirne lo stato
```

Quando una partita termina, viene rimossa dal dataset attivo.

Questo mantiene naturalmente limitata la quantità di informazioni sincronizzate.

---

# 21. Stati della partita

Una possibile macchina a stati è:

```text
LOCAL_DRAFT
     ↓
OPEN
     ↓
MATCHED
     ↓
ACTIVE
     ↓
FINISHED
```

Con stati laterali:

```text
ABORTED
CONFLICTED
```

`LOCAL_DRAFT` non appartiene alla rete.

`OPEN`, `MATCHED` e `ACTIVE` appartengono al ledger distribuito.

`FINISHED` viene trasferito nel database storico.

---

# 22. Fine della partita

Quando una partita termina viene generato:

```text
GAME_FINISHED {
    game_id
    result
    final_event
    final_position_hash
}
```

Tutti i nodi verificano l'evento.

Poi ricostruiscono la partita:

```text
initial position
+
moves
+
result
+
metadata
```

e la materializzano come normale record del database.

Per esempio:

```text
PGN
```

o nel formato interno utilizzato dal programma.

---

# 23. Dalla rete al database

Il ciclo completo è quindi:

```text
OPEN GAME
     ↓
LIVE NETWORK
     ↓
moves
     ↓
moves
     ↓
GAME FINISHED
     ↓
materializzazione
     ↓
LOCAL DATABASE
```

La partita smette di essere un oggetto della rete e diventa un documento storico.

---

# 24. Tutti possono osservare le partite

Poiché tutti i peer sincronizzano il ledger attivo, qualunque nodo possiede già le informazioni necessarie per visualizzare una partita in corso.

Non deve necessariamente collegarsi direttamente ai due giocatori.

La schermata può mostrare:

```text
LIVE

Player A — Player B
22...Nc6

Player C — Player D
31.Rxf7+

Player E — Player F
Waiting for Black
```

Se l'utente apre una partita, la posizione viene ricostruita localmente dal ledger.

---

# 25. Le partite pubbliche diventano automaticamente materiale del database

Quando una partita pubblica termina, ogni peer che ne possiede il log può convertirla in una partita normale.

Quindi il database locale può progressivamente contenere anche partite giocate dagli altri utenti.

La politica può essere configurabile:

```text
save every completed public game

save only observed games

save only my games

ask before saving
```

Questo non cambia il protocollo.

Il punto fondamentale è che **la rete non rimane responsabile della partita storica**.

---

# 26. Storico e rete rimangono concettualmente separati

Il database può crescere nel tempo:

```text
10.000 games
100.000 games
1.000.000 games
```

senza aumentare il ledger sincronizzato.

Il ledger invece dipende soltanto dal numero delle partite attualmente vive.

Per esempio:

```text
database locale:
250.000 partite

ledger della rete:
842 partite attive
```

---

# 27. Gestione delle partite chiuse per peer rimasti indietro

Esiste un problema importante.

Un peer può andare offline quando la partita è ancora attiva:

```text
Game ABC → ACTIVE
```

Nel frattempo la partita termina.

Quando quel peer ritorna potrebbe tentare di ripubblicare:

```text
Game ABC → ACTIVE
```

Per impedirlo è utile mantenere una piccola informazione di chiusura:

```text
CLOSED {
    game_id
    final_event_hash
}
```

Questo record non contiene tutta la partita.

È semplicemente un **tombstone**.

Può essere conservato per molto più tempo del log attivo perché occupa pochissimo spazio.

In alternativa ogni nodo può consultare il proprio database storico per verificare se possiede già una versione conclusa di quel `game_id`.

Una soluzione robusta può usare entrambe le strategie.

---

# 28. Peer-to-peer puro

Non esiste un server centrale.

Ogni processo attivo è contemporaneamente:

```text
client
server
replica
observer
router
```

La topologia può essere:

```text
       A────B
      / \  / \
     C───D────E
      \      /
       F────G
```

Ogni nodo mantiene alcune connessioni con altri peer.

Non è necessario essere collegati direttamente a tutti.

---

# 29. Gossip

Quando un nodo genera un nuovo evento:

```text
A creates MOVE X
```

lo invia ai propri peer:

```text
A → B
A → C
A → D
```

B lo propaga ai propri:

```text
B → E
B → F
```

e così via.

Ogni evento possiede un identificatore univoco.

Se un nodo riceve un evento già conosciuto:

```text
event_id already known
```

lo ignora.

In questo modo gli eventi convergono progressivamente su tutta la rete.

---

# 30. Non è necessario trasmettere sempre tutto

Il gossip diretto di ogni evento a tutti i peer può diventare inefficiente.

Una implementazione migliore può utilizzare:

```text
INVENTORY
WANT
EVENT
```

Per esempio:

```text
A → B

I have:
event 123
event 124
event 125
```

B risponde:

```text
I need:
124
125
```

A manda soltanto quelli mancanti.

---

# 31. Sincronizzazione quando due peer si collegano

Quando due peer stabiliscono una connessione si scambiano un manifesto:

```text
game_id        state        head

A123           ACTIVE       h98f...
A124           OPEN         h27d...
A125           ACTIVE       h61b...
```

I due nodi confrontano i rispettivi stati.

Per ogni differenza richiedono soltanto gli eventi mancanti.

Per esempio:

```text
Node A:
Game X HEAD = event 42

Node B:
Game X HEAD = event 39
```

B richiede:

```text
40
41
42
```

---

# 32. Snapshot

Per partite particolarmente lunghe non è necessario trasferire sempre tutta la cronologia per ricostruire la posizione.

Si possono utilizzare snapshot verificabili:

```text
Snapshot {
    game_id
    at_event
    FEN
    state_hash
}
```

Un nuovo peer può ricevere:

```text
snapshot
+
eventi successivi
```

Il log completo può comunque essere richiesto se necessario.

---

# 33. Discovery dei peer

Il peer-to-peer non elimina il problema del **primo contatto**.

Un nodo appena installato deve sapere almeno dove trovare un altro nodo.

Non esiste una soluzione magica a questo problema.

Si possono combinare diversi meccanismi.

- peer conosciuti salvati localmente;
- discovery LAN tramite mDNS;
- invito diretto contenente indirizzo + public key;
- DHT;
- peer exchange;
- pochi seed iniziali distribuiti insieme al programma.

I seed non sono server applicativi.

Non possiedono utenti, partite o autorità.

Servono esclusivamente per:

```text
"conosci qualche peer della rete?"
```

Dopo il primo contatto il nodo riceve gli indirizzi di altri peer e può smettere completamente di usare il seed iniziale.

Qualunque nodo pubblico può quindi svolgere temporaneamente funzione di bootstrap.

---

# 34. Peer exchange

Quando due nodi si collegano possono scambiarsi una parte delle rispettive peer table:

```text
PEERS {
    pubkey A
    addresses [...]

    pubkey B
    addresses [...]

    pubkey C
    addresses [...]
}
```

In questo modo la conoscenza della rete si diffonde senza directory centrale.

---

# 35. Connessioni attraverso NAT

Molti utenti saranno dietro:

```text
router
NAT
firewall
CGNAT
```

La rete dovrebbe tentare progressivamente:

```text
IPv6 diretto

connessione TCP/QUIC diretta

UPnP / NAT-PMP / PCP

UDP hole punching

multi-hop attraverso altri peer
```

Un nodo raggiungibile pubblicamente è particolarmente utile alla rete.

Lasciare un computer acceso a casa può quindi contribuire concretamente alla connettività degli altri nodi.

---

# 36. I peer possono fungere da trasporto senza essere server

Supponiamo:

```text
A non può raggiungere direttamente B
```

ma:

```text
A ↔ X
X ↔ Y
Y ↔ B
```

Un evento può propagarsi:

```text
A → X → Y → B
```

X e Y non devono conoscere alcuna logica privata della partita oltre alle informazioni pubbliche necessarie alla replicazione.

Non esiste quindi un relay speciale.

**La rete stessa è il relay.**

---

# 37. Asincronia reale

Questo risolve anche il caso:

```text
10:00
A gioca
B offline

18:00
A offline
B online
```

Se al momento della mossa esistevano altri peer:

```text
A → C
A → D
A → E
```

C, D ed E possiedono l'evento.

Alle 18:00:

```text
B → D
```

e B riceve la mossa.

Quindi il funzionamento asincrono deriva direttamente dalla replicazione del ledger attivo.

Non serve una mailbox centrale.

---

# 38. Disponibilità della rete

L'assenza di infrastruttura centrale comporta una proprietà che deve essere resa esplicita all'utente.

La rete esiste perché gli utenti la tengono viva.

Più nodi rimangono accesi, maggiore è:

```text
disponibilità
replicazione
ridondanza
capacità di discovery
capacità di attraversamento della rete
```

Il programma dovrebbe comunicare chiaramente:

```text
Network peers: 18
Connected peers: 6
Replication: healthy
```

oppure:

```text
No peers reachable.
Network synchronization unavailable.
```

Non deve fingere che esista un cloud invisibile.

---

# 39. Nodo personale sempre acceso

Un computer lasciato permanentemente acceso può diventare naturalmente il nodo principale dell'utente.

Per esempio:

```text
             home node
                │
        ┌───────┼────────┐
        │       │        │
      laptop  phone    tablet
```

Il nodo di casa:

```text
mantiene le connessioni
riceve le mosse
propaga gli eventi
mantiene il ledger
esegue le mosse programmate
```

Il telefono o il laptop possono collegarsi successivamente e sincronizzarsi.

---

# 40. Differenza fra disponibilità della partita e automazione privata

Anche se il ledger pubblico continua a esistere grazie agli altri peer, una **variante privata programmata** può essere eseguita soltanto da un nodo che la conosce.

Quindi:

```text
rete viva
+
utente offline
```

permette comunque di ricevere e conservare una mossa.

Ma:

```text
variante privata
+
nessun nodo personale online
```

non permette di eseguire automaticamente quella variante.

Quando uno dei dispositivi personali torna online:

```text
riceve l'evento
vede il trigger
esegue la risposta programmata
```

---

# 41. Pubblicità delle partite e segretezza delle decisioni

Sono due livelli differenti.

La partita attiva è pubblica:

```text
players
actual moves
position
result
```

Le decisioni future rimangono private:

```text
candidate branches
conditional moves
private annotations
private analysis
```

Quando una variante viene effettivamente percorsa, la mossa reale diventa naturalmente pubblica.

---

# 42. Il database può conservare anche le varianti private

Quando una partita termina, il record pubblico contiene la linea realmente giocata.

Il database personale del giocatore può però aggiungere:

```text
varianti preparate
rami non utilizzati
note
tempo dedicato all'analisi
commitment
```

Quindi la propria copia della partita può essere più ricca della copia posseduta dagli spettatori.

Questo è particolarmente interessante perché trasforma la partita in materiale di studio.

---

# 43. Il ledger globale non deve necessariamente essere una singola catena

Non serve costruire:

```text
block 1
   ↓
block 2
   ↓
block 3
```

Le partite sono indipendenti.

È più naturale avere:

```text
Game A:
E1 → E2 → E3 → E4

Game B:
F1 → F2 → F3

Game C:
G1 → G2 → G3 → G4 → G5
```

Il ledger globale è semplicemente l'insieme di questi log attivi.

Non serve determinare un ordine globale tra:

```text
mossa della partita A
mossa della partita B
```

perché non hanno alcuna relazione.

Questo elimina quasi tutto il problema del consenso globale.

---

# 44. Consenso locale alla singola partita

Per ogni partita conta soltanto:

```text
qual è l'ultimo evento valido?
chi deve muovere?
qual è la prossima transizione valida?
```

Quindi il consenso è locale alla partita.

Non c'è bisogno che tutta la rete concordi se:

```text
Game A event 12
```

sia avvenuto prima o dopo:

```text
Game B event 33
```

È irrilevante.

---

# 45. Eventi paralleli non scacchistici

Alcuni eventi potrebbero non appartenere alla linea principale delle mosse:

```text
chat
commenti
spectator messages
metadata
```

Questi possono eventualmente essere modellati come stream separati.

La linea scacchistica deve invece rimanere rigorosamente deterministica:

```text
position
 ↓
move
 ↓
position
 ↓
move
```

Separare i due livelli semplifica enormemente la validazione.

---

# 46. Dati locali possibili

Una implementazione pratica può utilizzare localmente tabelle concettualmente simili a:

```text
identities
devices
peers

active_games
game_events
game_heads

private_branches
branch_commits

closed_games

archived_games
```

Il database concreto può essere SQLite o qualsiasi altro sistema locale.

La rete non dipende dal database utilizzato internamente.

---

# 47. Albero privato delle mosse programmate

Una struttura locale potrebbe essere:

```text
ConditionalNode {
    id
    game_id
    position_hash

    trigger_move
    response_move

    children[]
}
```

Oppure:

```text
position_hash
    ↓
map<trigger_move, response>
```

Esempio:

```text
P123:

c7c5 → g1f3
e7e5 → g1f3
c7c6 → d2d4
```

Dopo l'esecuzione:

```text
new_position_hash
```

viene utilizzato per cercare eventuali condizioni successive.

---

# 48. Hash della posizione

La posizione dovrebbe possedere un identificatore deterministico.

È possibile derivarlo da una rappresentazione canonica contenente almeno:

```text
piece placement
side to move
castling rights
en-passant state
```

Non basta quindi guardare soltanto la posizione visiva dei pezzi.

Due posizioni graficamente identiche possono avere diritti di arrocco differenti e quindi non essere equivalenti.

---

# 49. Eventi idempotenti

Ogni operazione di rete deve essere idempotente.

Ricevere dieci volte:

```text
event X
```

deve produrre lo stesso risultato che riceverlo una volta.

La regola è semplicemente:

```text
if event_id already exists:
    ignore
```

Questo rende il gossip molto più semplice.

---

# 50. Eventi fuori ordine

In una rete distribuita è normale ricevere:

```text
event 42
```

prima di:

```text
event 41
```

Il nodo non deve considerarlo immediatamente invalido.

Può conservarlo in:

```text
pending_events
```

e richiedere:

```text
previous_event
```

Quando arriva la dipendenza mancante, l'evento viene validato.

---

# 51. Sicurezza

Ogni evento ricevuto deve essere considerato ostile fino alla verifica.

Bisogna controllare:

```text
dimensione massima
firma
hash
formato
game_id
parent
autore
legalità della transizione
```

Non bisogna permettere a un peer di costringere gli altri a conservare quantità arbitrarie di dati.

---

# 52. Spam

Una rete senza autorità centrale permette a chiunque di creare identità e pubblicare migliaia di partite.

Quindi sono necessari almeno meccanismi locali come:

```text
rate limit
limite OPEN_GAME per identità
block list
peer reputation locale
dimensione massima eventi
limiti di propagazione
```

Eventualmente può essere introdotto un piccolo proof-of-work esclusivamente per operazioni costose come:

```text
OPEN_GAME
```

senza trasformare l'intera rete in una blockchain.

Non è però necessario per una prima implementazione privata o sperimentale.

---

# 53. Partite abbandonate

Poiché non esiste timeout scacchistico, una partita potrebbe teoricamente rimanere attiva per anni.

Questo non deve obbligare tutti i peer a conservarla per sempre.

Bisogna distinguere:

```text
risultato della partita
```

da:

```text
politica di caching della rete
```

Una partita molto inattiva può diventare:

```text
DORMANT
```

e smettere di essere replicata aggressivamente.

I partecipanti ne conservano comunque lo stato completo localmente.

Quando uno di loro ritorna può ripubblicare:

```text
game_id
current_head
proof
```

e la partita torna attiva.

L'eventuale pruning non equivale quindi a sconfitta o timeout.

---

# 54. Stato live della rete

Il ledger permette di derivare localmente una homepage del tipo:

```text
PEERS ONLINE
47

OPEN GAMES
12

LIVE GAMES
86
```

Non arriva da un servizio web.

È semplicemente una vista dello stato distribuito che il nodo già possiede.

---

# 55. Nessun backend applicativo

La conseguenza architetturale più importante è che non esiste:

```text
application server
central database
user service
match service
game server
history server
```

Esistono soltanto:

```text
nodi
eventi firmati
gossip
database locali
```

La rete dei nodi costituisce l'infrastruttura.

---

# 56. Proprietà risultanti

Da questa architettura emergono automaticamente alcune caratteristiche:

```text
nessun proprietario della rete

nessun singolo punto di guasto

nessun database utenti centrale

nessun archivio partite centrale

possibilità di osservare le partite live

gioco asincrono

replicazione delle partite attive

possibilità di lasciare un nodo sempre acceso

mosse condizionali automatiche

varianti future private

verifica crittografica degli eventi

archiviazione locale delle partite concluse
```

---

# 57. Ciclo completo di una partita

Il flusso complessivo può essere rappresentato così:

```text
PLAYER A
creates game
    │
    ▼
OPEN_GAME
    │
    ├──────────── gossip ────────────► network
    │
    ▼
PLAYER B
JOIN_REQUEST
    │
    ▼
MATCH
    │
    ▼
ACTIVE GAME
    │
    ├── MOVE
    ├── MOVE
    ├── conditional automatic MOVE
    ├── MOVE
    ├── conditional automatic MOVE
    └── ...
    │
    ▼
GAME_FINISHED
    │
    ├──────────── gossip ────────────► network
    │
    ▼
materialize game
    │
    ▼
LOCAL DATABASES
    │
    ▼
remove from active ledger
```

---

# 58. Principio fondamentale

Il sistema non deve essere pensato come un sito di scacchi decentralizzato.

È più semplice pensarlo come:

> **un protocollo attraverso il quale programmi locali si scambiano decisioni scacchistiche firmate e mantengono sincronizzato lo stato delle partite attualmente vive.**

La rete non possiede le partite.

I giocatori possiedono le proprie identità e i propri database.

La rete serve esclusivamente a rendere condiviso il presente.

Le partite concluse tornano a essere normali oggetti locali.

---

# 59. La proprietà più originale

Il vero elemento nuovo non è semplicemente il P2P e nemmeno il gioco per corrispondenza.

È la possibilità di trattare una decisione scacchistica come qualcosa che può essere espresso anticipatamente:

```text
WHEN opponent does X
DO Y
```

Una variante quindi non è più soltanto:

> «questa posizione potrebbe continuare così».

Può diventare:

> «se questa posizione continuerà così, questa è già la mia decisione».

La variante passa da **annotazione passiva** ad **azione futura condizionata**.

Due giocatori possono costruire contemporaneamente alberi di decisioni e lasciare che i propri nodi li percorrano autonomamente mentre la partita avanza.

Il risultato è una forma di gioco asincrono nella quale una parte dell'attività scacchistica può essere preparata, firmata, verificata ed eseguita in anticipo, senza togliere all'avversario la libertà di scegliere qualsiasi mossa legale.




# Tornei asincroni a 4 giocatori e lobby dinamica

## Idea generale

L'unità principale del sistema non è la singola partita, ma un **torneo asincrono a 4 giocatori**.

Ogni torneo è una stanza con quattro posti.

I quattro partecipanti giocano un girone all'italiana completo di andata e ritorno.

Ogni coppia di giocatori disputa quindi due partite:

```text
A - B
B - A
```

Di conseguenza ogni giocatore disputa:

```text
6 partite
3 con il Bianco
3 con il Nero
```

e il torneo completo contiene:

```text
4 giocatori × 6 partite / 2
=
12 partite
```

Il torneo è quindi l'oggetto sociale fondamentale.

Le singole partite esistono esclusivamente come elementi interni del torneo.

---

# 1. La lobby contiene tornei, non partite

L'utente non pubblica una singola partita.

Entra invece in una lobby che mostra tornei nei quali esistono ancora posti disponibili.

Per esempio:

```text
AVAILABLE TOURNAMENTS

Room #A91
3 / 4 players

Room #B27
2 / 4 players
```

L'azione principale dell'utente è:

```text
JOIN
```

oppure, concettualmente:

```text
SIT AT TABLE
```

Non esiste quindi:

```text
Create game
```

come operazione primaria.

Esiste invece:

```text
join tournament
```

Il torneo costituisce una piccola unità sociale temporanea di quattro giocatori.

---

# 2. Non è necessario attendere quattro giocatori

Un torneo può iniziare appena sono presenti almeno due giocatori.

Per esempio:

```text
Tournament T1

Seat A: Alice
Seat B: Bob
Seat C: empty
Seat D: empty
```

Alice e Bob possono già iniziare a giocare.

Tra loro vengono automaticamente generate due partite:

```text
Alice - Bob
Bob - Alice
```

Non è necessario attendere gli altri due partecipanti.

---

# 3. Il torneo cresce progressivamente

Se successivamente entra Carol:

```text
Seat A: Alice
Seat B: Bob
Seat C: Carol
Seat D: empty
```

vengono generate automaticamente le partite tra Carol e tutti i partecipanti già presenti:

```text
Alice - Carol
Carol - Alice

Bob - Carol
Carol - Bob
```

Le partite precedentemente iniziate continuano normalmente.

Quando infine entra David:

```text
Seat A: Alice
Seat B: Bob
Seat C: Carol
Seat D: David
```

vengono generate:

```text
Alice - David
David - Alice

Bob - David
David - Bob

Carol - David
David - Carol
```

A quel punto il torneo possiede il calendario completo di 12 partite.

---

# 4. Regola deterministica di generazione delle partite

Quando un nuovo giocatore `X` entra nel torneo, per ogni giocatore già presente `Y` vengono generate due partite:

```text
X - Y
Y - X
```

Formalmente:

```text
on JOIN(X):

    for each existing player Y:

        create game:
            White = X
            Black = Y

        create game:
            White = Y
            Black = X
```

Non serve quindi memorizzare un calendario tradizionale a turni.

L'intero set di partite deriva deterministicamente dall'insieme dei partecipanti.

---

# 5. Nessun concetto di round

Il torneo non è organizzato in:

```text
Round 1
Round 2
Round 3
...
```

perché le partite sono asincrone.

Ogni partita procede indipendentemente dalle altre.

Un giocatore può avere contemporaneamente:

```text
vs Alice, White
your move

vs Alice, Black
waiting

vs Bob, White
move automatically executed

vs Bob, Black
your move

vs Carol, White
finished

vs Carol, Black
waiting
```

Non esiste alcun obbligo di completare una partita prima di iniziarne un'altra.

---

# 6. Fino a sei partite contemporanee per giocatore

Quando il torneo è completo, ogni giocatore può avere fino a sei partite contemporaneamente attive.

Per esempio:

```text
MY TOURNAMENT

vs Alice
  White   your move
  Black   waiting

vs Bob
  White   finished
  Black   waiting

vs Carol
  White   automatic line running
  Black   your move
```

Questa caratteristica è particolarmente compatibile con un sistema di gioco asincrono.

---

# 7. Compatibilità con mosse condizionali

Ogni singola partita mantiene il proprio meccanismo di mosse programmate.

Per esempio, in una delle sei partite il giocatore può avere preparato:

```text
if ...c5 → Nf3
if ...e5 → Nf3
if ...c6 → d4
```

Un'altra partita può avere un albero completamente differente.

Le partite procedono indipendentemente.

Quindi il torneo costituisce il contenitore, mentre ogni partita mantiene autonomamente:

```text
current position
side to move
conditional branches
result
state
```

---

# 8. Stato del torneo

Una macchina a stati semplice può essere:

```text
OPEN
RUNNING
FULL
FINISHED
```

Più precisamente:

```text
OPEN
1 / 4

RUNNING
2 / 4

RUNNING
3 / 4

FULL
4 / 4

FINISHED
12 / 12 games completed
```

È possibile anche introdurre:

```text
ABORTED
```

o altri stati particolari, ma non sono indispensabili per il modello base.

---

# 9. Stato delle singole partite

Ogni partita appartenente al torneo può invece essere:

```text
WAITING_TO_START
ACTIVE
FINISHED
ABORTED
```

In pratica, però, una partita può essere creata immediatamente quando entrambi i giocatori esistono.

Quindi normalmente passa direttamente a:

```text
ACTIVE
```

---

# 10. Il torneo come struttura dati

Concettualmente:

```text
Tournament
│
├── tournament_id
│
├── seats
│   ├── A
│   ├── B
│   ├── C
│   └── D
│
├── games
│   ├── A-B
│   ├── B-A
│   ├── A-C
│   ├── C-A
│   ├── A-D
│   ├── D-A
│   ├── B-C
│   ├── C-B
│   ├── B-D
│   ├── D-B
│   ├── C-D
│   └── D-C
│
└── standings
```

La partita non è quindi un'entità isolata della lobby.

Possiede sempre:

```text
tournament_id
```

---

# 11. La classifica

La classifica può essere calcolata continuamente.

Sistema standard:

```text
Win  = 1
Draw = 0.5
Loss = 0
```

Esempio:

```text
             P   W   D   L   Pts

Alice        4   3   0   1   3.0
Bob          3   1   2   0   2.0
Carol        4   1   1   2   1.5
David        1   0   1   0   0.5
```

Poiché il torneo è asincrono, i partecipanti possono avere giocato un numero differente di partite.

Questo non costituisce un problema.

La classifica definitiva esiste soltanto quando tutte le 12 partite sono concluse.

---

# 12. Nessun requisito che la stanza sia piena

Lo stato:

```text
2 / 4 players
```

non significa:

```text
waiting to start
```

significa semplicemente:

```text
tournament running
2 seats still available
```

Analogamente:

```text
3 / 4
```

significa:

```text
running
1 seat available
```

Questa distinzione è importante.

La stanza non è una waiting room.

È già il torneo.

---

# 13. L'ingresso di un nuovo giocatore modifica il torneo

Quando entra un nuovo partecipante, il torneo cresce.

Per esempio:

```text
prima:

A
B
```

esistono:

```text
A-B
B-A
```

Poi entra C:

```text
A
B
C
```

il sistema aggiunge:

```text
A-C
C-A
B-C
C-B
```

Il nuovo giocatore entra quindi in un torneo che potrebbe già avere partite concluse.

Questo è perfettamente valido.

---

# 14. Un giocatore può entrare mentre altri hanno già giocato

Esempio:

```text
Alice-Bob       1-0
Bob-Alice       active
```

Entra Carol.

Il torneo diventa:

```text
Alice-Bob       1-0
Bob-Alice       active

Alice-Carol     active
Carol-Alice     active

Bob-Carol       active
Carol-Bob       active
```

Non è necessario resettare nulla.

---

# 15. Chiusura dei posti

Quando vengono occupati tutti e quattro i seat:

```text
4 / 4
```

il torneo diventa:

```text
FULL
```

e nessun altro giocatore può entrare.

Le dodici partite possono però continuare per tutto il tempo necessario.

---

# 16. Fine del torneo

Il torneo termina quando tutte le dodici partite sono terminate.

Condizione:

```text
finished_games == 12
```

quindi:

```text
TOURNAMENT_FINISHED
```

Il torneo non termina quando viene giocata una determinata quantità di turni.

Non esistendo round, conta esclusivamente lo stato delle dodici partite.

---

# 17. Abbandono di un giocatore

Una volta che un partecipante ha iniziato almeno una partita, il suo seat non dovrebbe essere riassegnato a un altro giocatore.

Questo è importante perché l'identità dei partecipanti appartiene alla struttura stessa del torneo.

Quindi:

```text
Alice joins Seat A
```

significa che `Seat A` rimane associato ad Alice fino alla fine del torneo.

Se Alice decide di non continuare, può esistere:

```text
WITHDRAW
```

ma il suo posto non viene liberato.

Le partite rimanenti possono essere:

```text
forfeit
aborted
drawn
```

secondo la policy definita.

Il dettaglio può essere deciso successivamente.

La regola strutturale importante è:

> un seat utilizzato non cambia proprietario.

---

# 18. Lobby dinamica

Il numero delle stanze disponibili non è fisso.

La rete deve assicurare che esista sempre una minima capacità disponibile per nuovi utenti.

La policy iniziale può essere:

```text
MIN_JOINABLE_ROOMS = 2
```

Significa:

> devono esistere sempre almeno due tornei nei quali sia possibile entrare.

---

# 19. Definizione di stanza disponibile

Una stanza è `JOINABLE` quando:

```text
players < 4
```

e non è terminata o chiusa.

Quindi:

```text
1 / 4 → JOINABLE
2 / 4 → JOINABLE
3 / 4 → JOINABLE
4 / 4 → NOT JOINABLE
```

---

# 20. Regola di capacità della lobby

Si può calcolare:

```text
available_rooms =
    count(tournaments where joinable)
```

Poi:

```text
missing =
    MIN_JOINABLE_ROOMS
    -
    available_rooms
```

Se:

```text
missing <= 0
```

non bisogna fare nulla.

Se:

```text
missing > 0
```

bisogna rendere disponibili `missing` nuovi slot di stanza.

---

# 21. Esempio

Supponiamo:

```text
Room A   3/4
Room B   2/4
```

Esistono due stanze disponibili.

Quindi:

```text
available_rooms = 2
```

e la lobby è soddisfatta.

Un giocatore entra nella Room A:

```text
Room A   4/4
Room B   2/4
```

Ora:

```text
available_rooms = 1
```

manca una stanza.

La lobby deve quindi esporre una nuova possibilità:

```text
Room B   2/4
New Room
```

---

# 22. Le stanze vuote non devono necessariamente esistere

Questo è particolarmente importante in una rete distribuita.

Non è necessario creare realmente due tornei:

```text
0/4
0/4
```

nel ledger.

Una stanza completamente vuota può essere considerata semplicemente:

```text
available capacity
```

La lobby può mostrare:

```text
Room A     2/4
Room B     3/4

New Room
New Room
```

Le due `New Room` non sono ancora tornei reali.

---

# 23. Materializzazione di una stanza

Una stanza virtuale viene materializzata soltanto quando il primo giocatore decide di entrarvi.

Prima:

```text
New Room
```

Dopo il join:

```text
Room C
1 / 4
```

A quel punto viene creato realmente:

```text
TOURNAMENT_CREATED
```

con il primo seat occupato.

Questo evita di mantenere nella rete migliaia di tornei completamente vuoti.

---

# 24. Distinzione tra capacità e torneo

Quindi:

```text
0 / 4
```

non deve necessariamente corrispondere a un torneo reale.

La distinzione concettuale è:

```text
0 players
=
capacity

1-4 players
=
tournament
```

In altre parole:

> una stanza vuota non è ancora un torneo.

Diventa torneo nel momento in cui qualcuno occupa il primo seat.

---

# 25. Problema della creazione distribuita

In una rete P2P non conviene utilizzare una regola del tipo:

```text
if available_rooms < 2:
    create room
```

perché peer differenti possono osservare contemporaneamente lo stesso stato.

Per esempio:

```text
Peer A sees 1 available room
Peer B sees 1 available room
Peer C sees 1 available room
```

e tutti e tre potrebbero creare una stanza.

Risultato:

```text
4 available rooms
```

anziché due.

Quindi la capacità vuota dovrebbe essere **derivata**, non pubblicata.

---

# 26. Stanze virtuali deterministiche

Tutti i peer, dato lo stesso stato della rete, devono poter derivare autonomamente la stessa capacità disponibile.

Per esempio:

```text
existing joinable rooms = 1
minimum required = 2

virtual capacity needed = 1
```

La UI mostra quindi automaticamente:

```text
New Room
```

senza generare alcun evento.

Il torneo reale viene creato soltanto quando qualcuno usa quella capacità.

---

# 27. Identificatore della nuova stanza

È possibile assegnare il vero `tournament_id` soltanto quando arriva il primo join.

Per esempio:

```text
TOURNAMENT_CREATE {
    creator
    nonce
    timestamp_logical
    ...
}
```

da cui:

```text
tournament_id =
HASH(event)
```

La `New Room` mostrata nella lobby non deve necessariamente possedere un ID globale definitivo.

Può essere semplicemente un comando UI:

```text
CREATE_AND_JOIN
```

Questo semplifica ancora di più il modello.

---

# 28. Comportamento della lobby

Supponiamo:

```text
Room A    4/4
Room B    4/4
Room C    3/4
Room D    2/4
```

Solo C e D sono joinable.

La lobby mostra:

```text
Room C    3/4
Room D    2/4
```

Sono già due.

Non mostra nuove stanze.

Poi Room D diventa:

```text
4/4
```

Rimane:

```text
Room C    3/4
```

La lobby mostra:

```text
Room C    3/4
New Room
```

Qualcuno sceglie `New Room`.

Nasce:

```text
Room E    1/4
```

Ora nuovamente:

```text
Room C    3/4
Room E    1/4
```

quindi ci sono due possibilità reali di ingresso.

---

# 29. Autoscaling naturale

Questo produce una forma di autoscaling completamente decentralizzata.

Con pochi giocatori possono esserci:

```text
3 tornei
```

Con molti giocatori:

```text
100 tornei
500 tornei
1000 tornei
```

ma il sistema continua a mantenere alla frontiera:

```text
almeno 2 tornei joinable
```

Non serve un componente che conosca anticipatamente la dimensione della rete.

La capacità emerge dalla domanda.

---

# 30. Scalabilità verso l'alto

Quando molti utenti entrano rapidamente:

```text
join
join
join
join
```

le stanze vengono progressivamente riempite.

Appena il numero di stanze joinable scende sotto la soglia:

```text
MIN_JOINABLE_ROOMS
```

la lobby rende visibile nuova capacità.

Non è necessario creare preventivamente centinaia di stanze.

---

# 31. Scalabilità verso il basso

Quando i tornei diventano pieni:

```text
4 / 4
```

non fanno più parte della capacità disponibile.

Quando terminano:

```text
FINISHED
```

escono completamente dalla rete live.

Quindi il numero degli oggetti attivi diminuisce naturalmente.

Non è necessario un processo esplicito di scaling down.

---

# 32. Lobby come frontiera di ingresso

La lobby può mostrare esclusivamente i tornei nei quali è ancora possibile entrare.

Per esempio:

```text
JOIN A TOURNAMENT

Room #A71    3 / 4
Room #B19    1 / 4
```

I tornei pieni possono invece essere mostrati separatamente:

```text
LIVE TOURNAMENTS
```

Questo mantiene la lobby estremamente semplice.

---

# 33. Tre viste distinte

Il sistema può quindi essere concettualmente diviso in:

```text
LOBBY
tornei con posti disponibili

LIVE
tornei pieni o comunque in corso

HISTORY
tornei terminati
```

Naturalmente un torneo `2/4` o `3/4` è contemporaneamente:

```text
live
+
joinable
```

quindi le viste sono interfacce, non necessariamente stati mutuamente esclusivi.

---

# 34. Un torneo aperto è già live

È importante non introdurre una falsa separazione:

```text
waiting room
→
live tournament
```

Il torneo diventa live appena esistono almeno due giocatori e una partita può iniziare.

Quindi:

```text
1/4
```

è solo disponibile.

```text
2/4
```

è disponibile e live.

```text
3/4
```

è disponibile e live.

```text
4/4
```

è live ma non più disponibile.

---

# 35. Stato derivato

Molte informazioni non devono essere memorizzate come eventi espliciti.

Per esempio:

```text
JOINABLE
FULL
FINISHED
```

possono essere derivati.

Esempio:

```text
JOINABLE =
players < 4
AND tournament not finished
```

```text
FULL =
players == 4
```

```text
FINISHED =
number_of_finished_games == 12
```

Meno stato esplicito significa meno possibilità di divergenza tra peer.

---

# 36. Classifica derivata

Anche la classifica non deve necessariamente essere sincronizzata.

Ogni peer possiede i risultati delle partite e può calcolare localmente:

```text
points
wins
draws
losses
played
```

Quindi la rete deve sincronizzare soltanto i fatti:

```text
Game A finished 1-0
Game B finished 1/2-1/2
...
```

La classifica è una vista derivata.

---

# 37. Calendario derivato

Analogamente, non serve sincronizzare:

```text
12 game records
```

nel momento in cui il torneo viene creato.

Le partite possono essere derivate progressivamente dai partecipanti.

Dato:

```text
players = [A, B, C]
```

il set delle partite è automaticamente:

```text
A-B
B-A
A-C
C-A
B-C
C-B
```

Quando entra D vengono aggiunte:

```text
A-D
D-A
B-D
D-B
C-D
D-C
```

Questo rende il modello molto deterministico.

---

# 38. Identificatore delle partite

Anche l'identificatore della partita può essere derivato.

Per esempio:

```text
game_id =
HASH(
    tournament_id,
    white_player_id,
    black_player_id
)
```

Dato che in ogni torneo esiste una sola partita:

```text
A white vs B black
```

non serve altro.

La partita inversa:

```text
B white vs A black
```

produce automaticamente un altro ID.

Questo evita collisioni e rende tutti i peer capaci di identificare autonomamente le stesse partite.

---

# 39. Nessuna creazione manuale della singola partita

L'utente non deve mai fare:

```text
new game
```

La singola partita nasce automaticamente come conseguenza:

```text
player joins tournament
```

Questa è una caratteristica importante del modello.

La partita è un effetto della struttura sociale del torneo.

---

# 40. Esperienza utente

L'esperienza dovrebbe essere molto semplice.

L'utente apre la lobby:

```text
Room 31       3/4
Room 45       2/4
```

sceglie:

```text
JOIN Room 31
```

e immediatamente vede:

```text
You joined Room 31

Players:
Alice
Bob
You
Empty
```

Il sistema genera automaticamente:

```text
You - Alice
Alice - You

You - Bob
Bob - You
```

Non serve alcuna configurazione ulteriore.

---

# 41. Quando arriva il quarto

Se successivamente entra David:

```text
Alice
Bob
You
David
```

il tuo client aggiunge automaticamente:

```text
You - David
David - You
```

e lo stesso avviene per gli altri partecipanti.

---

# 42. La dashboard personale del torneo

Una possibile vista:

```text
TOURNAMENT #31

Players
Alice
Bob
Carol
David

My games

White vs Alice     your move
Black vs Alice     waiting

White vs Bob       1-0
Black vs Bob       waiting

White vs Carol     active
Black vs Carol     your move

Score
2.0 / 3

Tournament progress
5 / 12 finished
```

L'utente vede quindi il torneo come oggetto unico.

---

# 43. Osservazione del torneo

Gli altri utenti possono osservare l'intero torneo.

Invece di aprire semplicemente una partita, possono vedere:

```text
Tournament #31

Standings
Games
Players
Progress
```

e poi entrare in una singola partita se vogliono guardarla.

Questo rafforza ulteriormente il torneo come unità principale.

---

# 44. Fine e archiviazione

Quando:

```text
12 / 12 games finished
```

il torneo viene chiuso.

Le singole partite vengono trasformate in normali partite storiche nei database locali.

Si può inoltre materializzare un record torneo:

```text
TournamentResult {
    tournament_id
    players
    standings
    game_ids
    completed_at
}
```

Il record storico può quindi collegare le dodici partite.

---

# 45. La rete conserva solo i tornei vivi

Come per le partite asincrone, la rete deve rappresentare principalmente il presente.

Quindi contiene:

```text
joinable tournaments
active tournaments
active games
```

I tornei conclusi vengono trasferiti nei database locali.

Non devono rimanere indefinitamente nel dataset live.

---

# 46. Tombstone dei tornei conclusi

Per evitare che un vecchio peer torni online e riproponga un torneo già terminato, è possibile conservare temporaneamente:

```text
TOURNAMENT_CLOSED {
    tournament_id
    final_hash
}
```

È un piccolo tombstone.

Non contiene tutte le dodici partite.

Serve soltanto a comunicare:

```text
questo torneo non appartiene più allo stato live
```

---

# 47. Separazione concettuale

Il modello complessivo diventa:

```text
LOBBY
    ↓
TOURNAMENT
    ↓
PLAYERS
    ↓
GAMES
    ↓
RESULTS
    ↓
STANDINGS
    ↓
TOURNAMENT FINISHED
    ↓
LOCAL DATABASE
```

La partita non è più il livello principale dell'interfaccia.

Il torneo sì.

---

# 48. Regola strutturale fondamentale

L'unità sociale della rete è:

```text
4-player tournament
```

L'unità scacchistica è:

```text
game
```

L'unità di disponibilità della lobby è:

```text
joinable tournament slot
```

Sono tre concetti differenti.

---

# 49. La lobby non deve conoscere la domanda futura

Non è necessario dire:

```text
ci sono 100 utenti,
creiamo 30 stanze
```

La lobby osserva esclusivamente la capacità corrente:

```text
quante stanze accettano ancora giocatori?
```

e mantiene:

```text
MIN_JOINABLE_ROOMS
```

Questa regola è sufficiente per adattarsi automaticamente alla dimensione della rete.

---

# 50. Policy configurabile

Il valore:

```text
MIN_JOINABLE_ROOMS = 2
```

non deve necessariamente appartenere al protocollo.

Può essere una policy.

In futuro potrebbe diventare:

```text
2
5
10
```

oppure essere calcolato in funzione del traffico.

Per esempio:

```text
desired_capacity =
max(
    2,
    recent_join_rate / factor
)
```

Ma una prima implementazione può utilizzare semplicemente:

```text
2
```

che ha anche un significato molto comprensibile:

> ci sono sempre almeno due tavoli ai quali puoi sederti.

---

# 51. Perché due e non uno

Con una sola stanza disponibile, l'utente non avrebbe una vera scelta.

Con due:

```text
Room A    3/4
Room B    1/4
```

può scegliere se:

```text
entrare in un torneo quasi completo
```

oppure:

```text
entrare in uno appena iniziato
```

Questo crea una lobby più naturale senza moltiplicare inutilmente le stanze.

---

# 52. Possibili criteri futuri di scelta

Il modello consente in seguito di introdurre criteri come:

```text
rating range
variant
starting position
rated/unrated
language
friends-only
public/private
```

Questi criteri produrrebbero pool distinti.

Per esempio:

```text
Standard / Open

Room A
Room B
```

e:

```text
Chess960 / Open

Room C
Room D
```

Ogni pool potrebbe mantenere autonomamente:

```text
MIN_JOINABLE_ROOMS = 2
```

Ma questo non è necessario nella prima versione.

---

# 53. Torneo come piccolo tavolo sociale

Il modello può essere pensato meno come un torneo tradizionale e più come un:

> tavolo persistente da quattro giocatori.

Quando ti siedi non stai prenotando una singola partita.

Stai accettando una relazione scacchistica con tutti gli altri partecipanti:

```text
2 games against each opponent
```

Questo rende molto naturale la struttura:

```text
4 seats
6 games each
12 games total
```

---

# 54. Conseguenza importante

L'utente non cerca più:

> qualcuno con cui giocare una partita.

Cerca:

> un piccolo gruppo nel quale giocare sei partite asincrone.

Questo può generare un comportamento sociale molto diverso dai normali server di scacchi.

Lo stesso gruppo rimane insieme per l'intera durata delle dodici partite.

---

# 55. Il torneo come contesto persistente

Dal momento in cui entra, il giocatore vede sempre:

```text
the same three opponents
```

e segue:

```text
their games
the standings
the progress of the tournament
```

Quindi esiste una narrativa naturale:

```text
sono entrato
ho incontrato tre giocatori
abbiamo giocato dodici partite
è emerso un vincitore
```

nonostante tutto avvenga in modo completamente asincrono.

---

# 56. Principio finale

Il sistema non deve essere pensato come:

```text
un matchmaking che genera singole partite
```

ma come:

```text
una rete che mantiene una quantità dinamica
di tavoli da quattro giocatori
```

Ogni tavolo:

```text
accetta fino a 4 giocatori
parte già con 2
genera automaticamente le partite
mantiene una classifica
rimane aperto fino al completamento
```

La lobby mantiene sempre una piccola riserva di tavoli disponibili.

Il numero delle stanze cresce automaticamente quando la domanda aumenta e diminuisce naturalmente quando i tornei vengono completati.

La regola fondamentale può essere riassunta così:

```text
4 giocatori
6 partite per giocatore
12 partite totali

il torneo parte appena ci sono 2 giocatori

ogni nuovo giocatore genera
2 partite contro ogni partecipante già presente

tutte le partite sono indipendenti e asincrone

quando 12 partite terminano
il torneo termina

la lobby mantiene sempre
almeno 2 possibilità di ingresso
```

L'oggetto principale non è quindi la partita.

È il **tavolo da quattro giocatori**, dal quale le partite emergono automaticamente.




