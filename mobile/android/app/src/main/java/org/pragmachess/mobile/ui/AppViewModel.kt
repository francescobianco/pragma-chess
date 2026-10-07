package org.pragmachess.mobile.ui

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.sample
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.GameLine
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.data.AppStore
import org.pragmachess.mobile.data.Computer
import org.pragmachess.mobile.data.Corpus
import org.pragmachess.mobile.data.CorpusEntry
import org.pragmachess.mobile.data.Dedupe
import org.pragmachess.mobile.data.GameIdentity
import org.pragmachess.mobile.data.DatabaseRef
import org.pragmachess.mobile.data.GameHeaders
import org.pragmachess.mobile.data.GameRecord
import org.pragmachess.mobile.data.GameSummary
import org.pragmachess.mobile.data.Library
import org.pragmachess.mobile.data.NewerSchemaException
import org.pragmachess.mobile.data.PdbDatabase
import org.pragmachess.mobile.engine.Analysis
import org.pragmachess.mobile.engine.OexEngine
import org.pragmachess.mobile.engine.OexEngines
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
import org.pragmachess.mobile.smart.SmartPrograms

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


/** Today in PGN form, 2026.09.29. */
private fun today(): String = LocalDate.now().format(DateTimeFormatter.ofPattern("yyyy.MM.dd"))

class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val app = application
    private val identity = PhoneIdentity(application)
    private val store = AppStore(application)
    private val library = Library(File(application.filesDir, "databases"))
    private val sync = ComputerSync(application, identity, store, library)
    private val corpus = Corpus(library, store)
    private val engine = UciEngine()
    private val settings = application.getSharedPreferences("settings", android.content.Context.MODE_PRIVATE)

    // Side menu
    /** Every database of the phone, one list sorted by name. */
    var databases by mutableStateOf(emptyList<CorpusEntry>())
        private set
    var computers by mutableStateOf(emptyList<Computer>())
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
    /** Engines installed as separate apps (Open Exchange), found again when the app comes back. */
    var engines by mutableStateOf(emptyList<OexEngine>())
        private set
    var engineId by mutableStateOf(settings.getString("engine", null))
        private set
    val selectedEngine: OexEngine? get() = engines.firstOrNull { it.id == engineId } ?: engines.firstOrNull()
    private var engineBinary by mutableStateOf<File?>(null)
    /** An engine is installed and can analyse; otherwise the analysis area offers to install one. */
    val engineReady: Boolean get() = engineBinary != null

    /** Explain: arrows on the board justifying the evaluation of the move on it. */
    val explainer = ExplainController(ExplainStrings(application))

    /** A short message for the snackbar, consumed by the UI. */
    var message by mutableStateOf<String?>(null)

    /** A computer that refused the phone: the UI offers to scan again. */
    var refusedBy by mutableStateOf<Computer?>(null)

    val phoneName: String get() = identity.name
    val phoneKey: String get() = identity.keys.publicKeyHex

    val position: Position get() = line.positionAt(ply)

    init {
        // Explain and the tutor are the SMART programs of the repository, carried as assets.
        SmartPrograms.reader = { name ->
            runCatching { app.assets.open("smart/$name").bufferedReader().use { it.readText() } }.getOrNull()
        }
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                corpus.prepare(identity.name, store.computers().associate { it.pubkey to it.name })
                // Repairs phones where a sync of version 0.1 left one database twice.
                corpus.dedupe()
                ensureDefault()
            }
            refresh()
            gameDatabase = defaultDatabase()
            syncAll(quiet = true)
        }
        viewModelScope.launch {
            // An engine prints many lines a second; a few are enough to follow it,
            // and the board stays free for the finger.
            engine.analysis.sample(200).collect {
                analysis = it
                explainer.liveAnalysis(it)
            }
        }
        refreshEngines()
    }

    override fun onCleared() {
        explainer.close()
        engine.close()
        store.close()
    }

    suspend fun refresh() {
        val (entries, paired) = withContext(Dispatchers.IO) { corpus.entries() to store.computers() }
        databases = entries.sortedBy { it.ref.title.lowercase() }
        computers = paired
    }

    /** Where a new game goes unless one is chosen: "My Games" if it is there, else the first game collection. */
    private fun defaultDatabase(): DatabaseRef? {
        val collections = databases.filterNot { it.openingBook }
        val mine = app.getString(R.string.default_database)
        return (collections.firstOrNull { it.ref.title == mine } ?: collections.firstOrNull())?.ref
    }

    /** A phone on its own starts with one database, "My Games", with a lineage of its own. */
    private fun ensureDefault() {
        if (library.list().isNotEmpty()) return
        library.create(app.getString(R.string.default_database))?.let { (_, lineage) -> store.setOrigin(lineage, identity.name) }
    }

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
        runCatching { PdbDatabase.open(library.file(ref)).use { it.games(filter) } }.getOrElse { error ->
            // A database from a newer Pragma Chess is fine: it is the app that has to be updated.
            val text = if (error is NewerSchemaException) R.string.database_needs_update else R.string.database_unreadable
            withContext(Dispatchers.Main) { message = app.getString(text, ref.title) }
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
        gameDatabase = ref ?: defaultDatabase()
        dirty = false
        stored = false
        ply = 0
        showBoard()
        positionChanged()
    }

    fun createDatabase(title: String, then: (DatabaseRef) -> Unit = {}) {
        viewModelScope.launch {
            val ref = withContext(Dispatchers.IO) {
                runCatching { library.create(title) }.getOrNull()?.let { (ref, lineage) ->
                    store.setOrigin(lineage, identity.name)
                    ref
                }
            }
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
            withContext(Dispatchers.IO) {
                // Remembered by lineage, so the computers that have it are told and it is not downloaded again.
                val lineage = runCatching { PdbDatabase.open(library.file(ref)).use { it.lineage() } }.getOrNull()
                if (lineage != null) store.markDeleted(lineage, ref.name, java.time.Instant.now().toString())
                library.delete(ref)
            }
            if (gameDatabase == ref) gameDatabase = null
            withContext(Dispatchers.IO) { ensureDefault() }
            refresh()
            if (gameDatabase == null) gameDatabase = defaultDatabase()
            screens = screens.filterNot { it is Screen.Games && it.ref == ref }
        }
    }

    /** Every database a game can be saved to (opening books excluded). */
    val writableDatabases: List<DatabaseRef>
        get() = databases.filterNot { it.openingBook }.map { it.ref }

    /** The name, and where it came from when another database has the same name. */
    fun databaseLabel(ref: DatabaseRef): String {
        val entry = databases.firstOrNull { it.ref == ref } ?: return ref.title
        return if (homonyms(entry)) "${displayTitle(entry)} · ${entry.origin}" else displayTitle(entry)
    }

    /**
     * The name shown: two databases with one name are two files, "Name" and
     * "Name (device)"; both show "Name", with the device underneath.
     */
    fun displayTitle(entry: CorpusEntry): String = Dedupe.displayTitle(entry.ref.title, entry.origin)

    /** Another database has the same name but a different lineage. */
    fun homonyms(entry: CorpusEntry): Boolean =
        databases.any { it.lineage != entry.lineage && displayTitle(it).equals(displayTitle(entry), ignoreCase = true) }

    /**
     * Saves the game on the board as a new game of [ref]; the next sync
     * reconciles it with every computer.
     */
    fun saveGame(newHeaders: GameHeaders, ref: DatabaseRef) {
        headers = newHeaders
        val record = GameRecord(
            newHeaders,
            startFen = if (line.start == Position.starting()) "" else line.start.fen(),
            movesSan = line.sanText,
            movesUci = line.uciText,
            modified = GameIdentity.now(),
        )
        viewModelScope.launch {
            val saved = withContext(Dispatchers.IO) {
                runCatching { PdbDatabase.open(library.file(ref), writable = true).use { it.insert(record) } }
            }
            saved.onSuccess {
                gameDatabase = ref
                dirty = false
                stored = true
                message = app.getString(R.string.game_saved, ref.title)
                refresh()
                syncAll(quiet = true)
            }.onFailure { message = app.getString(R.string.game_not_saved, ref.title) }
        }
    }

    // The board

    fun goTo(target: Int) {
        val clamped = target.coerceIn(0, line.plyCount)
        if (clamped == ply) return
        // An explanation belongs to one move: moving on turns it off until asked again.
        explainer.stop()
        ply = clamped
        positionChanged()
    }

    fun play(move: Move) {
        val next = line.moves.getOrNull(ply)
        if (next?.move == move) {
            goTo(ply + 1)
            return
        }
        explainer.stop()
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
        engineOn = !engineOn
        if (!engineOn) {
            analysis = null
            explainer.stop() // Explaining needs the engine.
        }
        positionChanged()
    }

    /**
     * Explain on or off for the move on the board. Explaining turns the engine
     * on; with none installed the analysis area offers one instead.
     */
    fun toggleExplain() {
        if (explainer.enabled) {
            explainer.stop()
            return
        }
        if (!engineOn) toggleEngine()
        if (engineBinary == null) return // The analysis area offers to install one.
        // A null move is no move to explain: the position is explained as a start.
        val played = line.moves.getOrNull(ply - 1)?.move?.takeUnless { it.isNull }
        explainer.start(if (played != null) line.positionAt(ply - 1) else null, played, position,
            app.getString(R.string.explain_analyzing))
    }

    fun chooseEngine(engine: OexEngine) {
        engineId = engine.id
        settings.edit().putString("engine", engine.id).apply()
        engineBinary = OexEngines.binary(app, engine)
        positionChanged()
    }

    /** Looks for installed engines, e.g. when the user comes back from installing one. */
    fun refreshEngines() {
        viewModelScope.launch {
            val (found, binary) = withContext(Dispatchers.IO) {
                val found = OexEngines.installed(app)
                val chosen = found.firstOrNull { it.id == engineId } ?: found.firstOrNull()
                found to chosen?.let { OexEngines.binary(app, it) }
            }
            engines = found
            if (binary != engineBinary) {
                engineBinary = binary
                if (binary == null) engine.close()
                positionChanged()
            }
        }
    }

    /** The Play Store page of a free engine app for this protocol. */
    fun installEngine(context: android.content.Context) {
        val id = OexEngines.SUGGESTED_PACKAGE
        val market = android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse("market://details?id=$id"))
        val web = android.content.Intent(android.content.Intent.ACTION_VIEW,
            android.net.Uri.parse("https://play.google.com/store/apps/details?id=$id"))
        try {
            context.startActivity(market)
        } catch (e: android.content.ActivityNotFoundException) {
            runCatching { context.startActivity(web) }
        }
    }

    private var foreground = true

    /** The engine only thinks while the app is on screen. */
    fun setForeground(visible: Boolean) {
        foreground = visible
        if (visible) refreshEngines() else explainer.stop()
        positionChanged()
    }

    private fun positionChanged() {
        if (!engineOn || !foreground) {
            // In the background the process goes: no memory held for nothing.
            if (foreground) engine.stop() else engine.close()
            return
        }
        val position = position
        if (position.legalMoves().isEmpty()) {
            engine.stop()
            return
        }
        val binary = engineBinary
        if (binary == null || !engine.analyse(binary, position.fen())) {
            // No engine (or its app was removed): end the process; the
            // analysis area says so and offers to install one.
            engine.close()
            analysis = null
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
            // Every database of the phone goes to the new computer at its first sync.
            withContext(Dispatchers.IO) { store.saveComputer(computer) }
            refresh()
            if (screens.lastOrNull() != Screen.Computers) open(Screen.Computers)
            sync(computer, quiet = false)
        }
    }

    fun forget(computer: Computer) {
        viewModelScope.launch {
            // The databases stay: they are part of the phone's corpus now.
            withContext(Dispatchers.IO) { store.removeComputer(computer.pubkey) }
            syncStates.remove(computer.pubkey)
            refresh()
        }
    }

    fun syncAll(quiet: Boolean = false) {
        computers.forEach { sync(it, quiet) }
    }

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
                if (result.changed) message = summary(result)
                else if (!quiet) message = app.getString(R.string.sync_up_to_date, result.computerName)
            } catch (e: SyncException) {
                syncStates[computer.pubkey] = SyncState.Failed(e.failure, e.message)
                if (e.failure == SyncFailure.Refused) refusedBy = current
                if (!quiet) message = failureText(e.failure, current.name)
            } catch (e: Exception) {
                syncStates[computer.pubkey] = SyncState.Failed(SyncFailure.Protocol, e.message)
                if (!quiet) message = failureText(SyncFailure.Protocol, current.name)
            }
            refresh()
        }
    }

    /** "Synced with Intel5: 3 games stored, 1 updated; 2 games differed on the two devices, the most recent version was kept." */
    fun summary(result: SyncResult): String {
        val parts = buildList {
            if (result.newDatabases > 0) add(app.resources.getQuantityString(R.plurals.sync_new_databases, result.newDatabases, result.newDatabases))
            if (result.stored > 0) add(app.resources.getQuantityString(R.plurals.sync_stored, result.stored, result.stored))
            if (result.updated > 0) add(app.resources.getQuantityString(R.plurals.sync_updated, result.updated, result.updated))
        }
        var text = app.getString(R.string.sync_summary, result.computerName, parts.joinToString(", ").ifEmpty { app.getString(R.string.sync_nothing_new) })
        if (result.conflicts.isNotEmpty()) {
            text += " " + app.resources.getQuantityString(R.plurals.sync_conflicts, result.conflicts.size, result.conflicts.size)
        }
        return text
    }

    fun failureText(failure: SyncFailure, computer: String): String = when (failure) {
        SyncFailure.NoRelays -> app.getString(R.string.sync_no_relays)
        SyncFailure.NoAnswer -> app.getString(R.string.sync_no_answer)
        SyncFailure.Refused -> app.getString(R.string.sync_refused, computer)
        SyncFailure.NoConnection -> app.getString(R.string.sync_no_connection)
        SyncFailure.Protocol -> app.getString(R.string.sync_failed, computer)
        SyncFailure.UpdateApp -> app.getString(R.string.sync_update_app, computer)
    }

    fun setPhoneName(name: String) {
        identity.name = name
    }
}
