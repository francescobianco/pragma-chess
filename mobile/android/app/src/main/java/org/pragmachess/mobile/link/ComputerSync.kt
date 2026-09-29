package org.pragmachess.mobile.link

import android.content.Context
import kotlinx.coroutines.withTimeoutOrNull
import org.json.JSONArray
import org.json.JSONObject
import org.pragmachess.mobile.crypto.Hex
import org.pragmachess.mobile.crypto.Keys
import org.pragmachess.mobile.data.AppStore
import org.pragmachess.mobile.data.Computer
import org.pragmachess.mobile.data.DatabaseRef
import org.pragmachess.mobile.data.GameRecord
import org.pragmachess.mobile.data.Library
import org.pragmachess.mobile.data.PdbDatabase
import java.io.File
import java.security.MessageDigest

/** Why a sync stopped; the app turns it into a sentence. */
enum class SyncFailure { NoRelays, NoAnswer, Refused, NoConnection, Protocol }

class SyncException(val failure: SyncFailure, detail: String? = null) : Exception(detail ?: failure.name)

/** Where a sync is, for the progress shown in the app. */
data class SyncProgress(val step: Step, val file: String? = null, val fraction: Float? = null) {
    enum class Step { Relays, Offer, Connecting, Sending, Listing, Receiving }
}

data class SyncResult(val computerName: String, val pushed: Int, val received: List<String>)

/**
 * One sync with a paired computer (docs/phone-link.md): Nostr signaling, the
 * WebRTC data channel, then the games of the outbox pushed ("put") and the
 * changed databases pulled ("list", "get").
 */
class ComputerSync(
    private val context: Context,
    private val identity: PhoneIdentity,
    private val store: AppStore,
    private val library: Library,
) {
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
                val pushed = push(peer, computer, progress)
                val received = pull(peer, computer, progress)
                store.saveComputer(computer.copy(name = name, pairSecret = null, lastSync = System.currentTimeMillis()))
                return SyncResult(name, pushed, received)
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

    /** Pushes the outbox, a database at a time; entries leave it once the computer counted them. */
    private suspend fun push(peer: PeerLink, computer: Computer, progress: (SyncProgress) -> Unit): Int {
        val entries = store.outbox(computer.pubkey)
        var done = 0
        for ((database, games) in entries.groupBy { it.database }) {
            for (batch in games.chunked(PUT_BATCH)) {
                progress(SyncProgress(SyncProgress.Step.Sending, database, done.toFloat() / entries.size))
                send(peer, JSONObject().put("op", "put").put("name", database)
                    .put("games", JSONArray(batch.map { it.game.toJson() })))
                val reply = nextText(peer)
                if (reply.optString("op") == "error") throw SyncException(SyncFailure.Protocol, reply.optString("message"))
                val counted = reply.optInt("stored") + reply.optInt("known")
                if (reply.optString("op") == "put" && counted >= batch.size) store.sent(computer.pubkey, batch.map { it.gameId })
                done += batch.size
            }
        }
        return done
    }

    private suspend fun pull(peer: PeerLink, computer: Computer, progress: (SyncProgress) -> Unit): List<String> {
        progress(SyncProgress(SyncProgress.Step.Listing))
        send(peer, JSONObject().put("op", "list"))
        val list = nextText(peer)
        if (list.optString("op") != "list") throw SyncException(SyncFailure.Protocol, list.optString("message"))
        val files = list.getJSONArray("files")
        val received = ArrayList<String>()
        for (i in 0 until files.length()) {
            val entry = files.getJSONObject(i)
            val name = entry.getString("name")
            if (!isSafeName(name)) continue
            val ref = library.refForRemote(computer.pubkey, name)
            val target = library.file(ref)
            if (target.exists() && sha256(target) == entry.optString("sha256")) continue
            progress(SyncProgress(SyncProgress.Step.Receiving, name, 0f))
            val temp = File(target.path + ".part")
            receive(peer, name, temp) { fraction -> progress(SyncProgress(SyncProgress.Step.Receiving, name, fraction)) }
            replace(ref, target, temp, computer)
            received.add(name)
        }
        return received
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

    /**
     * Puts the received file in place of the phone's copy, then stores again
     * the phone's games the new file lacks: those of the old copy and those
     * still in the outbox, so nothing typed on the phone is lost by a pull.
     */
    private fun replace(ref: DatabaseRef, target: File, temp: File, computer: Computer) {
        val source = identity.source
        val keep = LinkedHashMap<String, GameRecord>()
        if (target.exists()) {
            runCatching { PdbDatabase.open(target).use { db -> db.phoneGames(source).forEach { g -> g.uuid?.let { keep[it] = g } } } }
        }
        store.outbox(computer.pubkey).filter { it.database == ref.name }.forEach { keep[it.gameId] = it.game }
        File(target.path + "-journal").delete()
        if (!temp.renameTo(target)) {
            target.delete()
            if (!temp.renameTo(target)) throw SyncException(SyncFailure.Protocol, "could not save ${ref.name}")
        }
        if (keep.isNotEmpty()) {
            PdbDatabase.open(target, writable = true).use { db -> keep.values.forEach { db.insert(it, source) } }
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
