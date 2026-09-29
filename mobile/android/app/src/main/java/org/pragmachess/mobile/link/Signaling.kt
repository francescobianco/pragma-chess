package org.pragmachess.mobile.link

import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withTimeoutOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONArray
import org.json.JSONObject
import org.pragmachess.mobile.crypto.Hex
import org.pragmachess.mobile.crypto.KeyPair
import org.pragmachess.mobile.crypto.Nip44
import org.pragmachess.mobile.crypto.NostrEvent
import java.util.concurrent.TimeUnit
import kotlin.math.abs

/**
 * The Nostr side of a connection: subscribes to the phone's own key on the
 * computer's relays and exchanges the encrypted kind-25050 messages with it.
 * Only events signed by [peer] and decrypted with the shared key get through.
 */
class Signaling(private val keys: KeyPair, private val peer: String, private val relays: List<String>) : AutoCloseable {
    private val client = OkHttpClient.Builder().pingInterval(20, TimeUnit.SECONDS).build()
    private val conversationKey = Nip44.conversationKey(keys.secret, Hex.decode(peer))
    private val incoming = Channel<JSONObject>(Channel.UNLIMITED)
    private val opened = Channel<Unit>(Channel.UNLIMITED)
    private val sockets = mutableListOf<WebSocket>()
    private val seen = HashSet<String>()
    private val subscription = "pragma-" + Hex.encode(org.pragmachess.mobile.crypto.Keys.random(4))

    /** Connects to the relays and subscribes; returns how many relays answered within [timeoutMs]. */
    suspend fun open(timeoutMs: Long = 8000): Int {
        val since = System.currentTimeMillis() / 1000 - 60
        val request = JSONArray().put("REQ").put(subscription)
            .put(JSONObject().put("kinds", JSONArray().put(KIND)).put("#p", JSONArray().put(keys.publicKeyHex)).put("since", since))
            .toString()
        for (url in relays) {
            val socket = client.newWebSocket(Request.Builder().url(url).build(), object : WebSocketListener() {
                override fun onOpen(webSocket: WebSocket, response: Response) {
                    webSocket.send(request)
                    opened.trySend(Unit)
                }

                override fun onMessage(webSocket: WebSocket, text: String) = handle(text)
            })
            synchronized(sockets) { sockets.add(socket) }
        }
        var count = 0
        withTimeoutOrNull(timeoutMs) {
            while (count < relays.size) {
                opened.receive()
                count++
                // One relay is enough to go on; give the others a moment.
                if (count == 1) break
            }
        }
        return count
    }

    private fun handle(text: String) {
        try {
            val message = JSONArray(text)
            if (message.optString(0) != "EVENT") return
            val event = NostrEvent.fromJson(message.getJSONObject(2)) ?: return
            if (event.kind != KIND || event.pubkey != peer || event.tag("p") != keys.publicKeyHex) return
            if (abs(event.createdAt - System.currentTimeMillis() / 1000) > 120) return
            synchronized(seen) { if (!seen.add(event.id)) return }
            if (!event.isValid()) return
            incoming.trySend(JSONObject(Nip44.decrypt(event.content, conversationKey)))
        } catch (e: Exception) {
            // Not for us or not readable: ignore, as relays may send anything.
        }
    }

    fun send(message: JSONObject) {
        val content = Nip44.encrypt(message.toString(), conversationKey)
        val event = NostrEvent.sign(keys, KIND, listOf(listOf("p", peer)), content)
        val frame = JSONArray().put("EVENT").put(event.toJson()).toString()
        synchronized(sockets) { sockets.forEach { it.send(frame) } }
    }

    /** The next message of [session] (duplicates from several relays are dropped), or null on timeout. */
    suspend fun receive(session: String, timeoutMs: Long): JSONObject? = withTimeoutOrNull(timeoutMs) {
        while (true) {
            val message = incoming.receive()
            if (message.optString("session") == session) return@withTimeoutOrNull message
        }
        @Suppress("UNREACHABLE_CODE")
        null
    }

    override fun close() {
        synchronized(sockets) {
            sockets.forEach { it.close(1000, null) }
            sockets.clear()
        }
        client.dispatcher.executorService.shutdown()
    }

    companion object {
        const val KIND = 25050
    }
}
