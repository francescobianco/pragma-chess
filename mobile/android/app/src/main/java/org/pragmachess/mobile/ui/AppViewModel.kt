package org.pragmachess.mobile.ui

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.pragmachess.mobile.BuildConfig
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.GameLine
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.data.AppStore
import org.pragmachess.mobile.data.Computer
import org.pragmachess.mobile.data.DatabaseLocation
import org.pragmachess.mobile.data.DatabaseRef
import org.pragmachess.mobile.data.GameHeaders
import org.pragmachess.mobile.data.GameRecord
import org.pragmachess.mobile.data.GameSummary
import org.pragmachess.mobile.data.Library
import org.pragmachess.mobile.data.PdbDatabase
import org.pragmachess.mobile.engine.Analysis
import org.pragmachess.mobile.engine.UciEngine
import org.pragmachess.mobile.link.ComputerSync
import org.pragmachess.mobile.link.PairingLink
import org.pragmachess.mobile.link.PhoneIdentity
import org.pragmachess.mobile.link.SyncException
import org.pragmachess.mobile.link.SyncFailure
import org.pragmachess.mobile.link.SyncProgress
import org.pragmachess.mobile.link.SyncResult
import java.io.File
import java.time.LocalDate
import java.time.format.DateTimeFormatter
import java.util.UUID

/** The screens above the board, which is always at the bottom of the stack. */
sealed interface Screen {
    data class Games(val ref: DatabaseRef) : Screen
    data object Computers : Screen
    data object Settings : Screen
    data object Licenses : Screen
}

/** Where a sync with one computer is. */
sealed interface SyncState {
    data object Idle : SyncState
    data class Running(val progress: SyncProgress) : SyncState
    data class Failed(val failure: SyncFailure, val detail: String?) : SyncState
    data class Done(val result: SyncResult) : SyncState
}

/** A database with the number of games in it, for the side menu. */
data class DatabaseEntry(val ref: DatabaseRef, val games: Int, val openingBook: Boolean = false)

/** Today in PGN form, 2026.09.29. */
private fun today(): String = LocalDate.now().format(DateTimeFormatter.ofPattern("yyyy.MM.dd"))

class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val app = application
    private val identity = PhoneIdentity(application)
    private val store = AppStore(application)
    private val library = Library(File(application.filesDir, "databases"))
    private val sync = ComputerSync(application, identity, store, library)
    private val engine = UciEngine(application)

    // Side menu
    var localDatabases by mutableStateOf(emptyList<DatabaseEntry>())
        private set
    var computers by mutableStateOf(emptyList<Computer>())
        private set
    var computerDatabases by mutableStateOf(emptyMap<String, List<DatabaseEntry>>())
        private set
    val syncStates = mutableStateMapOf<String, SyncState>()

    // Navigation
    var screens by mutableStateOf(emptyList<Screen>())
        private set

    // The game on the board
    var line by mutableStateOf(GameLine(Position.starting()))
        private set
    var ply by mutableStateOf(0)
        private set
    var headers by mutableStateOf(GameHeaders(date = today()))
        private set
    /** The database the game is in (saved) or will be saved to. */
    var gameDatabase by mutableStateOf<DatabaseRef?>(null)
        private set
    /** Unsaved moves: the game on the board is not the one in [gameDatabase]. */
    var dirty by mutableStateOf(false)
        private set
    /** The game on the board is stored as it is (opened from a database or just saved). */
    var stored by mutableStateOf(false)
        private set
    var flipped by mutableStateOf(false)

    /** Something that would drop an unsaved game, waiting for the user to confirm. */
    var pendingDiscard by mutableStateOf<(() -> Unit)?>(null)

    // Engine
    var engineOn by mutableStateOf(false)
        private set
    var analysis by mutableStateOf<Analysis?>(null)
        private set
    val engineAvailable: Boolean get() = BuildConfig.HAS_STOCKFISH && engine.isAvailable

    /** A short message for the snackbar, consumed by the UI. */
    var message by mutableStateOf<String?>(null)

    /** A computer that refused the phone: the UI offers to scan again. */
    var refusedBy by mutableStateOf<Computer?>(null)

    val phoneName: String get() = identity.name
    val phoneKey: String get() = identity.keys.publicKeyHex

    val position: Position get() = line.positionAt(ply)

    init {
        viewModelScope.launch {
            withContext(Dispatchers.IO) { library.ensureDefault(app.getString(R.string.default_database)) }
            refresh()
            gameDatabase = localDatabases.firstOrNull { !it.openingBook }?.ref
            syncAll(quiet = true)
        }
        viewModelScope.launch {
            engine.analysis.collect { analysis = it }
        }
    }

    override fun onCleared() {
        engine.close()
        store.close()
    }

    suspend fun refresh() {
        val (local, paired, remote) = withContext(Dispatchers.IO) {
            val local = library.list(DatabaseLocation.Local).map(::entry)
            val paired = store.computers()
            val remote = paired.associate { c ->
                c.pubkey to library.list(DatabaseLocation.Computer(c.pubkey)).map(::entry)
            }
            Triple(local, paired, remote)
        }
        localDatabases = local
        computers = paired
        computerDatabases = remote
    }

    private fun entry(ref: DatabaseRef): DatabaseEntry =
        runCatching { PdbDatabase.open(library.file(ref)).use { DatabaseEntry(ref, it.gameCount(), it.isOpeningBook()) } }
            .getOrDefault(DatabaseEntry(ref, 0))

    // Navigation

    fun open(screen: Screen) {
        screens = screens + screen
    }

    fun back(): Boolean {
        if (screens.isEmpty()) return false
        screens = screens.dropLast(1)
        return true
    }

    private fun showBoard() {
        screens = emptyList()
    }

    // Databases and games

    suspend fun games(ref: DatabaseRef, filter: String): List<GameSummary> = withContext(Dispatchers.IO) {
        runCatching { PdbDatabase.open(library.file(ref)).use { it.games(filter) } }.getOrElse {
            withContext(Dispatchers.Main) { message = it.message }
            emptyList()
        }
    }

    /** Runs [action] at once, or after a confirmation when it would lose moves not saved yet. */
    fun unlessUnsaved(action: () -> Unit) {
        if (dirty && line.plyCount > 0) pendingDiscard = action else action()
    }

    val canSave: Boolean get() = !stored || dirty

    fun openGame(ref: DatabaseRef, id: Long) = unlessUnsaved { loadGame(ref, id) }

    private fun loadGame(ref: DatabaseRef, id: Long) {
        viewModelScope.launch {
            val record = withContext(Dispatchers.IO) {
                runCatching { PdbDatabase.open(library.file(ref)).use { it.game(id) } }.getOrNull()
            } ?: return@launch
            line = withContext(Dispatchers.Default) { GameLine.replay(record.startFen, record.movesUci, record.movesSan) }
            headers = record.headers
            gameDatabase = ref
            dirty = false
            stored = true
            ply = 0
            showBoard()
            positionChanged()
        }
    }

    fun newGame(ref: DatabaseRef? = gameDatabase) = unlessUnsaved { startGame(ref) }

    private fun startGame(ref: DatabaseRef?) {
        line = GameLine(Position.starting())
        headers = GameHeaders(date = today())
        gameDatabase = ref ?: localDatabases.firstOrNull { !it.openingBook }?.ref
        dirty = false
        stored = false
        ply = 0
        showBoard()
        positionChanged()
    }

    fun createDatabase(title: String, then: (DatabaseRef) -> Unit = {}) {
        viewModelScope.launch {
            val ref = withContext(Dispatchers.IO) { runCatching { library.createLocal(title) }.getOrNull() }
            if (ref == null) {
                message = app.getString(R.string.database_name_taken)
                return@launch
            }
            refresh()
            then(ref)
        }
    }

    fun deleteDatabase(ref: DatabaseRef) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) { library.delete(ref) }
            if (gameDatabase == ref) gameDatabase = null
            if (localDatabases.none { it.ref != ref }) {
                withContext(Dispatchers.IO) { library.ensureDefault(app.getString(R.string.default_database)) }
            }
            refresh()
            if (gameDatabase == null) gameDatabase = localDatabases.firstOrNull { !it.openingBook }?.ref
            screens = screens.filterNot { it is Screen.Games && it.ref == ref }
        }
    }

    /** Every database a game can be saved to: the local ones and the computers' copies (opening books excluded). */
    val writableDatabases: List<DatabaseRef>
        get() = (localDatabases + computers.flatMap { c -> computerDatabases[c.pubkey].orEmpty() })
            .filterNot { it.openingBook }.map { it.ref }

    fun databaseLabel(ref: DatabaseRef): String = when (val location = ref.location) {
        DatabaseLocation.Local -> ref.title
        is DatabaseLocation.Computer -> {
            val computer = computers.firstOrNull { it.pubkey == location.pubkey }?.name.orEmpty()
            "${ref.title} · $computer"
        }
    }

    /**
     * Saves the game on the board as a new game of [ref], made on this phone
     * (with a uuid), and puts it in the outbox of the computers it goes to.
     */
    fun saveGame(newHeaders: GameHeaders, ref: DatabaseRef) {
        headers = newHeaders
        val record = GameRecord(
            newHeaders,
            startFen = if (line.start == Position.starting()) "" else line.start.fen(),
            movesSan = line.sanText,
            movesUci = line.uciText,
            uuid = UUID.randomUUID().toString(),
        )
        viewModelScope.launch {
            val saved = withContext(Dispatchers.IO) {
                runCatching {
                    PdbDatabase.open(library.file(ref), writable = true).use { it.insert(record, identity.source) }
                    val targets = when (val location = ref.location) {
                        DatabaseLocation.Local -> store.computers().map { it.pubkey }
                        is DatabaseLocation.Computer -> listOf(location.pubkey)
                    }
                    targets.forEach { store.enqueue(it, ref.name, record) }
                    targets
                }
            }
            saved.onSuccess { targets ->
                gameDatabase = ref
                dirty = false
                stored = true
                message = app.getString(R.string.game_saved, ref.title)
                refresh()
                targets.mapNotNull { pk -> computers.firstOrNull { it.pubkey == pk } }.forEach { sync(it, quiet = true) }
            }.onFailure { message = it.message }
        }
    }

    // The board

    fun goTo(target: Int) {
        val clamped = target.coerceIn(0, line.plyCount)
        if (clamped == ply) return
        ply = clamped
        positionChanged()
    }

    fun play(move: Move) {
        val next = line.moves.getOrNull(ply)
        if (next?.move == move) {
            goTo(ply + 1)
            return
        }
        line = line.play(ply, move)
        ply += 1
        if (!dirty) {
            // A new line: what is on the board is no longer the stored game.
            dirty = true
            headers = headers.copy(result = "*")
        }
        val position = line.last
        if (ply == line.plyCount && (position.isCheckmate || position.isStalemate)) {
            val result = when {
                position.isStalemate -> "1/2-1/2"
                position.sideToMove == org.pragmachess.mobile.chess.Side.White -> "0-1"
                else -> "1-0"
            }
            headers = headers.copy(result = result)
        }
        positionChanged()
    }

    fun toggleEngine() {
        if (!engineAvailable) {
            message = app.getString(R.string.engine_unavailable)
            return
        }
        engineOn = !engineOn
        positionChanged()
    }

    private var foreground = true

    /** The engine only thinks while the app is on screen. */
    fun setForeground(visible: Boolean) {
        foreground = visible
        positionChanged()
    }

    private fun positionChanged() {
        if (!engineOn || !foreground) {
            engine.stop()
            return
        }
        val position = position
        if (position.legalMoves().isEmpty()) {
            engine.stop()
            return
        }
        if (!engine.analyse(position.fen())) {
            engineOn = false
            message = app.getString(R.string.engine_unavailable)
        }
    }

    // Computers

    fun pair(text: String) {
        val link = PairingLink.parse(text)
        if (link == null) {
            message = app.getString(R.string.pair_invalid_link)
            return
        }
        refusedBy = null
        viewModelScope.launch {
            val computer = Computer(link.pubkey, link.name, link.relays, link.secret, 0)
            withContext(Dispatchers.IO) {
                store.saveComputer(computer)
                // The games already on the phone go to the new computer too.
                for (ref in library.list(DatabaseLocation.Local)) {
                    runCatching {
                        PdbDatabase.open(library.file(ref)).use { db ->
                            db.phoneGames(identity.source).forEach { store.enqueue(computer.pubkey, ref.name, it) }
                        }
                    }
                }
            }
            refresh()
            if (screens.lastOrNull() != Screen.Computers) open(Screen.Computers)
            sync(computer, quiet = false)
        }
    }

    fun forget(computer: Computer) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                store.removeComputer(computer.pubkey)
                library.deleteComputer(computer.pubkey)
            }
            syncStates.remove(computer.pubkey)
            if ((gameDatabase?.location as? DatabaseLocation.Computer)?.pubkey == computer.pubkey) {
                gameDatabase = localDatabases.firstOrNull { !it.openingBook }?.ref
            }
            refresh()
        }
    }

    fun syncAll(quiet: Boolean = false) {
        computers.forEach { sync(it, quiet) }
    }

    fun outboxCount(computer: Computer): Int = store.outboxCount(computer.pubkey)

    fun sync(computer: Computer, quiet: Boolean = false) {
        if (syncStates[computer.pubkey] is SyncState.Running) return
        syncStates[computer.pubkey] = SyncState.Running(SyncProgress(SyncProgress.Step.Relays))
        viewModelScope.launch {
            val current = store.computers().firstOrNull { it.pubkey == computer.pubkey } ?: computer
            try {
                val result = withContext(Dispatchers.IO) {
                    sync.run(current) { progress ->
                        viewModelScope.launch(Dispatchers.Main) {
                            if (syncStates[computer.pubkey] is SyncState.Running) syncStates[computer.pubkey] = SyncState.Running(progress)
                        }
                    }
                }
                syncStates[computer.pubkey] = SyncState.Done(result)
                if (!quiet && result.received.isEmpty() && result.pushed == 0) {
                    message = app.getString(R.string.sync_up_to_date, result.computerName)
                }
            } catch (e: SyncException) {
                syncStates[computer.pubkey] = SyncState.Failed(e.failure, e.message)
                if (e.failure == SyncFailure.Refused) refusedBy = current
                if (!quiet) message = failureText(e.failure, current.name)
            } catch (e: Exception) {
                syncStates[computer.pubkey] = SyncState.Failed(SyncFailure.Protocol, e.message)
                if (!quiet) message = e.message
            }
            refresh()
        }
    }

    fun failureText(failure: SyncFailure, computer: String): String = when (failure) {
        SyncFailure.NoRelays -> app.getString(R.string.sync_no_relays)
        SyncFailure.NoAnswer -> app.getString(R.string.sync_no_answer)
        SyncFailure.Refused -> app.getString(R.string.sync_refused, computer)
        SyncFailure.NoConnection -> app.getString(R.string.sync_no_connection)
        SyncFailure.Protocol -> app.getString(R.string.sync_failed, computer)
    }

    fun setPhoneName(name: String) {
        identity.name = name
    }
}
