package org.pragmachess.mobile.link

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PairingLinkTest {
    private val key = "ab".repeat(32)
    private val secret = "cd".repeat(32)

    @Test
    fun readsTheDesktopLink() {
        val link = PairingLink.parse(
            "pragma-chess://pair?k=$key&s=$secret&n=Studio%20di%20Francesco&r=wss%3A%2F%2Frelay.damus.io&r=wss://nos.lol"
        )!!
        assertEquals(key, link.pubkey)
        assertEquals(secret, link.secret)
        assertEquals("Studio di Francesco", link.name)
        assertEquals(listOf("wss://relay.damus.io", "wss://nos.lol"), link.relays)
    }

    @Test
    fun fallsBackToDefaultRelaysAndRejectsBadLinks() {
        assertEquals(PairingLink.DEFAULT_RELAYS, PairingLink.parse("pragma-chess://pair?k=$key&s=$secret")!!.relays)
        assertNull(PairingLink.parse("https://pair?k=$key&s=$secret"))
        assertNull(PairingLink.parse("pragma-chess://pair?k=abc&s=$secret"))
        assertNull(PairingLink.parse("pragma-chess://pair?k=$key"))
        assertNull(PairingLink.parse("not a link"))
    }

    @Test
    fun remoteNamesStayInsideTheFolder() {
        assertTrue(ComputerSync.isSafeName("Classic Games.pdb"))
        assertTrue(ComputerSync.isSafeName("Club/2026.pdb"))
        assertFalse(ComputerSync.isSafeName("../evil.pdb"))
        assertFalse(ComputerSync.isSafeName("/etc/x.pdb"))
        assertFalse(ComputerSync.isSafeName("notes.txt"))
        assertFalse(ComputerSync.isSafeName("a//b.pdb"))
    }
}
