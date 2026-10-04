package org.pragmachess.mobile.link

import android.content.Context
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONArray
import org.json.JSONObject
import org.pragmachess.mobile.crypto.Hex
import org.pragmachess.mobile.crypto.Keys
import org.pragmachess.mobile.data.AppStore
import org.pragmachess.mobile.data.Computer
import org.pragmachess.mobile.data.Corpus
import org.pragmachess.mobile.data.GameRecord
import org.pragmachess.mobile.data.Library
import org.pragmachess.mobile.data.NewerSchemaException
import org.pragmachess.mobile.data.PdbDatabase
import org.pragmachess.mobile.data.Reconciler
import java.io.File
import java.security.MessageDigest

/** Why a sync stopped; the app turns it into a sentence. */
/** UpdateApp: a database of the computer has a schema this app does not know yet. */
enum class SyncFailure { NoRelays, NoAnswer, Refused, NoConnection, Protocol, UpdateApp }

class SyncException(val failure: SyncFailure, detail: String? = null) : Exception(detail ?: failure.name)

/** Where a sync is, for the progress shown in the app. */
data class SyncProgress(val step: Step, val file: String? = null, val fraction: Float? = null) {
    enum class Step { Relays, Offer, Connecting, Sending, Listing, Receiving }
}

/**
 * What a sync did: games [stored] (new here or there), [updated] (a newer
 * version replaced an older one), [conflicts] (uids that differed on the two
 * sides; the newer was kept), and the databases one side had not had before.
 */
data class SyncResult(
    val computerName: String,
    val stored: Int,
    val updated: Int,
    val conflicts: List<String>,
    val newDatabases: Int,
) {
    val changed: Boolean get() = stored > 0 || updated > 0 || conflicts.isNotEmpty() || newDatabases > 0
}

/**
 * One sync with a paired computer (docs/phone-link.md, "One corpus"): Nostr
 * signaling, the WebRTC data channel, then the two copies of every database
 * reconciled by lineage — the computer's changed files pulled and merged, the
 * games it lacks put, and the files the put changed pulled again. Whatever
 * the phone learnt from another computer goes to this one too.
 */
class ComputerSync(
    private val context: Context,
    private val identity: PhoneIdentity,
    private val store: AppStore,
    private val library: Library,
) {
    private val corpus = Corpus(library, store)

    /** Totals of one run. */
    private class Tally {
        var stored = 0
        var updated = 0
        val conflicts = LinkedHashSet<String>()
        var newDatabases = 0
    }

    suspend fun run(computer: Computer, progress: (SyncProgress) -> Unit): SyncResult {
        progress(SyncProgress(SyncProgress.Step.Relays))
        Signaling(identity.keys, computer.pubkey, computer.relays).use { signaling ->
            if (signaling.open() == 0) throw SyncException(SyncFailure.NoRelays)
            PeerLink(context).use { peer ->
                progress(SyncProgress(SyncProgress.Step.Offer))
                val session = Hex.encode(Keys.random(16))
                val offer = JSONObject().put("t", "offer").put("session", session)
                    .put("sdp", peer.createOffer()).put("name", identity.name)
                computer.pairSecret?.let { offer.put("pair", it) }
                signaling.send(offer)
                val answer = signaling.receive(session, ANSWER_TIMEOUT_MS) ?: throw SyncException(SyncFailure.NoAnswer)
                if (answer.optString("t") == "refused") throw SyncException(SyncFailure.Refused, answer.optString("reason"))
                if (answer.optString("t") != "answer") throw SyncException(SyncFailure.Protocol, "unexpected ${answer.optString("t")}")
                progress(SyncProgress(SyncProgress.Step.Connecting))
                peer.acceptAnswer(answer.getString("sdp"))
                if (!peer.awaitOpen(CONNECT_TIMEOUT_MS)) throw SyncException(SyncFailure.NoConnection)

                val name = answer.optString("name").ifBlank { computer.name }
                // Accepted: the pairing secret has done its job.
                store.saveComputer(computer.copy(name = name, pairSecret = null))
                val tally = Tally()
                val listed = list(peer, progress)
                adoptLineages(listed, name)
                notifyDeleted(peer, computer.pubkey, listed)
                val toSend = pull(peer, computer.pubkey, listed, null, name, tally, progress)
                val changed = push(peer, toSend, tally, progress)
                val relisted = if (changed.isNotEmpty()) list(peer, progress) else listed
                if (changed.isNotEmpty()) pull(peer, computer.pubkey, relisted, changed, name, tally, progress)
                // Both copies as they are now: next time, unchanged pairs are not downloaded again.
                val mine = corpus.byLineage()
                for (r in relisted) {
                    val ref = mine[corpus.resolve(r.lineage)] ?: continue
                    store.setReconciled(computer.pubkey, r.lineage, r.sha256, sha256(library.file(ref)))
                }
                // Marks the computer as synced with lineages at least once, even with nothing in common.
                store.setReconciled(computer.pubkey, "*", "", "")
                confirmLineages(name, computer.pubkey)
                corpus.dedupe()
                store.saveComputer(computer.copy(name = name, pairSecret = null, lastSync = System.currentTimeMillis()))
                return SyncResult(name, tally.stored, tally.updated, tally.conflicts.toList(), tally.newDatabases)
            }
        }
    }

    private suspend fun nextText(peer: PeerLink, timeoutMs: Long = FRAME_TIMEOUT_MS): JSONObject {
        while (true) {
            val frame = withTimeoutOrNull(timeoutMs) { peer.frames.receive() }
                ?: throw SyncException(SyncFailure.Protocol, "the computer stopped answering")
            when (frame) {
                is Frame.Text -> return JSONObject(frame.text)
                is Frame.Binary -> continue
                Frame.Closed -> throw SyncException(SyncFailure.Protocol, "the connection was closed")
            }
        }
    }

    private fun send(peer: PeerLink, message: JSONObject) {
        if (!peer.send(message.toString())) throw SyncException(SyncFailure.Protocol, "could not send")
    }

    /** A database of the computer's list. */
    private data class Remote(val name: String, val lineage: String, val sha256: String)

    private suspend fun list(peer: PeerLink, progress: (SyncProgress) -> Unit): List<Remote> {
        progress(SyncProgress(SyncProgress.Step.Listing))
        send(peer, JSONObject().put("op", "list"))
        val list = nextText(peer)
        if (list.optString("op") != "list") throw SyncException(SyncFailure.Protocol, list.optString("message"))
        val files = list.getJSONArray("files")
        return (0 until files.length()).map { files.getJSONObject(it) }
            .map { Remote(it.getString("name"), it.optString("id"), it.optString("sha256")) }
            // A computer without lineages (older than version 5) cannot be reconciled with.
            .filter { isSafeName(it.name) && it.lineage.isNotBlank() }
    }

    /**
     * Files that were on the phone before databases had ids got a provisional
     * one: a database of the same name on the computer is the same database,
     * so the phone's takes the computer's id.
     */
    private fun adoptLineages(remote: List<Remote>, computerName: String) {
        val entries = corpus.entries()
        val lineages = entries.map { it.lineage }.toSet()
        for (r in remote) {
            if (r.lineage in lineages || corpus.resolve(r.lineage) in lineages) continue
            // Only from the computer the file came from, or any computer for the phone's own files.
            val mine = entries.firstOrNull {
                it.ref.name == r.name && store.isProvisional(it.lineage) && (it.origin == computerName || it.origin == identity.name)
            } ?: continue
            PdbDatabase.open(library.file(mine.ref), writable = true).use { it.setProperty(PdbDatabase.PROPERTY_ID, r.lineage) }
            store.adopt(mine.lineage, r.lineage, mine.origin.ifBlank { computerName })
        }
    }

    /**
     * A provisional id becomes final once it can no longer meet the
     * computer's id for the same file: after a sync with the computer the file
     * came from, or, for the phone's own files, once every paired computer
     * has been synced with.
     */
    private fun confirmLineages(computerName: String, pubkey: String) {
        val synced = store.computers().all { it.pubkey == pubkey || store.reconciled(it.pubkey).isNotEmpty() }
        for (entry in corpus.entries()) {
            if (!store.isProvisional(entry.lineage)) continue
            if (entry.origin == computerName || (entry.origin == identity.name && synced)) store.confirm(entry.lineage)
        }
    }

    /** Whether [lineage] of a computer's list is a database the user deleted on the phone. */
    private fun isDeleted(lineage: String, deleted: Set<String>): Boolean = lineage in deleted || corpus.resolve(lineage) in deleted

    /**
     * Tells the computer, once, which of its databases the user deleted on the
     * phone (docs/phone-link.md, "A database deleted on the phone"): the
     * computer asks its own user whether to delete it everywhere or keep it.
     * A computer that does not know the message answers with an error, and is
     * told again at the next sync.
     */
    private suspend fun notifyDeleted(peer: PeerLink, pubkey: String, remote: List<Remote>) {
        val deleted = store.deleted()
        if (deleted.isEmpty()) return
        val notified = store.notified(pubkey)
        for (r in remote) {
            if (r.lineage in notified || !isDeleted(r.lineage, deleted.keys)) continue
            val what = deleted[r.lineage] ?: deleted.getValue(corpus.resolve(r.lineage))
            send(peer, JSONObject().put("op", "deleted").put("db", r.lineage).put("name", what.name).put("when", what.deletedAt))
            val reply = nextText(peer)
            if (reply.optString("op") == "deleted") store.setNotified(pubkey, r.lineage)
        }
    }

    /**
     * Gets the computer's databases that differ from the phone's copy (all of
     * them, or those of [only]) and merges them in by lineage. Returns, by
     * lineage, the phone's games the computer lacks or has in an older version.
     */
    private suspend fun pull(
        peer: PeerLink,
        pubkey: String,
        remote: List<Remote>,
        only: Set<String>?,
        computerName: String,
        tally: Tally,
        progress: (SyncProgress) -> Unit,
    ): Map<String, List<GameRecord>> {
        val toSend = LinkedHashMap<String, List<GameRecord>>()
        val mine = corpus.byLineage()
        val last = store.reconciled(pubkey)
        val deleted = store.deleted().keys
        for (r in remote) {
            if (only != null && r.lineage !in only) continue
            // Deleted on the phone: whatever the computer decides, it does not come back here.
            if (isDeleted(r.lineage, deleted)) continue
            // A lineage merged into another here (Corpus.dedupe) is merged into that one.
            val key = corpus.resolve(r.lineage)
            val aliased = key != r.lineage
            val local = mine[key]
            if (local == null && aliased) continue
            if (local != null) {
                // The same file, or neither copy changed since they were last reconciled.
                val localSha = sha256(library.file(local))
                if (localSha == r.sha256 || last[r.lineage] == (r.sha256 to localSha)) continue
            }
            progress(SyncProgress(SyncProgress.Step.Receiving, r.name, 0f))
            val temp = File(library.dir, ".incoming-${r.lineage}.part")
            receive(peer, r.name, temp) { fraction -> progress(SyncProgress(SyncProgress.Step.Receiving, r.name, fraction)) }
            try {
                if (local == null) {
                    // A database the phone did not have: it is the phone's copy now.
                    val count = PdbDatabase.open(temp, writable = true).use { it.gameCount() }
                    val target = File(library.dir, library.freeName(r.name.substringAfterLast('/').removeSuffix(".pdb"), computerName))
                    if (!temp.renameTo(target)) throw SyncException(SyncFailure.Protocol, "could not save ${r.name}")
                    store.setOrigin(r.lineage, computerName)
                    tally.newDatabases++
                    tally.stored += count
                } else {
                    val (incoming, states) = PdbDatabase.open(temp, writable = true).use { it.allGames() to it.states() }
                    PdbDatabase.open(library.file(local), writable = true).use { db ->
                        // Where the games are first: what the computer trashed, deleted or purged.
                        db.mergeStates(states)
                        val plan = Reconciler.plan(db.allGames(), incoming)
                        tally.stored += db.merge(plan)
                        tally.updated += plan.update.size
                        tally.conflicts += plan.conflicts
                        if (plan.send.isNotEmpty() && !aliased) toSend[key] = plan.send
                    }
                }
            } catch (e: NewerSchemaException) {
                // Made by a newer Pragma Chess: the app is asked to be updated, the file is not kept.
                throw SyncException(SyncFailure.UpdateApp, r.name)
            } finally {
                temp.delete()
                File(temp.path + "-journal").delete()
            }
        }
        if (only == null) {
            // The phone's databases the computer does not have at all: all their games.
            val listed = remote.flatMap { listOf(it.lineage, corpus.resolve(it.lineage)) }.toSet()
            for ((lineage, ref) in corpus.byLineage()) {
                // A provisional id is not sent anywhere: the file may still take a computer's id.
                if (lineage !in listed && !store.isProvisional(lineage)) {
                    toSend[lineage] = PdbDatabase.open(library.file(ref)).use { it.allGames() }
                }
            }
        }
        return toSend
    }

    /** Puts the games the computer lacks, addressed by lineage. Returns the lineages it changed. */
    private suspend fun push(
        peer: PeerLink,
        toSend: Map<String, List<GameRecord>>,
        tally: Tally,
        progress: (SyncProgress) -> Unit,
    ): Set<String> {
        val mine = corpus.byLineage()
        val changed = LinkedHashSet<String>()
        val total = toSend.values.sumOf { it.size }.coerceAtLeast(1)
        var done = 0
        for ((lineage, games) in toSend) {
            val ref = mine[lineage] ?: continue
            val properties = PdbDatabase.open(library.file(ref)).use { db ->
                (db.properties() - PdbDatabase.PROPERTY_ID) + (PdbDatabase.PROPERTY_TYPE to
                    if (db.isOpeningBook()) PdbDatabase.TYPE_OPENING_BOOK else PdbDatabase.TYPE_GAMES)
            }
            // An empty database is created on the computer too: one put with no games.
            for (batch in games.chunked(PUT_BATCH).ifEmpty { listOf(emptyList()) }) {
                progress(SyncProgress(SyncProgress.Step.Sending, ref.name, done.toFloat() / total))
                send(peer, JSONObject().put("op", "put").put("db", lineage).put("name", ref.name)
                    .put("properties", JSONObject(properties))
                    .put("games", JSONArray(batch.map { it.toJson() })))
                val reply = nextText(peer)
                if (reply.optString("op") != "put") throw SyncException(SyncFailure.Protocol, reply.optString("message"))
                val stored = reply.optInt("stored")
                val updated = reply.optInt("updated")
                tally.stored += stored
                tally.updated += updated
                reply.optJSONArray("conflicts")?.let { c -> for (i in 0 until c.length()) c.optJSONObject(i)?.optString("uid")?.let(tally.conflicts::add) }
                if (stored > 0 || updated > 0 || batch.isEmpty()) changed += lineage
                done += batch.size
            }
        }
        return changed
    }

    private suspend fun receive(peer: PeerLink, name: String, temp: File, fraction: (Float) -> Unit) {
        send(peer, JSONObject().put("op", "get").put("name", name))
        val header = nextText(peer)
        if (header.optString("op") != "file") throw SyncException(SyncFailure.Protocol, header.optString("message", "no file"))
        val size = header.getLong("size")
        val expected = header.getString("sha256")
        temp.parentFile?.mkdirs()
        val digest = MessageDigest.getInstance("SHA-256")
        var written = 0L
        try {
            temp.outputStream().buffered().use { out ->
                while (true) {
                    val frame = withTimeoutOrNull(FRAME_TIMEOUT_MS) { peer.frames.receive() }
                        ?: throw SyncException(SyncFailure.Protocol, "the transfer of $name stalled")
                    when (frame) {
                        is Frame.Binary -> {
                            out.write(frame.data)
                            digest.update(frame.data)
                            written += frame.data.size
                            if (size > 0) fraction(written.toFloat() / size)
                        }
                        is Frame.Text -> {
                            val message = JSONObject(frame.text)
                            if (message.optString("op") == "done") break
                            if (message.optString("op") == "error") throw SyncException(SyncFailure.Protocol, message.optString("message"))
                        }
                        Frame.Closed -> throw SyncException(SyncFailure.Protocol, "the connection was closed")
                    }
                }
            }
            if (written != size || Hex.encode(digest.digest()) != expected)
                throw SyncException(SyncFailure.Protocol, "$name arrived damaged")
        } catch (e: Exception) {
            temp.delete()
            throw e
        }
    }

    companion object {
        const val ANSWER_TIMEOUT_MS = 25_000L
        const val CONNECT_TIMEOUT_MS = 30_000L
        const val FRAME_TIMEOUT_MS = 45_000L
        const val PUT_BATCH = 50

        fun isSafeName(name: String): Boolean =
            name.endsWith(".pdb") && !name.startsWith("/") && name.split('/').none { it.isEmpty() || it == "." || it == ".." }

        fun sha256(file: File): String {
            val digest = MessageDigest.getInstance("SHA-256")
            file.inputStream().use { input ->
                val buffer = ByteArray(1 shl 16)
                while (true) {
                    val read = input.read(buffer)
                    if (read < 0) break
                    digest.update(buffer, 0, read)
                }
            }
            return Hex.encode(digest.digest())
        }
    }
}
