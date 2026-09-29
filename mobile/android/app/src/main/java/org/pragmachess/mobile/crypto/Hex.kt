package org.pragmachess.mobile.crypto

object Hex {
    fun encode(bytes: ByteArray): String {
        val digits = "0123456789abcdef"
        val text = CharArray(bytes.size * 2)
        for ((i, b) in bytes.withIndex()) {
            text[2 * i] = digits[(b.toInt() shr 4) and 15]
            text[2 * i + 1] = digits[b.toInt() and 15]
        }
        return String(text)
    }

    fun decode(text: String): ByteArray {
        require(text.length % 2 == 0) { "odd hex length" }
        return ByteArray(text.length / 2) { i ->
            val high = Character.digit(text[2 * i], 16)
            val low = Character.digit(text[2 * i + 1], 16)
            require(high >= 0 && low >= 0) { "not hex" }
            ((high shl 4) or low).toByte()
        }
    }

    fun isHex(text: String, bytes: Int): Boolean =
        text.length == bytes * 2 && text.all { Character.digit(it, 16) >= 0 }
}
