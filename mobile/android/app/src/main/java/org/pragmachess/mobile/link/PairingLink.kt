package org.pragmachess.mobile.link

import org.pragmachess.mobile.crypto.Hex
import java.net.URI
import java.net.URLDecoder

/** The pairing link a computer shows as a QR code (docs/phone-link.md). */
data class PairingLink(val pubkey: String, val secret: String, val name: String, val relays: List<String>) {
    companion object {
        val DEFAULT_RELAYS = listOf("wss://relay.damus.io", "wss://nos.lol", "wss://relay.primal.net")

        /** Reads `pragma-chess://pair?k=…&s=…&n=…&r=…`; null when it is not a valid pairing link. */
        fun parse(text: String): PairingLink? {
            val uri = try {
                URI(text.trim())
            } catch (e: Exception) {
                return null
            }
            if (uri.scheme != "pragma-chess" || uri.host != "pair") return null
            val params = (uri.rawQuery ?: return null).split('&').mapNotNull { part ->
                val eq = part.indexOf('=')
                if (eq <= 0) null
                else part.substring(0, eq) to URLDecoder.decode(part.substring(eq + 1), "UTF-8")
            }
            fun first(key: String) = params.firstOrNull { it.first == key }?.second
            val pubkey = first("k")?.lowercase() ?: return null
            val secret = first("s")?.lowercase() ?: return null
            if (!Hex.isHex(pubkey, 32) || !Hex.isHex(secret, 32)) return null
            val relays = params.filter { it.first == "r" }.map { it.second }
                .filter { it.startsWith("wss://") || it.startsWith("ws://") }
            return PairingLink(pubkey, secret, first("n")?.ifBlank { null } ?: "Pragma Chess",
                relays.ifEmpty { DEFAULT_RELAYS })
        }
    }
}
