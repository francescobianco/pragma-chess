package org.pragmachess.mobile.crypto

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

/** Against the official vectors (github.com/paulmillr/nip44, a subset in test resources). */
class Nip44Test {
    private val vectors: JSONObject = JSONObject(
        javaClass.classLoader!!.getResourceAsStream("nip44.vectors.json")!!.readBytes().toString(Charsets.UTF_8)
    ).getJSONObject("v2")
    private val valid = vectors.getJSONObject("valid")
    private val invalid = vectors.getJSONObject("invalid")

    private fun hex(o: JSONObject, key: String) = Hex.decode(o.getString(key))

    @Test
    fun conversationKeys() {
        val cases = valid.getJSONArray("get_conversation_key")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            assertEquals(c.getString("conversation_key"),
                Hex.encode(Nip44.conversationKey(hex(c, "sec1"), hex(c, "pub2"))))
        }
    }

    @Test
    fun invalidConversationKeys() {
        val cases = invalid.getJSONArray("get_conversation_key")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            try {
                Nip44.conversationKey(hex(c, "sec1"), hex(c, "pub2"))
                fail("accepted: ${c.optString("note")}")
            } catch (expected: Exception) {
            }
        }
    }

    @Test
    fun messageKeys() {
        val group = valid.getJSONObject("get_message_keys")
        val conversationKey = hex(group, "conversation_key")
        val cases = group.getJSONArray("keys")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            val keys = Nip44.messageKeys(conversationKey, hex(c, "nonce"))
            assertEquals(c.getString("chacha_key"), Hex.encode(keys.chachaKey))
            assertEquals(c.getString("chacha_nonce"), Hex.encode(keys.chachaNonce))
            assertEquals(c.getString("hmac_key"), Hex.encode(keys.hmacKey))
        }
    }

    @Test
    fun paddedLengths() {
        val cases = valid.getJSONArray("calc_padded_len")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONArray(i)
            assertEquals(c.getInt(1), Nip44.paddedLength(c.getInt(0)))
        }
    }

    @Test
    fun encryptsAndDecrypts() {
        val cases = valid.getJSONArray("encrypt_decrypt")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            val conversationKey = Nip44.conversationKey(hex(c, "sec1"),
                KeyPair(hex(c, "sec2")).publicKey)
            assertEquals(c.getString("conversation_key"), Hex.encode(conversationKey))
            val payload = Nip44.encrypt(c.getString("plaintext"), conversationKey, hex(c, "nonce"))
            assertEquals(c.getString("payload"), payload)
            assertEquals(c.getString("plaintext"), Nip44.decrypt(payload, conversationKey))
        }
    }

    @Test
    fun longMessages() {
        val cases = valid.getJSONArray("encrypt_decrypt_long_msg")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            val plaintext = c.getString("pattern").repeat(c.getInt("repeat"))
            assertEquals(c.getString("plaintext_sha256"), Hex.encode(Keys.sha256(plaintext.toByteArray())))
            val payload = Nip44.encrypt(plaintext, hex(c, "conversation_key"), hex(c, "nonce"))
            assertEquals(c.getString("payload_sha256"), Hex.encode(Keys.sha256(payload.toByteArray())))
            assertEquals(plaintext, Nip44.decrypt(payload, hex(c, "conversation_key")))
        }
    }

    @Test
    fun rejectsInvalidPayloads() {
        val cases = invalid.getJSONArray("decrypt")
        for (i in 0 until cases.length()) {
            val c = cases.getJSONObject(i)
            try {
                Nip44.decrypt(c.getString("payload"), hex(c, "conversation_key"))
                fail("accepted: ${c.optString("note")}")
            } catch (expected: Nip44.DecryptionException) {
            }
        }
    }

    @Test
    fun rejectsInvalidMessageLengths() {
        val cases = invalid.getJSONArray("encrypt_msg_lengths")
        for (i in 0 until cases.length()) {
            val length = cases.getInt(i)
            try {
                Nip44.encrypt("x".repeat(length), Keys.random(32))
                fail("accepted length $length")
            } catch (expected: IllegalArgumentException) {
            }
        }
    }

    @Test
    fun signsAndVerifiesEvents() {
        val keys = KeyPair.generate()
        val event = NostrEvent.sign(keys, 25050, listOf(listOf("p", "ab".repeat(32))), "hello \"world\"\n")
        assertTrue(event.isValid())
        val parsed = NostrEvent.fromJson(JSONObject(event.toJson().toString()))!!
        assertTrue(parsed.isValid())
        assertEquals("ab".repeat(32), parsed.tag("p"))
        assertFalse(event.copy(content = "tampered").isValid())
        assertFalse(event.copy(pubkey = KeyPair.generate().publicKeyHex).isValid())
    }

    @Test
    fun eventIdMatchesKnownSerialization() {
        // NIP-01 serialization of a fixed event, hashed with sha256 by hand.
        val text = NostrEvent.serialize("aa".repeat(32), 1700000000, 1, listOf(listOf("t", "x")), "a\"b\\c\n")
        assertEquals("[0,\"${"aa".repeat(32)}\",1700000000,1,[[\"t\",\"x\"]],\"a\\\"b\\\\c\\n\"]", text)
    }

    @Test
    fun twoPartiesShareTheConversationKey() {
        val a = KeyPair.generate()
        val b = KeyPair.generate()
        val ab = Nip44.conversationKey(a.secret, b.publicKey)
        assertEquals(Hex.encode(ab), Hex.encode(Nip44.conversationKey(b.secret, a.publicKey)))
        assertEquals("ciao", Nip44.decrypt(Nip44.encrypt("ciao", ab), ab))
    }
}
