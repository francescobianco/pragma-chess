package org.pragmachess.mobile.crypto

import org.json.JSONArray
import org.json.JSONObject

/** A signed Nostr event (NIP-01). */
data class NostrEvent(
    val id: String,
    val pubkey: String,
    val createdAt: Long,
    val kind: Int,
    val tags: List<List<String>>,
    val content: String,
    val sig: String,
) {
    fun toJson(): JSONObject = JSONObject()
        .put("id", id)
        .put("pubkey", pubkey)
        .put("created_at", createdAt)
        .put("kind", kind)
        .put("tags", JSONArray(tags.map { JSONArray(it) }))
        .put("content", content)
        .put("sig", sig)

    /** The id is the hash of the serialized event and the signature is the author's. */
    fun isValid(): Boolean {
        if (!Hex.isHex(id, 32) || !Hex.isHex(pubkey, 32) || !Hex.isHex(sig, 64)) return false
        val hash = computeId(pubkey, createdAt, kind, tags, content)
        return Hex.encode(hash) == id && Keys.verify(Hex.decode(sig), hash, Hex.decode(pubkey))
    }

    fun tag(name: String): String? = tags.firstOrNull { it.size >= 2 && it[0] == name }?.get(1)

    companion object {
        /** NIP-01 serialization: [0,pubkey,created_at,kind,tags,content], minimal JSON escaping. */
        fun serialize(pubkey: String, createdAt: Long, kind: Int, tags: List<List<String>>, content: String): String =
            buildString {
                append("[0,\"").append(pubkey).append("\",").append(createdAt).append(',').append(kind).append(",[")
                tags.forEachIndexed { i, tag ->
                    if (i > 0) append(',')
                    append('[')
                    tag.forEachIndexed { j, value ->
                        if (j > 0) append(',')
                        appendString(value)
                    }
                    append(']')
                }
                append("],")
                appendString(content)
                append(']')
            }

        private fun StringBuilder.appendString(value: String) {
            append('"')
            for (c in value) {
                when (c) {
                    '"' -> append("\\\"")
                    '\\' -> append("\\\\")
                    '\n' -> append("\\n")
                    '\r' -> append("\\r")
                    '\t' -> append("\\t")
                    '\b' -> append("\\b")
                    '\u000c' -> append("\\f")
                    else -> if (c < ' ') append("\\u%04x".format(c.code)) else append(c)
                }
            }
            append('"')
        }

        fun computeId(pubkey: String, createdAt: Long, kind: Int, tags: List<List<String>>, content: String): ByteArray =
            Keys.sha256(serialize(pubkey, createdAt, kind, tags, content).toByteArray(Charsets.UTF_8))

        fun sign(keys: KeyPair, kind: Int, tags: List<List<String>>, content: String,
                 createdAt: Long = System.currentTimeMillis() / 1000): NostrEvent {
            val pubkey = keys.publicKeyHex
            val hash = computeId(pubkey, createdAt, kind, tags, content)
            return NostrEvent(Hex.encode(hash), pubkey, createdAt, kind, tags, content, Hex.encode(keys.sign(hash)))
        }

        fun fromJson(json: JSONObject): NostrEvent? = try {
            val tagsJson = json.getJSONArray("tags")
            val tags = (0 until tagsJson.length()).map { i ->
                val tag = tagsJson.getJSONArray(i)
                (0 until tag.length()).map { tag.getString(it) }
            }
            NostrEvent(
                json.getString("id"), json.getString("pubkey"), json.getLong("created_at"),
                json.getInt("kind"), tags, json.getString("content"), json.getString("sig"),
            )
        } catch (e: Exception) {
            null
        }
    }
}
