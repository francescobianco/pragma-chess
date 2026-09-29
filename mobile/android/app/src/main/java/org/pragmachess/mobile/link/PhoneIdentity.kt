package org.pragmachess.mobile.link

import android.content.Context
import android.os.Build
import org.pragmachess.mobile.crypto.Hex
import org.pragmachess.mobile.crypto.KeyPair
import org.pragmachess.mobile.data.PhoneSource

/** The phone's Nostr key pair and name, kept in app-private preferences. */
class PhoneIdentity(context: Context) {
    private val prefs = context.getSharedPreferences("identity", Context.MODE_PRIVATE)

    val keys: KeyPair = prefs.getString("secret", null)?.let { KeyPair(Hex.decode(it)) }
        ?: KeyPair.generate().also { prefs.edit().putString("secret", Hex.encode(it.secret)).apply() }

    var name: String
        get() = prefs.getString("name", null) ?: defaultName()
        set(value) = prefs.edit().putString("name", value.trim().ifEmpty { defaultName() }).apply()

    val source: PhoneSource get() = PhoneSource(keys.publicKeyHex, name)

    private fun defaultName(): String {
        val model = Build.MODEL.orEmpty()
        val maker = Build.MANUFACTURER.orEmpty().replaceFirstChar { it.uppercase() }
        return if (model.startsWith(maker, ignoreCase = true)) model else "$maker $model".trim().ifEmpty { "Android" }
    }
}
