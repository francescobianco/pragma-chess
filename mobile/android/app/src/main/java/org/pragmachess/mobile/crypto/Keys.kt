package org.pragmachess.mobile.crypto

import fr.acinq.secp256k1.Secp256k1
import java.security.MessageDigest
import java.security.SecureRandom

/** A Nostr key pair: a secp256k1 secret and its BIP-340 x-only public key. */
class KeyPair(val secret: ByteArray) {
    init {
        require(secret.size == 32 && Secp256k1.secKeyVerify(secret)) { "invalid secret key" }
    }

    val publicKey: ByteArray = Keys.xOnly(Secp256k1.pubkeyCreate(secret))
    val publicKeyHex: String get() = Hex.encode(publicKey)

    /** BIP-340 signature of a 32-byte message. */
    fun sign(message32: ByteArray): ByteArray = Secp256k1.signSchnorr(message32, secret, Keys.random(32))

    companion object {
        fun generate(): KeyPair {
            while (true) {
                val candidate = Keys.random(32)
                if (Secp256k1.secKeyVerify(candidate)) return KeyPair(candidate)
            }
        }
    }
}

object Keys {
    private val random = SecureRandom()

    fun random(size: Int): ByteArray = ByteArray(size).also { random.nextBytes(it) }

    fun sha256(data: ByteArray): ByteArray = MessageDigest.getInstance("SHA-256").digest(data)

    /** X coordinate of a serialized public key (33 or 65 bytes). */
    internal fun xOnly(publicKey: ByteArray): ByteArray = publicKey.copyOfRange(1, 33)

    fun verify(signature: ByteArray, message32: ByteArray, publicKey: ByteArray): Boolean =
        try {
            signature.size == 64 && publicKey.size == 32 && Secp256k1.verifySchnorr(signature, message32, publicKey)
        } catch (e: Exception) {
            false
        }

    /**
     * The x coordinate of secret × point(publicKey), the shared secret of NIP-44
     * (unhashed, unlike libsecp256k1's default ECDH). The x-only key is lifted
     * to the point with an even y.
     */
    fun sharedX(secret: ByteArray, publicKey: ByteArray): ByteArray {
        require(publicKey.size == 32) { "public key must be 32 bytes" }
        require(Secp256k1.secKeyVerify(secret)) { "invalid secret key" }
        val point = Secp256k1.pubkeyParse(byteArrayOf(2) + publicKey)
        return xOnly(Secp256k1.pubKeyTweakMul(point, secret))
    }
}
