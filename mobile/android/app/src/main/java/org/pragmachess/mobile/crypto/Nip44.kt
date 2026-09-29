package org.pragmachess.mobile.crypto

import java.security.MessageDigest
import java.util.Base64
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/** NIP-44 version 2: the encryption of the signaling messages (see docs/phone-link.md). */
object Nip44 {
    class DecryptionException(message: String) : Exception(message)

    private const val VERSION: Byte = 2
    private const val MIN_PLAINTEXT = 1
    private const val MAX_PLAINTEXT = 65535

    fun hmac(key: ByteArray, vararg data: ByteArray): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(key, "HmacSHA256"))
        for (part in data) mac.update(part)
        return mac.doFinal()
    }

    /** HKDF-SHA256 expand of RFC 5869. */
    private fun hkdfExpand(prk: ByteArray, info: ByteArray, length: Int): ByteArray {
        val output = ByteArray(length)
        var previous = ByteArray(0)
        var offset = 0
        var counter = 1
        while (offset < length) {
            previous = hmac(prk, previous, info, byteArrayOf(counter.toByte()))
            val count = minOf(previous.size, length - offset)
            previous.copyInto(output, offset, 0, count)
            offset += count
            counter++
        }
        return output
    }

    /** The key two parties share: HKDF-extract of the ECDH x coordinate with salt "nip44-v2". */
    fun conversationKey(secret: ByteArray, publicKey: ByteArray): ByteArray =
        hmac("nip44-v2".toByteArray(), Keys.sharedX(secret, publicKey))

    data class MessageKeys(val chachaKey: ByteArray, val chachaNonce: ByteArray, val hmacKey: ByteArray)

    fun messageKeys(conversationKey: ByteArray, nonce: ByteArray): MessageKeys {
        require(conversationKey.size == 32 && nonce.size == 32)
        val keys = hkdfExpand(conversationKey, nonce, 76)
        return MessageKeys(keys.copyOfRange(0, 32), keys.copyOfRange(32, 44), keys.copyOfRange(44, 76))
    }

    fun paddedLength(length: Int): Int {
        require(length >= 1)
        if (length <= 32) return 32
        val nextPower = 1 shl (32 - Integer.numberOfLeadingZeros(length - 1))
        val chunk = if (nextPower <= 256) 32 else nextPower / 8
        return chunk * ((length - 1) / chunk + 1)
    }

    private fun pad(plaintext: ByteArray): ByteArray {
        require(plaintext.size in MIN_PLAINTEXT..MAX_PLAINTEXT) { "invalid plaintext length" }
        val padded = ByteArray(2 + paddedLength(plaintext.size))
        padded[0] = (plaintext.size shr 8).toByte()
        padded[1] = plaintext.size.toByte()
        plaintext.copyInto(padded, 2)
        return padded
    }

    private fun unpad(padded: ByteArray): ByteArray {
        if (padded.size < 2) throw DecryptionException("invalid padding")
        val length = ((padded[0].toInt() and 0xff) shl 8) or (padded[1].toInt() and 0xff)
        if (length < MIN_PLAINTEXT || 2 + length > padded.size || padded.size != 2 + paddedLength(length))
            throw DecryptionException("invalid padding")
        return padded.copyOfRange(2, 2 + length)
    }

    fun encrypt(plaintext: String, conversationKey: ByteArray, nonce: ByteArray = Keys.random(32)): String {
        val keys = messageKeys(conversationKey, nonce)
        val ciphertext = ChaCha20.xor(keys.chachaKey, keys.chachaNonce, pad(plaintext.toByteArray(Charsets.UTF_8)))
        val mac = hmac(keys.hmacKey, nonce, ciphertext)
        return Base64.getEncoder().encodeToString(byteArrayOf(VERSION) + nonce + ciphertext + mac)
    }

    fun decrypt(payload: String, conversationKey: ByteArray): String {
        if (payload.isEmpty() || payload[0] == '#') throw DecryptionException("unknown version")
        if (payload.length !in 132..87472) throw DecryptionException("invalid payload size")
        val data = try {
            Base64.getDecoder().decode(payload)
        } catch (e: IllegalArgumentException) {
            throw DecryptionException("invalid base64")
        }
        if (data.size !in 99..65603) throw DecryptionException("invalid data size")
        if (data[0] != VERSION) throw DecryptionException("unknown version ${data[0]}")
        val nonce = data.copyOfRange(1, 33)
        val ciphertext = data.copyOfRange(33, data.size - 32)
        val mac = data.copyOfRange(data.size - 32, data.size)
        val keys = messageKeys(conversationKey, nonce)
        if (!MessageDigest.isEqual(mac, hmac(keys.hmacKey, nonce, ciphertext)))
            throw DecryptionException("invalid MAC")
        val padded = ChaCha20.xor(keys.chachaKey, keys.chachaNonce, ciphertext)
        return String(unpad(padded), Charsets.UTF_8)
    }
}
