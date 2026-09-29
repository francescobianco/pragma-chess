package org.pragmachess.mobile.link

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.pragmachess.mobile.crypto.KeyPair

/**
 * Two parties exchanging an encrypted kind-25050 message through a real
 * public relay. Needs the network: run with PRAGMA_RELAY_TEST=1.
 */
class SignalingRelayTest {
    @Test
    fun exchangesMessagesThroughAPublicRelay() = runBlocking {
        assumeTrue(System.getenv("PRAGMA_RELAY_TEST") == "1")
        val phone = KeyPair.generate()
        val computer = KeyPair.generate()
        val relays = listOf(System.getenv("PRAGMA_RELAY") ?: "wss://relay.damus.io")
        Signaling(phone, computer.publicKeyHex, relays).use { phoneSide ->
            Signaling(computer, phone.publicKeyHex, relays).use { computerSide ->
                assertEquals(1, phoneSide.open())
                assertEquals(1, computerSide.open())
                Thread.sleep(1500)
                phoneSide.send(JSONObject().put("t", "offer").put("session", "s1").put("sdp", "v=0 fake"))
                val offer = computerSide.receive("s1", 15000)
                assertNotNull("the offer never arrived", offer)
                assertEquals("v=0 fake", offer!!.getString("sdp"))
                computerSide.send(JSONObject().put("t", "answer").put("session", "s1").put("sdp", "v=0 answer"))
                val answer = phoneSide.receive("s1", 15000)
                assertEquals("v=0 answer", answer!!.getString("sdp"))
            }
        }
    }
}
