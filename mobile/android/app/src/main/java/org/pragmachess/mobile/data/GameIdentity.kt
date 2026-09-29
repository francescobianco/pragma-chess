package org.pragmachess.mobile.data

import java.nio.ByteBuffer
import java.security.MessageDigest
import java.time.Instant
import java.util.UUID

/**
 * Universal ids of games and databases, the same on every device
 * (docs/phone-link.md, "One corpus: reconciliation"); the desktop's
 * GameIdentity computes exactly the same values.
 */
object GameIdentity {
    /** Namespace of the content-derived (version 5) game uids. */
    val GAME_NAMESPACE: UUID = UUID.fromString("7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c00")

    /** Lineage ids of the databases Pragma Chess ships, by their file name. */
    const val OPENING_NAMES_LINEAGE = "7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c01"
    const val CLASSIC_GAMES_LINEAGE = "7b0c5a1e-3f0d-4a55-9e3c-6f1d0a9b4c02"
    val SHIPPED = mapOf("Opening Names.pdb" to OPENING_NAMES_LINEAGE, "Classic Games.pdb" to CLASSIC_GAMES_LINEAGE)

    /** white, black, event, site, date, round, result, start FEN and UCI moves, trimmed, one per line. */
    fun content(game: GameRecord): String {
        val h = game.headers
        val uci = game.movesUci.split(' ').map { it.trim() }.filter { it.isNotEmpty() }.joinToString(" ")
        return listOf(h.white, h.black, h.event, h.site, h.date, h.round, h.result, game.startFen)
            .joinToString("\n") { it.trim() } + "\n" + uci
    }

    /** The uid of a game created with this content; [occurrence] > 1 tells identical games of one database apart. */
    fun uid(game: GameRecord, occurrence: Int = 1): String {
        val name = content(game) + if (occurrence > 1) "\n#$occurrence" else ""
        return uuidV5(GAME_NAMESPACE, name.toByteArray(Charsets.UTF_8))
    }

    /** RFC 4122 version 5 (SHA-1), lowercase with dashes. */
    fun uuidV5(namespace: UUID, name: ByteArray): String {
        val sha1 = MessageDigest.getInstance("SHA-1")
        sha1.update(ByteBuffer.allocate(16).putLong(namespace.mostSignificantBits).putLong(namespace.leastSignificantBits).array())
        sha1.update(name)
        val hash = sha1.digest()
        hash[6] = ((hash[6].toInt() and 0x0f) or 0x50).toByte()
        hash[8] = ((hash[8].toInt() and 0x3f) or 0x80).toByte()
        val buffer = ByteBuffer.wrap(hash, 0, 16)
        return UUID(buffer.long, buffer.long).toString()
    }

    fun newLineageId(): String = UUID.randomUUID().toString()

    /** Now, as stored in `modified`: ISO 8601 UTC with milliseconds. */
    fun now(): String = STAMP.format(Instant.now())

    // Always three decimals, like Qt's ISODateWithMs, so text order is time order.
    private val STAMP = java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'")
        .withZone(java.time.ZoneOffset.UTC)

    /** Two versions of one game say the same thing: the uid content plus ECO and ratings. */
    fun sameContent(a: GameRecord, b: GameRecord): Boolean =
        content(a) == content(b) && a.headers.eco.trim() == b.headers.eco.trim() &&
            a.headers.whiteElo == b.headers.whiteElo && a.headers.blackElo == b.headers.blackElo
}
