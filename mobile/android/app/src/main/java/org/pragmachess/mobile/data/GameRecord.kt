package org.pragmachess.mobile.data

import org.json.JSONObject

/** The headers of a game, as stored in a .pdb and sent with "put". */
data class GameHeaders(
    val white: String = "",
    val black: String = "",
    val event: String = "",
    val site: String = "",
    val date: String = "",
    val round: String = "",
    val result: String = "*",
    val whiteElo: Int = 0,
    val blackElo: Int = 0,
    val eco: String = "",
)

/**
 * A whole game: headers, starting position and main line. [uid] is its
 * universal id (schema version 5, see [GameIdentity]); [modified] when it was
 * created or last changed (ISO 8601 UTC, empty = never).
 */
data class GameRecord(
    val headers: GameHeaders,
    val startFen: String = "",
    val movesSan: String = "",
    val movesUci: String = "",
    val uid: String? = null,
    val modified: String = "",
    /**
     * The desktop's columns the phone does not read — the variations, the
     * other PGN tags, the comments — carried as they are, so that a game
     * merged here keeps them; null when not loaded, and then not written.
     */
    val variations: String? = null,
    val tags: String? = null,
    val comments: String? = null,
) {
    val plyCount: Int get() = movesUci.split(' ').count { it.isNotBlank() }

    /** The game object of the file protocol (docs/phone-link.md). */
    fun toJson(): JSONObject = JSONObject()
        .put("uid", uid ?: "")
        .put("modified", modified)
        .put("white", headers.white)
        .put("black", headers.black)
        .put("event", headers.event)
        .put("site", headers.site)
        .put("date", headers.date)
        .put("round", headers.round)
        .put("result", headers.result)
        .put("white_elo", headers.whiteElo)
        .put("black_elo", headers.blackElo)
        .put("eco", headers.eco)
        .put("start_fen", startFen)
        .put("moves_san", movesSan)
        .put("moves_uci", movesUci)
        .apply {
            variations?.let { put("variations", it) }
            tags?.let { put("tags", it) }
            comments?.let { put("comments", it) }
        }

    companion object {
        val RESULTS = listOf("*", "1-0", "0-1", "1/2-1/2")

        fun fromJson(json: JSONObject) = GameRecord(
            GameHeaders(
                white = json.optString("white"),
                black = json.optString("black"),
                event = json.optString("event"),
                site = json.optString("site"),
                date = json.optString("date"),
                round = json.optString("round"),
                result = json.optString("result", "*").ifEmpty { "*" },
                whiteElo = json.optInt("white_elo"),
                blackElo = json.optInt("black_elo"),
                eco = json.optString("eco"),
            ),
            startFen = json.optString("start_fen"),
            movesSan = json.optString("moves_san"),
            movesUci = json.optString("moves_uci"),
            uid = json.optString("uid").ifEmpty { null },
            modified = json.optString("modified"),
            variations = if (json.has("variations")) json.optString("variations") else null,
            tags = if (json.has("tags")) json.optString("tags") else null,
            comments = if (json.has("comments")) json.optString("comments") else null,
        )
    }
}

/** One row of a games list. */
data class GameSummary(
    val id: Long,
    val white: String,
    val black: String,
    val result: String,
    val date: String,
    val event: String,
    val plyCount: Int,
)
